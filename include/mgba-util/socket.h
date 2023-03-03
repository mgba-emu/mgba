/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SOCKET_H
#define SOCKET_H

#include <mgba-util/common.h>

CXX_GUARD_START

#if defined(__cplusplus) && !defined(restrict)
#define restrict __restrict__
#endif

#ifdef _WIN32
#include <ws2tcpip.h>

#define SOCKET_FAILED(s) ((s) == INVALID_SOCKET)
typedef SOCKET Socket;
#else
#ifdef GEKKO
#define USE_GETHOSTBYNAME
#include <network.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

#ifndef GEKKO
#define INVALID_SOCKET (-1)
#endif
#define SOCKET_FAILED(s) ((s) < 0)
typedef int Socket;
#endif

#if !defined(__3DS__) && !defined(GEKKO)
#define HAS_IPV6
#endif

enum IP {
	IPV4,
	IPV6
};

struct Address {
	enum IP version;
	union {
		uint32_t ipv4;
		uint8_t ipv6[16];
	};
};

#ifdef __3DS__
#include <3ds.h>
#include <malloc.h>

#define SOCU_ALIGN 0x1000
#define SOCU_BUFFERSIZE 0x100000

extern u32* SOCUBuffer;
#endif
#ifdef __SWITCH__
#include <switch.h>
#endif
#ifdef PSP2
#include <psp2/net/net.h>
#include <psp2/sysmodule.h>
#endif

static inline void SocketSubsystemInit() {
#ifdef _WIN32
	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);
#elif defined(__3DS__)
	if (!SOCUBuffer) {
		SOCUBuffer = memalign(SOCU_ALIGN, SOCU_BUFFERSIZE);
		socInit(SOCUBuffer, SOCU_BUFFERSIZE);
	}
#elif defined(__SWITCH__)
	socketInitializeDefault();
#elif defined(GEKKO)
	net_init();
#elif defined(PSP2)
	static uint8_t netMem[1024*1024];
	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	sceNetInit(&(SceNetInitParam) { netMem, sizeof(netMem) });
#endif
}

static inline void SocketSubsystemDeinit() {
#ifdef _WIN32
	WSACleanup();
#elif defined(__3DS__)
	socExit();
	free(SOCUBuffer);
	SOCUBuffer = NULL;
#elif defined(__SWITCH__)
	socketExit();
#elif defined(GEKKO)
	net_deinit();
#elif defined(PSP2)
	sceNetTerm();
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
#endif
}

static inline int SocketError() {
#ifdef _WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

static inline bool SocketWouldBlock() {
#ifdef _WIN32
	return SocketError() == WSAEWOULDBLOCK;
#else
	return SocketError() == EWOULDBLOCK || SocketError() == EAGAIN;
#endif
}

static inline ssize_t SocketSend(Socket socket, const void* buffer, size_t size) {
#ifdef _WIN32
	return send(socket, (const char*) buffer, size, 0);
#elif defined(GEKKO)
	return net_write(socket, buffer, size);
#else
	return write(socket, buffer, size);
#endif
}

static inline ssize_t SocketRecv(Socket socket, void* buffer, size_t size) {
#if defined(_WIN32) || defined(__SWITCH__)
	return recv(socket, (char*) buffer, size, 0);
#elif defined(GEKKO)
	return net_read(socket, buffer, size);
#else
	return read(socket, buffer, size);
#endif
}

static inline int SocketClose(Socket socket) {
#ifdef _WIN32
	return closesocket(socket) == 0;
#elif defined(GEKKO)
	return net_close(socket) >= 0;
#else
	return close(socket) >= 0;
#endif
}

static inline void SocketCloseQuiet(Socket socket) {
	int savedErrno = SocketError();
	SocketClose(socket);
#ifdef _WIN32
	WSASetLastError(savedErrno);
#else
	errno = savedErrno;
#endif
}

static inline Socket SocketCreate(bool useIPv6, int protocol) {
	if (useIPv6) {
#ifdef HAS_IPV6
		return socket(AF_INET6, SOCK_STREAM, protocol);
#else
		errno = EAFNOSUPPORT;
		return INVALID_SOCKET;
#endif
	} else {
#ifdef GEKKO
		return net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
#else
		return socket(AF_INET, SOCK_STREAM, protocol);
#endif
	}
}

static inline Socket SocketOpenTCP(int port, const struct Address* bindAddress) {
	bool useIPv6 = bindAddress && (bindAddress->version == IPV6);
	Socket sock = SocketCreate(useIPv6, IPPROTO_TCP);
	if (SOCKET_FAILED(sock)) {
		return sock;
	}

	int err;

	const int enable = 1;
#ifdef GEKKO
	err = net_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
#elif defined(_WIN32)
	err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*) &enable, sizeof(enable));
