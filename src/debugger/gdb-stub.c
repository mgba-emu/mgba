#include "gdb-stub.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

void _gdbStubDeinit(struct ARMDebugger* debugger) {
	struct GDBStub* stub = (struct GDBStub*) debugger;
	if (stub->socket >= 0) {
		GDBStubShutdown(stub);
	}
}

void GDBStubCreate(struct GDBStub* stub) {
	stub->socket = -1;
	stub->connection = -1;
	stub->d.init = 0;
	stub->d.deinit = _gdbStubDeinit;
	stub->d.paused = 0;
}

int GDBStubListen(struct GDBStub* stub, int port, uint32_t bindAddress) {
	if (stub->socket >= 0) {
		GDBStubShutdown(stub);
	}
	// TODO: support IPv6
	stub->socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (stub->socket < 0) {
		printf("Couldn't open socket\n");
		return 0;
	}

	struct sockaddr_in bindInfo = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr = {
			.s_addr = htonl(bindAddress)
		}
	};
	int err = bind(stub->socket, (const struct sockaddr*) &bindInfo, sizeof(struct sockaddr_in));
	if (err) {
		goto cleanup;
	}
	err = listen(stub->socket, 1);
	if (err) {
		goto cleanup;
	}
	int flags = fcntl(stub->socket, F_GETFL);
	if (flags == -1) {
		goto cleanup;
	}
	flags |= O_NONBLOCK;
	fcntl(stub->socket, F_SETFL, flags | O_NONBLOCK);

	return 1;

cleanup:
	close(stub->socket);
	stub->socket = -1;
	return 0;
}

void GDBStubHangup(struct GDBStub* stub) {
	if (stub->connection >= 0) {
		close(stub->connection);
		stub->connection = -1;
	}
}

void GDBStubShutdown(struct GDBStub* stub) {
	GDBStubHangup(stub);
	if (stub->socket >= 0) {
		close(stub->socket);
		stub->socket = -1;
	}
}

void GDBStubUpdate(struct GDBStub* stub) {
	if (stub->connection == -1) {
		stub->connection = accept(stub->socket, 0, 0);
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			return;
		}
		if (stub->connection >= 0) {
			int flags = fcntl(stub->connection, F_GETFL);
			if (flags == -1) {
				goto connectionLost;
			}
			flags |= O_NONBLOCK;
			fcntl(stub->connection, F_SETFL, flags | O_NONBLOCK);
		} else {
			goto connectionLost;
		}
	}
	while (1) {
		ssize_t messageLen = recv(stub->connection, stub->line, GDB_STUB_MAX_LINE - 1, 0);
		if (messageLen == 0) {
			goto connectionLost;
		}
		if (messageLen == -1) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				return;
			}
			goto connectionLost;
		}
		stub->line[messageLen] = '\0';
		printf("Received message: %s\n", stub->line);
	}
	return;

connectionLost:
	// TODO: add logging support to the debugging interface
	printf("Connection lost\n");
	GDBStubHangup(stub);
}
