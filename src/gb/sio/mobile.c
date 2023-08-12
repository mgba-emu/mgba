#ifdef USE_LIBMOBILE

#include <mgba/internal/gb/sio/mobile.h>

#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>

mLOG_DECLARE_CATEGORY(GB_MOBILE);
mLOG_DEFINE_CATEGORY(GB_MOBILE, "Mobile Adapter (GBC)", "gb.mobile");

#define USER1 (*(struct GBMobileAdapter*) (user))
#define ADDR4 (*(struct mobile_addr4*) (addr))
#define ADDR6 (*(struct mobile_addr6*) (addr))

static void debug_log(void* user, const char* line) {
	UNUSED(user);
	mLOG(GB_MOBILE, DEBUG, "%s", line);
}

static void serial_disable(void* user) {
	USER1.serial = 0;
}

static void serial_enable(void* user, bool mode_32bit) {
	USER1.serial = mode_32bit ? 4 : 1;
}

static bool config_read(void* user, void* dest, uintptr_t offset, size_t size) {
	return memcpy(dest, USER1.config + offset, size) == dest;
}

static bool config_write(void* user, const void* src, uintptr_t offset, size_t size) {
	return memcpy(USER1.config + offset, src, size) == USER1.config + offset;
}

static void time_latch(void* user, unsigned timer) {
	USER1.timeLatch[timer] = mTimingCurrentTime(&USER1.d.p->p->timing);
}

static bool time_check_ms(void* user, unsigned timer, unsigned ms) {
	uint64_t diff = (uint64_t) mTimingCurrentTime(&USER1.d.p->p->timing) - (uint64_t) USER1.timeLatch[timer];
	return (unsigned) (diff * 1000ULL / CGB_SM83_FREQUENCY) >= ms;
}

