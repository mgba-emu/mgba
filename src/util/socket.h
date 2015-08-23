/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SOCKET_H
#define SOCKET_H

#include "util/common.h"

#ifdef __cplusplus
#define restrict __restrict__
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

#define SOCKET_FAILED(s) ((s) == INVALID_SOCKET)
typedef SOCKET Socket;
#else
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>

#define INVALID_SOCKET (-1)
#define SOCKET_FAILED(s) ((s) < 0)
typedef int Socket;
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

static inline void SocketSubsystemInit() {
#ifdef _WIN32
	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);
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
#else
	return write(socket, buffer, size);
#endif
}

static inline ssize_t SocketRecv(Socket socket, void* buffer, size_t size) {
#ifdef _WIN32
	return recv(socket, (char*) buffer, size, 0);
#else
	return read(socket, buffer, size);
#endif
}

static inline Socket SocketOpenTCP(int port, const struct Address* bindAddress) {
	Socket sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (SOCKET_FAILED(sock)) {
		return sock;
	}

	int err;
	if (!bindAddress) {
		struct sockaddr_in bindInfo;
		memset(&bindInfo, 0, sizeof(bindInfo));
		bindInfo.sin_family = AF_INET;
		bindInfo.sin_port = htons(port);
		err = bind(sock, (const struct sockaddr*) &bindInfo, sizeof(bindInfo));
	} else if (bindAddress->version == IPV4) {
		struct sockaddr_in bindInfo;
		memset(&bindInfo, 0, sizeof(bindInfo));
		bindInfo.sin_family = AF_INET;
		bindInfo.sin_port = htons(port);
		bindInfo.sin_addr.s_addr = bindAddress->ipv4;
		err = bind(sock, (const struct sockaddr*) &bindInfo, sizeof(bindInfo));
	} else {
		struct sockaddr_in6 bindInfo;
		memset(&bindInfo, 0, sizeof(bindInfo));
		bindInfo.sin6_family = AF_INET6;
		bindInfo.sin6_port = htons(port);
		memcpy(bindInfo.sin6_addr.s6_addr, bindAddress->ipv6, sizeof(bindInfo.sin6_addr.s6_addr));
		err = bind(sock, (const struct sockaddr*) &bindInfo, sizeof(bindInfo));
	}
	if (err) {
		close(sock);
		return INVALID_SOCKET;
	}
	return sock;
}

static inline Socket SocketConnectTCP(int port, const struct Address* destinationAddress) {
	Socket sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (SOCKET_FAILED(sock)) {
		return sock;
	}

	int err;
	if (!destinationAddress) {
		struct sockaddr_in bindInfo;
		memset(&bindInfo, 0, sizeof(bindInfo));
		bindInfo.sin_family = AF_INET;
		bindInfo.sin_port = htons(port);
		err = connect(sock, (const struct sockaddr*) &bindInfo, sizeof(bindInfo));
	} else if (destinationAddress->version == IPV4) {
		struct sockaddr_in bindInfo;
		memset(&bindInfo, 0, sizeof(bindInfo));
		bindInfo.sin_family = AF_INET;
		bindInfo.sin_port = htons(port);
		bindInfo.sin_addr.s_addr = destinationAddress->ipv4;
		err = connect(sock, (const struct sockaddr*) &bindInfo, sizeof(bindInfo));
	} else {
		struct sockaddr_in6 bindInfo;
		memset(&bindInfo, 0, sizeof(bindInfo));
		bindInfo.sin6_family = AF_INET6;
		bindInfo.sin6_port = htons(port);
		memcpy(bindInfo.sin6_addr.s6_addr, destinationAddress->ipv6, sizeof(bindInfo.sin6_addr.s6_addr));
		err = connect(sock, (const struct sockaddr*) &bindInfo, sizeof(bindInfo));
	}

	if (err) {
		close(sock);
		return INVALID_SOCKET;
	}
	return sock;
}

static inline Socket SocketListen(Socket socket, int queueLength) {
	return listen(socket, queueLength);
}

static inline Socket SocketAccept(Socket socket, struct Address* address) {
	if (!address) {
		return accept(socket, 0, 0);
	}
	if (address->version == IPV4) {
		struct sockaddr_in addrInfo;
		memset(&addrInfo, 0, sizeof(addrInfo));
		addrInfo.sin_family = AF_INET;
		addrInfo.sin_addr.s_addr = address->ipv4;
		socklen_t len = sizeof(addrInfo);
		return accept(socket, (struct sockaddr*) &addrInfo, &len);
	} else {
		struct sockaddr_in6 addrInfo;
		memset(&addrInfo, 0, sizeof(addrInfo));
		addrInfo.sin6_family = AF_INET6;
		memcpy(addrInfo.sin6_addr.s6_addr, address->ipv6, sizeof(addrInfo.sin6_addr.s6_addr));
		socklen_t len = sizeof(addrInfo);
		return accept(socket, (struct sockaddr*) &addrInfo, &len);
	}
}

static inline int SocketClose(Socket socket) {
	return close(socket) >= 0;
}

static inline int SocketSetBlocking(Socket socket, bool blocking) {
#ifdef _WIN32
	u_long unblocking = !blocking;
	return ioctlsocket(socket, FIONBIO, &unblocking) == NO_ERROR;
#else
	int flags = fcntl(socket, F_GETFL);
	if (flags == -1) {
		return 0;
	}
	if (blocking) {
		flags &= ~O_NONBLOCK;
	} else {
		flags |= O_NONBLOCK;
	}
	return fcntl(socket, F_SETFL, flags) >= 0;
#endif
}

static inline int SocketSetTCPPush(Socket socket, int push) {
	return setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char*) &push, sizeof(int)) >= 0;
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
	struct timeval tv;
	tv.tv_sec = timeoutMillis / 1000;
	tv.tv_usec = (timeoutMillis % 1000) * 1000;
	int result = select(maxFd + 1, &rset, &wset, &eset, timeoutMillis < 0 ? 0 : &tv);
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

#endif
