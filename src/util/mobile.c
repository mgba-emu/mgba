#include <mgba-util/mobile.h>

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

	memcpy(dest, mobile->config + offset, size);
	return true;
}

static bool config_write(void* user, const void* src, uintptr_t offset, size_t size) {
	struct MobileAdapterGB* mobile = user;

	memcpy(mobile->config + offset, src, size);
	return true;
}

static bool sock_open(void* user, unsigned conn, enum mobile_socktype type, enum mobile_addrtype addrtype, unsigned bindport) {
	struct MobileAdapterGB* mobile = user;

	Socket fd;

	mobile->socket[conn].socktype = type;

	struct Address bindaddr = {0};
	if (addrtype != MOBILE_ADDRTYPE_IPV6) {
		bindaddr.version = IPV4;
	} else {
		bindaddr.version = IPV6;
	}

	if (type != MOBILE_SOCKTYPE_UDP) {
		fd = SocketOpenTCP(bindport, &bindaddr);
	} else {
		fd = SocketOpenUDP(bindport, &bindaddr);
	}
	if (!SOCKET_FAILED(fd)) {
		SocketSetBlocking(fd, false);
	}

	mobile->socket[conn].fd = fd;
	return !SOCKET_FAILED(fd);
}

static void sock_close(void* user, unsigned conn) {
	struct MobileAdapterGB* mobile = user;

	if (!SOCKET_FAILED(mobile->socket[conn].fd)) {
		SocketClose(mobile->socket[conn].fd);
	}
	mobile->socket[conn].fd = INVALID_SOCKET;
	mobile->socket[conn].socktype = 0;
}

static int sock_connect(void* user, unsigned conn, const struct mobile_addr* addr) {
	struct MobileAdapterGB* mobile = user;

	Socket fd = mobile->socket[conn].fd;

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

	// Two slightly uncommon assumptions are being made about SocketConnect:
	// (these are true under any *strict* implementation of BSD sockets)
	// - It works on a socket that has been obtained through SocketOpenTCP/UDP()
	// - It doesn't block when SocketSetBlocking() has been used
	int rc = SocketConnect(fd, connport, &connaddr);
	if (!rc || SocketIsConnected()) {
		return 1;
	}

	return SocketIsConnecting() ? 0 : -1;
}

static bool sock_listen(void* user, unsigned conn) {
	struct MobileAdapterGB* mobile = user;

	return !SOCKET_RESERROR(SocketListen(mobile->socket[conn].fd, 1));
}

static bool sock_accept(void* user, unsigned conn) {
	struct MobileAdapterGB* mobile = user;

	Socket fd = SocketAccept(mobile->socket[conn].fd, NULL);
	if (!SOCKET_FAILED(fd)) {
		SocketSetBlocking(fd, false);
		SocketClose(mobile->socket[conn].fd);
		mobile->socket[conn].fd = fd;
	}

	return !SOCKET_FAILED(fd);
}

static int sock_send(void* user, unsigned conn, const void* data, unsigned size, const struct mobile_addr* addr) {
	struct MobileAdapterGB* mobile = user;

	struct Address sendaddr;
	int destport = 0;
	struct Address* destaddr = NULL;

	if (addr) {
		if (addr->type == MOBILE_ADDRTYPE_IPV6) {
			const struct mobile_addr6* addr6 = (struct mobile_addr6*) addr;
			sendaddr.version = IPV6;
			memcpy(&sendaddr.ipv6, addr6->host, MOBILE_HOSTLEN_IPV6);
			destaddr = &sendaddr;
			destport = addr6->port;
		} else {
			const struct mobile_addr4* addr4 = (struct mobile_addr4*) addr;
			sendaddr.version = IPV4;
			sendaddr.ipv4 = ntohl(*(uint32_t*) &addr4->host);
			destaddr = &sendaddr;
			destport = addr4->port;
		}
	}

	ssize_t res = SocketSendTo(mobile->socket[conn].fd, data, size, destport, destaddr);
	return !SOCKET_RESERROR(res) ? res : -1;
}

static int sock_recv(void* user, unsigned conn, void* data, unsigned size, struct mobile_addr* addr) {
	struct MobileAdapterGB* mobile = user;

	Socket r = mobile->socket[conn].fd;
	Socket e = mobile->socket[conn].fd;
	if (SocketPoll(1, &r, NULL, &e, 0) <= 0) {
		return 0;
	}

	struct Address srcaddr;
	int srcport;
	ssize_t res = SocketRecvFrom(mobile->socket[conn].fd, data, size, &srcport, &srcaddr);
	if (SOCKET_RESERROR(res)) {
		return SocketWouldBlock() ? 0 : -1;
	}

	if (res > 0 && addr) {
		if (srcaddr.version == IPV6) {
			struct mobile_addr6 *addr6 = (struct mobile_addr6*) addr;
			addr6->type = MOBILE_ADDRTYPE_IPV6;
			memcpy(&addr6->host, &srcaddr.ipv6, MOBILE_HOSTLEN_IPV6);
			addr6->port = srcport;
		} else {
			struct mobile_addr4 *addr4 = (struct mobile_addr4*) addr;
			addr4->type = MOBILE_ADDRTYPE_IPV4;
			*(uint32_t*) &addr4->host = htonl(srcaddr.ipv4);
			addr4->port = srcport;
		}
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
