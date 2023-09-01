#include <mgba-util/mobile.h>

// Non-portable error checking
// TODO: should this go in socket.h? x_ Wit-MKW
#ifdef _WIN32
#define _SOCKERR(x) WSA ## x
#else
#define _SOCKERR(x) x
#endif

static void serial_disable(void* user) {
	struct MobileAdapterGB* mobile = user;

	mobile->serial = 0;
}

static void serial_enable(void* user, bool mode_32bit) {
	struct MobileAdapterGB* mobile = user;

	mobile->serial = mode_32bit ? 4 : 1;
}

static bool config_read(void* user, void* dest, uintptr_t offset, size_t size) {
	struct MobileAdapterGB* mobile = user;

	return memcpy(dest, mobile->config + offset, size) == dest;
}

static bool config_write(void* user, const void* src, uintptr_t offset, size_t size) {
	struct MobileAdapterGB* mobile = user;

	return memcpy(mobile->config + offset, src, size) == mobile->config + offset;
}

static bool sock_open(void* user, unsigned conn, enum mobile_socktype type, enum mobile_addrtype addrtype, unsigned bindport) {
	struct MobileAdapterGB* mobile = user;

	Socket fd;
	mobile->socket[conn].socktype = type;
	if (type != MOBILE_SOCKTYPE_UDP) {
		fd = INVALID_SOCKET;
		mobile->socket[conn].addrtype = addrtype;
		mobile->socket[conn].bindport = bindport;
	} else {
		struct Address bindaddr, *bindptr = NULL;
		if (addrtype == MOBILE_ADDRTYPE_IPV6) {
			bindaddr.version = IPV6;
			memset(&bindaddr.ipv6, 0, sizeof(bindaddr.ipv6));
			bindptr = &bindaddr;
		}
		fd = SocketOpenUDP(bindport, bindptr);
		if (!SOCKET_FAILED(fd)) {
			SocketSetBlocking(fd, false);
		}
	}
	return !SOCKET_FAILED(mobile->socket[conn].fd = fd) || type != MOBILE_SOCKTYPE_UDP;
}

static void sock_close(void* user, unsigned conn) {
	struct MobileAdapterGB* mobile = user;

	if (!SOCKET_FAILED(mobile->socket[conn].fd)) {
		SocketClose(mobile->socket[conn].fd);
	}
	mobile->socket[conn].fd = INVALID_SOCKET;
	mobile->socket[conn].socktype = (enum mobile_socktype) 0;
	mobile->socket[conn].addrtype = MOBILE_ADDRTYPE_NONE;
	mobile->socket[conn].bindport = 0;
}

static int sock_connect(void* user, unsigned conn, const struct mobile_addr* addr) {
	struct MobileAdapterGB* mobile = user;

	Socket fd = mobile->socket[conn].fd;
	if (SOCKET_FAILED(fd)) {
		fd = SocketCreate(addr->type == MOBILE_ADDRTYPE_IPV6, IPPROTO_TCP);
		if (SOCKET_FAILED(fd)) return -1;
		if (!SocketSetBlocking(fd, false)) return -1;
		mobile->socket[conn].fd = fd;
	}

	struct Address connaddr;
	int connport;
	if (addr->type == MOBILE_ADDRTYPE_IPV6) {
		const struct mobile_addr6* addr6 = (struct mobile_addr6*) addr;
		connaddr.version = IPV6;
		memcpy(&connaddr.ipv6, addr6->host, MOBILE_HOSTLEN_IPV6);
		connport = addr6->port;
	} else {
		const struct mobile_addr4* addr4 = (struct mobile_addr4*) addr;
		connaddr.version = IPV4;
		connaddr.ipv4 = ntohl(*(uint32_t*) &addr4->host);
		connport = addr4->port;
	}

	int rc = SocketConnect(fd, connport, &connaddr);
	if (!rc) return 1;

	int err = SocketError();
	if (err == _SOCKERR(EWOULDBLOCK) ||
			err == _SOCKERR(EINPROGRESS) ||
			err == _SOCKERR(EALREADY)) {
		return 0;
	}
	if (err == _SOCKERR(EISCONN)) {
		return 1;
	}

	return -1;
}

static bool sock_listen(void* user, unsigned conn) {
	struct MobileAdapterGB* mobile = user;

	if (SOCKET_FAILED(mobile->socket[conn].fd)) {
		Socket fd;
		struct Address bindaddr, *bindptr = NULL;
		if (mobile->socket[conn].addrtype == MOBILE_ADDRTYPE_IPV6) {
			bindaddr.version = IPV6;
			memset(&bindaddr.ipv6, 0, sizeof(bindaddr.ipv6));
			bindptr = &bindaddr;
		}
		fd = SocketOpenTCP(mobile->socket[conn].bindport, bindptr);
		if (!SOCKET_FAILED(fd)) {
			SocketSetBlocking(fd, false);
		}
		mobile->socket[conn].fd = fd;
	}
	return !SOCKET_FAILED(SocketListen(mobile->socket[conn].fd, 1));
}

