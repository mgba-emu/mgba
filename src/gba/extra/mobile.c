#include <mgba/gba/interface.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/sio.h>

mLOG_DECLARE_CATEGORY(GBA_MOBILE);
mLOG_DEFINE_CATEGORY(GBA_MOBILE, "Mobile Adapter", "gba.mobile");

#define USER1 (*(struct GBASIOMobileAdapter*) (user))
#define ADDR4 (*(struct mobile_addr4*) (addr))
#define ADDR6 (*(struct mobile_addr6*) (addr))

void debug_log(void* user, const char* line) {
	UNUSED(user);
	mLOG(GBA_MOBILE, DEBUG, "%s", line);
}

void serial_disable(void* user) {
	USER1.serial = 0;
}

void serial_enable(void* user, bool mode_32bit) {
	USER1.serial = mode_32bit ? 4 : 1;
}

bool config_read(void* user, void* dest, uintptr_t offset, size_t size) {
	return memcpy(dest, USER1.config + offset, size) == dest;
}

bool config_write(void* user, const void* src, uintptr_t offset, size_t size) {
	return memcpy(USER1.config + offset, src, size) == USER1.config + offset;
}

void time_latch(void* user, unsigned timer) {
	USER1.timeLatch[timer] = mTimingCurrentTime(&USER1.d.p->p->timing);
}

bool time_check_ms(void* user, unsigned timer, unsigned ms) {
	return (unsigned) (mTimingCurrentTime(&USER1.d.p->p->timing) - USER1.timeLatch[timer]) * 1000U / GBA_ARM7TDMI_FREQUENCY >= ms;
}

bool sock_open(void* user, unsigned conn, enum mobile_socktype type, enum mobile_addrtype addrtype, unsigned bindport) {
	Socket fd;
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
		if (fd != INVALID_SOCKET) SocketSetBlocking(fd, false);
	}
	return (USER1.socket[conn].fd = fd) != INVALID_SOCKET || type != MOBILE_SOCKTYPE_UDP;
}

void sock_close(void* user, unsigned conn) {
	if (USER1.socket[conn].fd != INVALID_SOCKET) {
		SocketClose(USER1.socket[conn].fd);
	}
	USER1.socket[conn].fd = INVALID_SOCKET;
	USER1.socket[conn].addrtype = MOBILE_ADDRTYPE_NONE;
	USER1.socket[conn].bindport = 0;
}

int sock_connect(void* user, unsigned conn, const struct mobile_addr* addr) {
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

bool sock_listen(void* user, unsigned conn) {
	if (SOCKET_FAILED(USER1.socket[conn].fd)) {
		Socket fd;
		struct Address bindaddr, *bindptr = NULL;
		if (USER1.socket[conn].addrtype == MOBILE_ADDRTYPE_IPV6) {
			bindaddr.version = IPV6;
			memset(&bindaddr.ipv6, 0, sizeof(bindaddr.ipv6));
			bindptr = &bindaddr;
		}
		fd = SocketOpenTCP(USER1.socket[conn].bindport, bindptr);
		if (fd != INVALID_SOCKET) SocketSetBlocking(fd, false);
		USER1.socket[conn].fd = fd;
	}
	return !SOCKET_FAILED(SocketListen(USER1.socket[conn].fd, 1));
}

bool sock_accept(void* user, unsigned conn) {
	Socket fd = SocketAccept(USER1.socket[conn].fd, NULL);
	if (SOCKET_FAILED(fd)) return false;
	SocketSetBlocking(fd, false);
	sock_close(user, conn);
	USER1.socket[conn].fd = fd;
	return true;
}

int sock_send(void* user, unsigned conn, const void* data, unsigned size, const struct mobile_addr* addr) {
	if (addr) {
		struct Address destaddr;
		int destport;
		if (addr->type == MOBILE_ADDRTYPE_IPV6) {
			destaddr.version = IPV6;
			memcpy(&destaddr.ipv6, &ADDR6.host, MOBILE_HOSTLEN_IPV6);
			destport = ntohs(ADDR6.port);
		} else {
			destaddr.version = IPV4;
			destaddr.ipv4 = ntohl(*(uint32_t*) &ADDR4.host);
			destport = ntohs(ADDR4.port);
		}
		return SocketSendTo(USER1.socket[conn].fd, data, size, destport, &destaddr);
	}
	return SocketSend(USER1.socket[conn].fd, data, size);
}

int sock_recv(void* user, unsigned conn, void* data, unsigned size, struct mobile_addr* addr) {
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
			ADDR6.port = htons(srcport);
		} else {
			addr->type = MOBILE_ADDRTYPE_IPV4;
			*(uint32_t*) &ADDR4.host = htonl(srcaddr.ipv4);
			ADDR4.port = htons(srcport);
		}
		if (res == -1 && SocketWouldBlock(USER1.socket[conn].fd)) {
			return 0;
		}
		return res;
	}
	res = (int) SocketRecv(USER1.socket[conn].fd, data, size);
	if (res == -1 && SocketWouldBlock(USER1.socket[conn].fd)) {
		return 0;
	}
	return res ? res : -2;
}

