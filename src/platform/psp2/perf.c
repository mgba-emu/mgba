/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/socket.h>

#include <psp2/appmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#define MAX_ARGS 19


void connectServer(char* args[]) {
	Socket server = SocketOpenTCP(7215, NULL);
	if (SOCKET_FAILED(server)) {
		return;
	}
	if (SOCKET_FAILED(SocketListen(server, 0))) {
		SocketClose(server);
		return;
	}
	Socket conn = SocketAccept(server, NULL);
	if (SOCKET_FAILED(conn)) {
		SocketClose(server);
		return;
	}

	int i = 0;
	ssize_t len;
	char arg[1024];
	while (i < MAX_ARGS && (len = SocketRecv(conn, arg, sizeof(arg) - 1)) > 0) {
		arg[len] = '\0';
		char* tok;
		for (tok = strtok(arg, "\n"); tok && i < MAX_ARGS; ++i) {
			args[i] = strdup(tok);
			tok = strtok(NULL, "\n");
		}
		if (arg[len - 1] == '\n') {
			return;
		}
	}

	SocketClose(conn);
	SocketClose(server);
}

int main() {
	char* args[MAX_ARGS + 1] = { 0 };
	SocketSubsystemInit();
	connectServer(args);
	SocketSubsystemDeinit();
	sceAppMgrLoadExec("app0:/perf.bin", args, NULL);
	sceKernelExitProcess(0);
	return 0;
}