#else
	err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
#endif
	if (err) {
		SocketCloseQuiet(sock);
		return INVALID_SOCKET;
	}

	if (!bindAddress) {
		struct sockaddr_in bindInfo;
		memset(&bindInfo, 0, sizeof(bindInfo));
		bindInfo.sin_family = AF_INET;
		bindInfo.sin_port = htons(port);
#ifndef __3DS__
		bindInfo.sin_addr.s_addr = INADDR_ANY;
#else
		bindInfo.sin_addr.s_addr = gethostid();
#endif
#ifdef GEKKO
		err = net_bind(sock, (struct sockaddr*) &bindInfo, sizeof(bindInfo));
#else
		err = bind(sock, (const struct sockaddr*) &bindInfo, sizeof(bindInfo));
#endif
	} else if (!useIPv6) {
		struct sockaddr_in bindInfo;
		memset(&bindInfo, 0, sizeof(bindInfo));
		bindInfo.sin_family = AF_INET;
		bindInfo.sin_port = htons(port);
		bindInfo.sin_addr.s_addr = htonl(bindAddress->ipv4);
#ifdef GEKKO
		err = net_bind(sock, (struct sockaddr*) &bindInfo, sizeof(bindInfo));
#else
		err = bind(sock, (const struct sockaddr*) &bindInfo, sizeof(bindInfo));
#endif
#if !defined(__3DS__) && !defined(GEKKO)
	} else {
		struct sockaddr_in6 bindInfo;
		memset(&bindInfo, 0, sizeof(bindInfo));
		bindInfo.sin6_family = AF_INET6;
		bindInfo.sin6_port = htons(port);
		memcpy(bindInfo.sin6_addr.s6_addr, bindAddress->ipv6, sizeof(bindInfo.sin6_addr.s6_addr));
		err = bind(sock, (const struct sockaddr*) &bindInfo, sizeof(bindInfo));
#endif
	}
	if (err) {
		SocketCloseQuiet(sock);
		return INVALID_SOCKET;
	}
	return sock;
}

static inline Socket SocketConnectTCP(int port, const struct Address* destinationAddress) {
	bool useIPv6 = destinationAddress && (destinationAddress->version == IPV6);
	Socket sock = SocketCreate(useIPv6, IPPROTO_TCP);
	if (SOCKET_FAILED(sock)) {
		return sock;
	}

	int err;
	if (!destinationAddress) {
		struct sockaddr_in bindInfo;
		memset(&bindInfo, 0, sizeof(bindInfo));
		bindInfo.sin_family = AF_INET;
		bindInfo.sin_port = htons(port);
#ifdef GEKKO
		err = net_connect(sock, (struct sockaddr*) &bindInfo, sizeof(bindInfo));
#else
		err = connect(sock, (const struct sockaddr*) &bindInfo, sizeof(bindInfo));
#endif
	} else if (destinationAddress->version == IPV4) {
		struct sockaddr_in bindInfo;
		memset(&bindInfo, 0, sizeof(bindInfo));
		bindInfo.sin_family = AF_INET;
		bindInfo.sin_port = htons(port);
		bindInfo.sin_addr.s_addr = htonl(destinationAddress->ipv4);
#ifdef GEKKO
		err = net_connect(sock, (struct sockaddr*) &bindInfo, sizeof(bindInfo));
#else
		err = connect(sock, (const struct sockaddr*) &bindInfo, sizeof(bindInfo));
#endif
#ifdef HAS_IPV6
	} else {
		struct sockaddr_in6 bindInfo;
		memset(&bindInfo, 0, sizeof(bindInfo));
		bindInfo.sin6_family = AF_INET6;
		bindInfo.sin6_port = htons(port);
		memcpy(bindInfo.sin6_addr.s6_addr, destinationAddress->ipv6, sizeof(bindInfo.sin6_addr.s6_addr));
		err = connect(sock, (const struct sockaddr*) &bindInfo, sizeof(bindInfo));
#endif
	}

	if (err) {
		SocketCloseQuiet(sock);
		return INVALID_SOCKET;
	}
	return sock;
}