static bool sock_accept(void* user, unsigned conn) {
	struct MobileAdapterGB* mobile = user;

	Socket fd = SocketAccept(mobile->socket[conn].fd, NULL);
	if (SOCKET_FAILED(fd)) return false;
	SocketSetBlocking(fd, false);
	sock_close(user, conn);
	mobile->socket[conn].fd = fd;
	return true;
}

static int sock_send(void* user, unsigned conn, const void* data, unsigned size, const struct mobile_addr* addr) {
	struct MobileAdapterGB* mobile = user;

	if (addr) {
		struct Address destaddr;
		int destport;
		if (addr->type == MOBILE_ADDRTYPE_IPV6) {
			const struct mobile_addr6* addr6 = (struct mobile_addr6*) addr;
			destaddr.version = IPV6;
			memcpy(&destaddr.ipv6, addr6->host, MOBILE_HOSTLEN_IPV6);
			destport = addr6->port;
		} else {
			const struct mobile_addr4* addr4 = (struct mobile_addr4*) addr;
			destaddr.version = IPV4;
			destaddr.ipv4 = ntohl(*(uint32_t*) &addr4->host);
			destport = addr4->port;
		}
		return SocketSendTo(mobile->socket[conn].fd, data, size, destport, &destaddr);
	}
	return SocketSend(mobile->socket[conn].fd, data, size);
}

static int sock_recv(void* user, unsigned conn, void* data, unsigned size, struct mobile_addr* addr) {
	struct MobileAdapterGB* mobile = user;

	int res;
	Socket r = mobile->socket[conn].fd;
	Socket e = mobile->socket[conn].fd;
	if (SocketPoll(1, &r, NULL, &e, 0) <= 0) {
		return 0;
	}
	if (addr) {
		struct Address srcaddr;
		int srcport;
		res = (int) SocketRecvFrom(mobile->socket[conn].fd, data, size, &srcport, &srcaddr);
		if (res > 0) {
			if (srcaddr.version == IPV6) {
				struct mobile_addr6 *addr6 = (struct mobile_addr6*) addr;
				addr->type = MOBILE_ADDRTYPE_IPV6;
				memcpy(&addr6->host, &srcaddr.ipv6, MOBILE_HOSTLEN_IPV6);
				addr6->port = srcport;
			} else {
				struct mobile_addr4 *addr4 = (struct mobile_addr4*) addr;
				addr->type = MOBILE_ADDRTYPE_IPV4;
				*(uint32_t*) &addr4->host = htonl(srcaddr.ipv4);
				addr4->port = srcport;
			}
		} else if (res == -1 && SocketWouldBlock()) {
			return 0;
		}
		return (res || (mobile->socket[conn].socktype == MOBILE_SOCKTYPE_UDP)) ? res : -2;
	}
	res = (int) SocketRecv(mobile->socket[conn].fd, data, size);
	if (res == -1 && SocketWouldBlock()) {
		return 0;
	}
	return (res || (mobile->socket[conn].socktype == MOBILE_SOCKTYPE_UDP)) ? res : -2;
}

static void update_number(void* user, enum mobile_number type, const char* number) {
	struct MobileAdapterGB* mobile = user;

	char* dest = mobile->number[type];
	if (number) {
		strncpy(dest, number, MOBILE_MAX_NUMBER_SIZE);
		dest[MOBILE_MAX_NUMBER_SIZE] = '\0';
	} else {
		dest[0] = '\0';
	}
}

struct mobile_adapter* MobileAdapterGBNew(struct MobileAdapterGB *mobile) {
	struct mobile_adapter* adapter = mobile_new(mobile);
	if (!adapter) return NULL;

	mobile_def_serial_disable(adapter, serial_disable);
	mobile_def_serial_enable(adapter, serial_enable);
	mobile_def_config_read(adapter, config_read);
	mobile_def_config_write(adapter, config_write);
	mobile_def_sock_open(adapter, sock_open);
	mobile_def_sock_close(adapter, sock_close);
	mobile_def_sock_connect(adapter, sock_connect);
	mobile_def_sock_listen(adapter, sock_listen);
	mobile_def_sock_accept(adapter, sock_accept);
	mobile_def_sock_send(adapter, sock_send);
	mobile_def_sock_recv(adapter, sock_recv);
	mobile_def_update_number(adapter, update_number);

	return adapter;
}