static bool sock_open(void* user, unsigned conn, enum mobile_socktype type, enum mobile_addrtype addrtype, unsigned bindport) {
	Socket fd;
	USER1.socket[conn].socktype = type;
	if (type != MOBILE_SOCKTYPE_UDP) {
		fd = INVALID_SOCKET;
		USER1.socket[conn].addrtype = addrtype;
		USER1.socket[conn].bindport = bindport;
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
	return !SOCKET_FAILED(USER1.socket[conn].fd = fd) || type != MOBILE_SOCKTYPE_UDP;
}

static void sock_close(void* user, unsigned conn) {
	if (!SOCKET_FAILED(USER1.socket[conn].fd)) {
		SocketClose(USER1.socket[conn].fd);
	}
	USER1.socket[conn].fd = INVALID_SOCKET;
	USER1.socket[conn].socktype = (enum mobile_socktype) 0;
	USER1.socket[conn].addrtype = MOBILE_ADDRTYPE_NONE;
	USER1.socket[conn].bindport = 0;
}

static int sock_connect(void* user, unsigned conn, const struct mobile_addr* addr) {
	Socket fd = USER1.socket[conn].fd;

	if (SOCKET_FAILED(fd)) {
		fd = SocketCreate(addr->type == MOBILE_ADDRTYPE_IPV6, IPPROTO_TCP);
		if (SOCKET_FAILED(fd)) return -1;
		if (!SocketSetBlocking(fd, false)) return -1;
		USER1.socket[conn].fd = fd;
	}

	struct Address connaddr;
	int connport;
	if (addr->type == MOBILE_ADDRTYPE_IPV6) {
		connaddr.version = IPV6;
		memcpy(&connaddr.ipv6, &ADDR6.host, MOBILE_HOSTLEN_IPV6);
		connport = ADDR6.port;
	} else {
		connaddr.version = IPV4;
		connaddr.ipv4 = ntohl(*(uint32_t*) &ADDR4.host);
		connport = ADDR4.port;
	}

	int rc = SocketConnect(fd, connport, &connaddr);
	if (!rc) return 1;

	// Non-portable error checking
	// TODO: should this go in socket.h? x_ Wit-MKW
#ifdef _WIN32
#define _SOCKERR(x) WSA ## x
#else
#define _SOCKERR(x) x
#endif
	int err = SocketError();
	if (err == _SOCKERR(EWOULDBLOCK) ||
			err == _SOCKERR(EINPROGRESS) ||
			err == _SOCKERR(EALREADY)) {
		return 0;
	}
	if (err == _SOCKERR(EISCONN)) {
		return 1;
	}
#undef _SOCKERR

	return -1;
}

static bool sock_listen(void* user, unsigned conn) {
	if (SOCKET_FAILED(USER1.socket[conn].fd)) {
		Socket fd;
		struct Address bindaddr, *bindptr = NULL;
		if (USER1.socket[conn].addrtype == MOBILE_ADDRTYPE_IPV6) {
			bindaddr.version = IPV6;
			memset(&bindaddr.ipv6, 0, sizeof(bindaddr.ipv6));
			bindptr = &bindaddr;
		}
		fd = SocketOpenTCP(USER1.socket[conn].bindport, bindptr);
		if (!SOCKET_FAILED(fd)) {
			SocketSetBlocking(fd, false);
		}
		USER1.socket[conn].fd = fd;
	}
	return !SOCKET_FAILED(SocketListen(USER1.socket[conn].fd, 1));
}

static bool sock_accept(void* user, unsigned conn) {
	Socket fd = SocketAccept(USER1.socket[conn].fd, NULL);
	if (SOCKET_FAILED(fd)) return false;
	SocketSetBlocking(fd, false);
	sock_close(user, conn);
	USER1.socket[conn].fd = fd;
	return true;
}

static int sock_send(void* user, unsigned conn, const void* data, unsigned size, const struct mobile_addr* addr) {
	if (addr) {
		struct Address destaddr;
		int destport;
		if (addr->type == MOBILE_ADDRTYPE_IPV6) {
			destaddr.version = IPV6;
			memcpy(&destaddr.ipv6, &ADDR6.host, MOBILE_HOSTLEN_IPV6);
			destport = ADDR6.port;
		} else {
			destaddr.version = IPV4;
			destaddr.ipv4 = ntohl(*(uint32_t*) &ADDR4.host);
			destport = ADDR4.port;
		}
		return SocketSendTo(USER1.socket[conn].fd, data, size, destport, &destaddr);
	}
	return SocketSend(USER1.socket[conn].fd, data, size);
}

static int sock_recv(void* user, unsigned conn, void* data, unsigned size, struct mobile_addr* addr) {
	int res;
	Socket r = USER1.socket[conn].fd;
	Socket e = USER1.socket[conn].fd;
	if (SocketPoll(1, &r, NULL, &e, 0) <= 0) {
		return 0;
	}
	if (addr) {
		struct Address srcaddr;
		int srcport;
		res = (int) SocketRecvFrom(USER1.socket[conn].fd, data, size, &srcport, &srcaddr);
		if (srcaddr.version == IPV6) {
			addr->type = MOBILE_ADDRTYPE_IPV6;
			memcpy(&ADDR6.host, &srcaddr.ipv6, MOBILE_HOSTLEN_IPV6);
			ADDR6.port = srcport;
		} else {
			addr->type = MOBILE_ADDRTYPE_IPV4;
			*(uint32_t*) &ADDR4.host = htonl(srcaddr.ipv4);
			ADDR4.port = srcport;
		}
		if (res == -1 && SocketWouldBlock(USER1.socket[conn].fd)) {
			return 0;
		}
		return (res || (USER1.socket[conn].socktype == MOBILE_SOCKTYPE_UDP)) ? res : -2;
	}
	res = (int) SocketRecv(USER1.socket[conn].fd, data, size);
	if (res == -1 && SocketWouldBlock(USER1.socket[conn].fd)) {
		return 0;
	}
	return (res || (USER1.socket[conn].socktype == MOBILE_SOCKTYPE_UDP)) ? res : -2;
}

static void update_number(void* user, enum mobile_number type, const char* number) {
	char* dest = USER1.number[type];
	if (number) {
		strncpy(dest, number, MOBILE_MAX_NUMBER_SIZE);
		dest[MOBILE_MAX_NUMBER_SIZE] = '\0';
	} else {
		dest[0] = '\0';
	}
}

static bool GBMobileAdapterInit(struct GBSIODriver* driver);
static void GBMobileAdapterDeinit(struct GBSIODriver* driver);
static void GBMobileAdapterWriteSB(struct GBSIODriver* driver, uint8_t value);
static uint8_t GBMobileAdapterWriteSC(struct GBSIODriver* driver, uint8_t value);

void GBMobileAdapterCreate(struct GBMobileAdapter* mobile) {
	mobile->d.init = GBMobileAdapterInit;
	mobile->d.deinit = GBMobileAdapterDeinit;
	mobile->d.writeSB = GBMobileAdapterWriteSB;
	mobile->d.writeSC = GBMobileAdapterWriteSC;

	memset(&mobile->adapter, 0, sizeof(*mobile) - sizeof(mobile->d));
}

bool GBMobileAdapterInit(struct GBSIODriver* driver) {
	struct GBMobileAdapter* mobile = (struct GBMobileAdapter*) driver;
	mobile->adapter = mobile_new(mobile);
	if (!mobile->adapter) {
		return false;
	}
#define _MOBILE_SETCB(NAME) mobile_def_ ## NAME(mobile->adapter, NAME);
	_MOBILE_SETCB(debug_log);
	_MOBILE_SETCB(serial_disable);
	_MOBILE_SETCB(serial_enable);
	_MOBILE_SETCB(config_read);
	_MOBILE_SETCB(config_write);
	_MOBILE_SETCB(time_latch);
	_MOBILE_SETCB(time_check_ms);
	_MOBILE_SETCB(sock_open);
	_MOBILE_SETCB(sock_close);
	_MOBILE_SETCB(sock_connect);
	_MOBILE_SETCB(sock_listen);
	_MOBILE_SETCB(sock_accept);
	_MOBILE_SETCB(sock_send);
	_MOBILE_SETCB(sock_recv);
	_MOBILE_SETCB(update_number);
#undef _MOBILE_SETCB
	mobile_start(mobile->adapter);
	return true;
}

void GBMobileAdapterDeinit(struct GBSIODriver* driver) {
	struct GBMobileAdapter* mobile = (struct GBMobileAdapter*) driver;
	mobile_config_save(mobile->adapter);
	mobile_stop(mobile->adapter);
}

void GBMobileAdapterWriteSB(struct GBSIODriver* driver, uint8_t value) {
	(*(struct GBMobileAdapter*) driver).nextData[0] = value;
}

uint8_t GBMobileAdapterWriteSC(struct GBSIODriver* driver, uint8_t value) {
	struct GBMobileAdapter* mobile = (struct GBMobileAdapter*) driver;
	if ((value & 0x81) == 0x81) {
		mobile_loop(mobile->adapter);
		if (mobile->serial == 1) {
			driver->p->pendingSB = mobile->nextData[1];
			mobile->nextData[1] = mobile_transfer(mobile->adapter, mobile->nextData[0]);
		}
	}
	return value;
}

#undef USER1
#undef ADDR4
#undef ADDR6

#endif /* defined(USE_LIBMOBILE) */
