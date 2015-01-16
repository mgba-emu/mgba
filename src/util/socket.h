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
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#define INVALID_SOCKET (-1)
#define SOCKET_FAILED(s) ((s) < 0)
typedef int Socket;
#endif


static inline void SocketSubsystemInitialize() {
#ifdef _WIN32
	WSAStartup(MAKEWORD(2, 2), 0);
#endif
}

static inline ssize_t SocketSend(Socket socket, const void* buffer, size_t size) {
	return write(socket, buffer, size);
}

static inline ssize_t SocketRecv(Socket socket, void* buffer, size_t size) {
	return read(socket, buffer, size);
}

static inline Socket SocketOpenTCP(int port, uint32_t bindAddress) {
	Socket sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (SOCKET_FAILED(sock)) {
		return sock;
	}

	struct sockaddr_in bindInfo = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr = { 0 }
	};
	bindInfo.sin_addr.s_addr = htonl(bindAddress);
	int err = bind(sock, (const struct sockaddr*) &bindInfo, sizeof(struct sockaddr_in));
	if (err) {
		close(sock);
		return -1;
	}
	return sock;
}

static inline Socket SocketConnectTCP(int port, uint32_t destinationAddress) {
	Socket sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (SOCKET_FAILED(sock)) {
		return sock;
	}

	struct sockaddr_in bindInfo = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr = { 0 }
	};
	bindInfo.sin_addr.s_addr = htonl(destinationAddress);
	int err = connect(sock, (const struct sockaddr*) &bindInfo, sizeof(struct sockaddr_in));
	if (err) {
		close(sock);
		return -1;
	}
	return sock;
}

static inline Socket SocketListen(Socket socket, int queueLength) {
	return listen(socket, queueLength);
}

static inline Socket SocketAccept(Socket socket, struct sockaddr* restrict address, socklen_t* restrict addressLength) {
	return accept(socket, address, addressLength);
}

static inline int SocketClose(Socket socket) {
	return close(socket) >= 0;
}

static inline int SocketSetBlocking(Socket socket, int blocking) {
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

#endif
