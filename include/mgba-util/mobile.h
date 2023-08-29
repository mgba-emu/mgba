#ifndef MOBILE_H
#define MOBILE_H

#ifdef USE_LIBMOBILE

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/socket.h>
#include <mobile.h>

struct MobileAdapterGB {
	void *p;

	struct mobile_adapter *adapter;
	uint8_t config[MOBILE_CONFIG_SIZE];
	struct {
		Socket fd;
		enum mobile_socktype socktype;
		enum mobile_addrtype addrtype;
		unsigned bindport;
	} socket[MOBILE_MAX_CONNECTIONS];
	int serial;
	char number[2][MOBILE_MAX_NUMBER_SIZE + 1];
};

void serial_disable(void* user);
void serial_enable(void* user, bool mode_32bit);
bool config_read(void* user, void* dest, uintptr_t offset, size_t size);
bool config_write(void* user, const void* src, uintptr_t offset, size_t size);
bool sock_open(void* user, unsigned conn, enum mobile_socktype type, enum mobile_addrtype addrtype, unsigned bindport);
void sock_close(void* user, unsigned conn);
int sock_connect(void* user, unsigned conn, const struct mobile_addr* addr);
bool sock_listen(void* user, unsigned conn);
bool sock_accept(void* user, unsigned conn);
int sock_send(void* user, unsigned conn, const void* data, unsigned size, const struct mobile_addr* addr);
int sock_recv(void* user, unsigned conn, void* data, unsigned size, struct mobile_addr* addr);
void update_number(void* user, enum mobile_number type, const char* number);

CXX_GUARD_END

#endif

#endif