void update_number(void* user, enum mobile_number type, const char* number) {
	char* dest = USER1.number[type];
	if (number) {
		strncpy(dest, number, MOBILE_MAX_NUMBER_SIZE);
		dest[MOBILE_MAX_NUMBER_SIZE] = '\0';
	} else {
		dest[0] = '\0';
	}
}

static bool GBASIOMobileAdapterInit(struct GBASIODriver* driver);
static void GBASIOMobileAdapterDeinit(struct GBASIODriver* driver);
static uint16_t GBASIOMobileAdapterWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);

static void _mobileTransfer(struct GBASIOMobileAdapter* mobile);
static void _mobileEvent(struct mTiming* timing, void* mobile, uint32_t cyclesLate);

void GBASIOMobileAdapterCreate(struct GBASIOMobileAdapter* mobile) {
	mobile->d.init = GBASIOMobileAdapterInit;
	mobile->d.deinit = GBASIOMobileAdapterDeinit;
	mobile->d.load = NULL;
	mobile->d.unload = NULL;
	mobile->d.writeRegister = GBASIOMobileAdapterWriteRegister;

	mobile->event.context = mobile;
	mobile->event.callback = _mobileEvent;
	mobile->event.priority = 0x80;

	memset(&mobile->adapter, 0, sizeof(*mobile) - sizeof(mobile->d) - sizeof(mobile->event));
}

bool GBASIOMobileAdapterInit(struct GBASIODriver* driver) {
	struct GBASIOMobileAdapter* mobile = (struct GBASIOMobileAdapter*) driver;
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

void GBASIOMobileAdapterDeinit(struct GBASIODriver* driver) {
	struct GBASIOMobileAdapter* mobile = (struct GBASIOMobileAdapter*) driver;
	mobile_config_save(mobile->adapter);
	mobile_stop(mobile->adapter);
}

uint16_t GBASIOMobileAdapterWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	struct GBASIOMobileAdapter* mobile = (struct GBASIOMobileAdapter*) driver;
	switch (address) {
	case REG_SIOCNT:
		value &= ~0x4;
		if (value & 0x80) {
			_mobileTransfer(mobile);
		}
		break;
	default:
		break;
	}
	return value;
}

void GBASIOMobileAdapterUpdate(struct GBASIOMobileAdapter* mobile) {
	mobile_loop(mobile->adapter);
}

void _mobileTransfer(struct GBASIOMobileAdapter* mobile) {
	int32_t cycles;
	if (mobile->d.p->mode == SIO_NORMAL_32) {
		cycles = GBA_ARM7TDMI_FREQUENCY / 0x40000;
	} else {
		cycles = GBASIOCyclesPerTransfer[GBASIOMultiplayerGetBaud(mobile->d.p->siocnt)][1];
	}
	mTimingDeschedule(&mobile->d.p->p->timing, &mobile->event);
	mTimingSchedule(&mobile->d.p->p->timing, &mobile->event, cycles);
}

void _mobileEvent(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	UNUSED(timing);
	struct GBASIOMobileAdapter* mobile = user;

	GBASIOMobileAdapterUpdate(mobile);

	if (mobile->d.p->mode == SIO_NORMAL_32 && mobile->serial == 4) {
		uint8_t buf[4];
#define _SIO_DATA (&mobile->d.p->p->memory.io[REG_SIODATA32_LO >> 1])
		memcpy(buf, _SIO_DATA, sizeof(buf));
		memcpy(_SIO_DATA, mobile->nextData, sizeof(mobile->nextData));
#undef _SIO_DATA
		for (int i = 3; i >= 0; --i) {
			mobile->nextData[i] = mobile_transfer(mobile->adapter, buf[i]);
		}
	} else if (mobile->d.p->mode == SIO_NORMAL_8 && mobile->serial == 1) {
		uint8_t tmp = *(uint8_t*) &mobile->d.p->p->memory.io[REG_SIODATA8 >> 1];
		*(uint8_t*) &mobile->d.p->p->memory.io[REG_SIODATA8 >> 1] = mobile->nextData[3];
		mobile->nextData[3] = mobile_transfer(mobile->adapter, tmp);
	}

	mobile->d.p->siocnt = GBASIONormalClearStart(mobile->d.p->siocnt);

	if (GBASIOMultiplayerIsIrq(mobile->d.p->siocnt)) {
		GBARaiseIRQ(mobile->d.p->p, GBA_IRQ_SIO, cyclesLate);
	}
}

#undef USER1
#undef ADDR4
#undef ADDR6