static inline Socket SocketListen(Socket socket, int queueLength) {
#ifdef GEKKO
	return net_listen(socket, queueLength);
#else
#ifdef PSP2
	if (queueLength <= 0) {
		queueLength = 1;
	}
#endif
	return listen(socket, queueLength);
#endif
}

static inline Socket SocketAccept(Socket socket, struct Address* address) {
	if (!address) {
#ifdef GEKKO
		struct sockaddr_in addrInfo;
		memset(&addrInfo, 0, sizeof(addrInfo));
		socklen_t len = sizeof(addrInfo);
		return net_accept(socket, (struct sockaddr*) &addrInfo, &len);
#else
		return accept(socket, 0, 0);
#endif
	}
	if (address->version == IPV4) {
		struct sockaddr_in addrInfo;
		memset(&addrInfo, 0, sizeof(addrInfo));
		addrInfo.sin_family = AF_INET;
		addrInfo.sin_addr.s_addr = address->ipv4;
		socklen_t len = sizeof(addrInfo);
#ifdef GEKKO
		return net_accept(socket, (struct sockaddr*) &addrInfo, &len);
#else
		return accept(socket, (struct sockaddr*) &addrInfo, &len);
#endif
#if !defined(__3DS__) && !defined(GEKKO)
	} else {
		struct sockaddr_in6 addrInfo;
		memset(&addrInfo, 0, sizeof(addrInfo));
		addrInfo.sin6_family = AF_INET6;
		memcpy(addrInfo.sin6_addr.s6_addr, address->ipv6, sizeof(addrInfo.sin6_addr.s6_addr));
		socklen_t len = sizeof(addrInfo);
		return accept(socket, (struct sockaddr*) &addrInfo, &len);
#endif
	}
	return INVALID_SOCKET;
}

static inline int SocketSetBlocking(Socket socket, bool blocking) {
#ifdef _WIN32
	u_long unblocking = !blocking;
	return ioctlsocket(socket, FIONBIO, &unblocking) == NO_ERROR;
#else
#ifdef GEKKO
	int flags = net_fcntl(socket, F_GETFL, 0);
#else
	int flags = fcntl(socket, F_GETFL);
#endif
	if (flags == -1) {
		return 0;
	}
	if (blocking) {
		flags &= ~O_NONBLOCK;
	} else {
		flags |= O_NONBLOCK;
	}
#ifdef GEKKO
	return net_fcntl(socket, F_SETFL, flags) >= 0;
#else
	return fcntl(socket, F_SETFL, flags) >= 0;
#endif
#endif
}

static inline int SocketSetTCPPush(Socket socket, int push) {
#ifdef GEKKO
	return net_setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char*) &push, sizeof(int)) >= 0;
#else
	return setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char*) &push, sizeof(int)) >= 0;
#endif
}

static inline int SocketPoll(size_t nSockets, Socket* reads, Socket* writes, Socket* errors, int64_t timeoutMillis) {
	fd_set rset;
	fd_set wset;
	fd_set eset;
	FD_ZERO(&rset);
	FD_ZERO(&wset);
	FD_ZERO(&eset);
	size_t i;
	Socket maxFd = 0;
	if (reads) {
		for (i = 0; i < nSockets; ++i) {
			if (SOCKET_FAILED(reads[i])) {
				break;
			}
			if (reads[i] > maxFd) {
				maxFd = reads[i];
			}
			FD_SET(reads[i], &rset);
			reads[i] = INVALID_SOCKET;
		}
	}
	if (writes) {
		for (i = 0; i < nSockets; ++i) {
			if (SOCKET_FAILED(writes[i])) {
				break;
			}
			if (writes[i] > maxFd) {
				maxFd = writes[i];
			}
			FD_SET(writes[i], &wset);
			writes[i] = INVALID_SOCKET;
		}
	}
	if (errors) {
		for (i = 0; i < nSockets; ++i) {
			if (SOCKET_FAILED(errors[i])) {
				break;
			}
			if (errors[i] > maxFd) {
				maxFd = errors[i];
			}
			FD_SET(errors[i], &eset);
			errors[i] = INVALID_SOCKET;
		}
	}
	++maxFd;
	struct timeval tv;
	tv.tv_sec = timeoutMillis / 1000;
	tv.tv_usec = (timeoutMillis % 1000) * 1000;
#ifdef GEKKO
	int result = net_select(maxFd, &rset, &wset, &eset, timeoutMillis < 0 ? 0 : &tv);
#else
	int result = select(maxFd, &rset, &wset, &eset, timeoutMillis < 0 ? 0 : &tv);
#endif
	int r = 0;
	int w = 0;
	int e = 0;
	Socket j;
	for (j = 0; j < maxFd; ++j) {
		if (reads && FD_ISSET(j, &rset)) {
			reads[r] = j;
			++r;
		}
		if (writes && FD_ISSET(j, &wset)) {
			writes[w] = j;
			++w;
		}
		if (errors && FD_ISSET(j, &eset)) {
			errors[e] = j;
			++e;
		}
	}
	return result;
}

static inline int SocketResolveHost(const char* addrString, struct Address* destAddress) {
	int result = 0;
#ifdef USE_GETHOSTBYNAME
#warning Using gethostbyname() for hostname resolution is not threadsafe
#ifdef GEKKO
	struct hostent* host = net_gethostbyname(addrString);
#else
	struct hostent* host = gethostbyname(addrString);
#endif
	if (!host) {
		return errno;
	}
	if (host->h_addrtype == AF_INET && host->h_length == 4) {
		destAddress->version = IPV4;
		destAddress->ipv4 = ntohl(*host->h_addr_list[0]);
	}
#ifdef HAS_IPV6
	else if (host->h_addrtype == AF_INET6 && host->h_length == 16) {
		destAddress->version = IPV6;
		memcpy(destAddress->ipv6, host->h_addr_list[0], 16);
	}
#endif
	else {
#ifdef GEKKO
		result = errno;
#else
		result = -h_errno;
#endif
	}
#else
	struct addrinfo* addr = NULL;
	result = getaddrinfo(addrString, NULL, NULL, &addr);
	if (result) {
#ifdef EAI_SYSTEM
		if (result == EAI_SYSTEM) {
			result = errno;
		}
#endif
		goto error;
	}
	if (addr->ai_family == AF_INET && addr->ai_addrlen == sizeof(struct sockaddr_in)) {
		struct sockaddr_in* addr4 = (struct sockaddr_in*) addr->ai_addr;
		destAddress->version = IPV4;
		destAddress->ipv4 = ntohl(addr4->sin_addr.s_addr);
	}
#ifdef HAS_IPV6
	else if (addr->ai_family == AF_INET6 && addr->ai_addrlen == sizeof(struct sockaddr_in6)) {
		struct sockaddr_in6* addr6 = (struct sockaddr_in6*) addr->ai_addr;
		destAddress->version = IPV6;
		memcpy(destAddress->ipv6, addr6->sin6_addr.s6_addr, 16);
	}
#endif
	else {
#ifdef _WIN32
		result = WSANO_DATA;
#else
		result = EAI_NONAME;
#endif
	}
error:
	if (addr) {
		freeaddrinfo(addr);
	}
#endif
	return result;
}

CXX_GUARD_END

#endif
