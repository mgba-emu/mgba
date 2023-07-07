/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/context.h>

#include <errno.h>

#include <mgba/internal/script/socket.h>
#include <mgba/script/macros.h>
#include <mgba-util/socket.h>

struct mScriptSocket {
	Socket socket;
	struct Address address;
	int32_t error;
	uint16_t port;
};
mSCRIPT_DECLARE_STRUCT(mScriptSocket);

static const struct _mScriptSocketErrorMapping {
	int32_t nativeError;
	enum mSocketErrorCode mappedError;
} _mScriptSocketErrorMappings[] = {
	{ EAGAIN,                mSCRIPT_SOCKERR_AGAIN },
	{ EADDRINUSE,            mSCRIPT_SOCKERR_ADDRESS_IN_USE },
	{ ECONNREFUSED,          mSCRIPT_SOCKERR_CONNECTION_REFUSED },
	{ EACCES,                mSCRIPT_SOCKERR_DENIED },
	{ EPERM,                 mSCRIPT_SOCKERR_DENIED },
	{ ENOTRECOVERABLE,       mSCRIPT_SOCKERR_FAILED },
	{ ENETUNREACH,           mSCRIPT_SOCKERR_NETWORK_UNREACHABLE },
	{ ETIMEDOUT,             mSCRIPT_SOCKERR_TIMEOUT },
	{ EINVAL,                mSCRIPT_SOCKERR_UNSUPPORTED },
	{ EPROTONOSUPPORT,       mSCRIPT_SOCKERR_UNSUPPORTED },
#ifndef USE_GETHOSTBYNAME
#ifdef _WIN32
	{ WSATRY_AGAIN,          mSCRIPT_SOCKERR_AGAIN },
	{ WSAEWOULDBLOCK,        mSCRIPT_SOCKERR_AGAIN },
	{ WSANO_RECOVERY,        mSCRIPT_SOCKERR_FAILED },
	{ WSANO_DATA,            mSCRIPT_SOCKERR_NO_DATA },
	{ WSAHOST_NOT_FOUND,     mSCRIPT_SOCKERR_NOT_FOUND },
	{ WSATYPE_NOT_FOUND,     mSCRIPT_SOCKERR_NOT_FOUND },
	{ WSA_NOT_ENOUGH_MEMORY, mSCRIPT_SOCKERR_OUT_OF_MEMORY },
	{ WSAEAFNOSUPPORT,       mSCRIPT_SOCKERR_UNSUPPORTED },
	{ WSAEINVAL,             mSCRIPT_SOCKERR_UNSUPPORTED },
	{ WSAESOCKTNOSUPPORT,    mSCRIPT_SOCKERR_UNSUPPORTED },
#else
	{ EAI_AGAIN,             mSCRIPT_SOCKERR_AGAIN },
	{ EAI_FAIL,              mSCRIPT_SOCKERR_FAILED },
#ifdef EAI_NODATA
	{ EAI_NODATA,            mSCRIPT_SOCKERR_NO_DATA },
#endif
	{ EAI_NONAME,            mSCRIPT_SOCKERR_NOT_FOUND },
	{ EAI_MEMORY,            mSCRIPT_SOCKERR_OUT_OF_MEMORY },
#endif
#else
	{ -TRY_AGAIN,            mSCRIPT_SOCKERR_AGAIN },
	{ -NO_RECOVERY,          mSCRIPT_SOCKERR_FAILED },
	{ -NO_DATA,              mSCRIPT_SOCKERR_NO_DATA },
	{ -HOST_NOT_FOUND,       mSCRIPT_SOCKERR_NOT_FOUND },
#endif
};
static const int _mScriptSocketNumErrorMappings = sizeof(_mScriptSocketErrorMappings) / sizeof(struct _mScriptSocketErrorMapping);

static void _mScriptSocketSetError(struct mScriptSocket* ssock, int32_t err) {
	if (!err) {
		ssock->error = mSCRIPT_SOCKERR_OK;
		return;
	}
	int i;
	for (i = 0; i < _mScriptSocketNumErrorMappings; i++) {
		if (_mScriptSocketErrorMappings[i].nativeError == err) {
			ssock->error = _mScriptSocketErrorMappings[i].mappedError;
			return;
		}
	}
	ssock->error = mSCRIPT_SOCKERR_UNKNOWN_ERROR;
}

static void _mScriptSocketSetSocket(struct mScriptSocket* ssock, Socket socket) {
	if (SOCKET_FAILED(socket)) {
		ssock->socket = INVALID_SOCKET;
		_mScriptSocketSetError(ssock, SocketError());
	} else {
		ssock->socket = socket;
		ssock->error = mSCRIPT_SOCKERR_OK;
	}
}

struct mScriptValue* _mScriptSocketCreate() {
	struct mScriptSocket client = {
		.socket = INVALID_SOCKET,
		.error = mSCRIPT_SOCKERR_OK,
		.port = 0
	};

	struct mScriptValue* result = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptSocket));
	result->value.opaque = calloc(1, sizeof(struct mScriptSocket));
	*(struct mScriptSocket*) result->value.opaque = client;
	result->flags = mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	return result;
}

void _mScriptSocketClose(struct mScriptSocket* ssock) {
	if (!SOCKET_FAILED(ssock->socket)) {
		SocketClose(ssock->socket);
	}
}

struct mScriptValue* _mScriptSocketAccept(struct mScriptSocket* ssock) {
	struct mScriptValue* value = _mScriptSocketCreate();
	struct mScriptSocket* client = (struct mScriptSocket*) value->value.opaque;
	_mScriptSocketSetSocket(client, SocketAccept(ssock->socket, &client->address));
	if (!client->error) {
		SocketSetBlocking(client->socket, false);
	}
	return value;
}

int32_t _mScriptSocketOpen(struct mScriptSocket* ssock, const char* addressStr, uint16_t port) {
	struct Address* addr = NULL;
	if (addressStr && addressStr[0]) {
		int32_t err = SocketResolveHost(addressStr, &ssock->address);
		if (err) {
			_mScriptSocketSetError(ssock, err);
			return err;
		}
		addr = &ssock->address;
	}
	ssock->port = port;
	_mScriptSocketSetSocket(ssock, SocketOpenTCP(port, addr));
	if (!ssock->error) {
		SocketSetBlocking(ssock->socket, false);
	}
	return ssock->error;
}

int32_t _mScriptSocketConnect(struct mScriptSocket* ssock, const char* addressStr, uint16_t port) {
	int32_t err = SocketResolveHost(addressStr, &ssock->address);
	if (err) {
		_mScriptSocketSetError(ssock, err);
		return err;
	}
	ssock->port = port;
	_mScriptSocketSetSocket(ssock, SocketConnectTCP(port, &ssock->address));
	if (!ssock->error) {
		SocketSetBlocking(ssock->socket, false);
	}
	return ssock->error;
}

int32_t _mScriptSocketListen(struct mScriptSocket* ssock, uint32_t queueLength) {
	_mScriptSocketSetError(ssock, SocketListen(ssock->socket, queueLength));
	return ssock->error;
}

int32_t _mScriptSocketSend(struct mScriptSocket* ssock, struct mScriptString* data) {
	ssize_t written = SocketSend(ssock->socket, data->buffer, data->size);
	if (written < 0) {
		_mScriptSocketSetError(ssock, SocketError());
		return -ssock->error;
	}
	ssock->error = mSCRIPT_SOCKERR_OK;
	return written;
}

struct mScriptValue* _mScriptSocketRecv(struct mScriptSocket* ssock, uint32_t maxBytes) {
	struct mScriptValue* value = mScriptStringCreateEmpty(maxBytes);
	struct mScriptString* data = value->value.string;
	ssize_t bytes = SocketRecv(ssock->socket, data->buffer, maxBytes);
	if (bytes <= 0) {
		data->size = 0;
		_mScriptSocketSetError(ssock, SocketError());
	} else {
		data->size = bytes;
		ssock->error = mSCRIPT_SOCKERR_OK;
	}
	return value;
}

// This works sufficiently well for a single socket, but it could be better.
// Ideally, all sockets would be tracked and selected on together for efficiency.
uint32_t _mScriptSocketSelectOne(struct mScriptSocket* ssock, int64_t timeoutMillis) {
	Socket reads[] = { ssock->socket };
	Socket errors[] = { ssock->socket };
	int result = SocketPoll(1, reads, NULL, errors, timeoutMillis);
	if (!result) {
		return 0;
	} else if (errors[0] != INVALID_SOCKET) {
		_mScriptSocketSetError(ssock, SocketError());
		return -1;
	}
	return 1;
}

mSCRIPT_BIND_FUNCTION(mScriptSocketCreate_Binding, W(mScriptSocket), _mScriptSocketCreate, 0);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptSocket, close, _mScriptSocketClose, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptSocket, W(mScriptSocket), accept, _mScriptSocketAccept, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptSocket, S32, open, _mScriptSocketOpen, 2, CHARP, address, U16, port);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptSocket, S32, connect, _mScriptSocketConnect, 2, CHARP, address, U16, port);
mSCRIPT_DECLARE_STRUCT_METHOD_WITH_DEFAULTS(mScriptSocket, S32, listen, _mScriptSocketListen, 1, U32, queueLength);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptSocket, S32, send, _mScriptSocketSend, 1, STR, data);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptSocket, WSTR, recv, _mScriptSocketRecv, 1, U32, maxBytes);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptSocket, S32, select, _mScriptSocketSelectOne, 1, S64, timeoutMillis);

mSCRIPT_DEFINE_STRUCT(mScriptSocket)
	mSCRIPT_DEFINE_INTERNAL
	mSCRIPT_DEFINE_CLASS_DOCSTRING("An internal implementation of a TCP network socket.")
	mSCRIPT_DEFINE_STRUCT_DEINIT_NAMED(mScriptSocket, close)
	mSCRIPT_DEFINE_DOCSTRING("Closes the socket. If the socket is already closed, this function does nothing.")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptSocket, close)
	mSCRIPT_DEFINE_DOCSTRING("Creates a new socket for an incoming connection from a listening server socket.")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptSocket, accept)
	mSCRIPT_DEFINE_DOCSTRING(
		"Binds the socket to a specified address and port. "
		"If no address is specified, the socket is bound to all network interfaces."
	)
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptSocket, open)
	mSCRIPT_DEFINE_DOCSTRING(
		"Opens a TCP connection to the specified address and port.\n"
		"**Caution:** This is a blocking call. The emulator will not respond until "
		"the connection either succeeds or fails."
	)
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptSocket, connect)
	mSCRIPT_DEFINE_DOCSTRING(
		"Begins listening for incoming connections. The socket must have first been "
		"bound with the struct::Socket.open method."
	)
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptSocket, listen)
	mSCRIPT_DEFINE_DOCSTRING(
		"Sends data over a socket. Returns the number of bytes written, or -1 if an "
		"error occurs."
	)
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptSocket, send)
	mSCRIPT_DEFINE_DOCSTRING(
		"Reads data from a socket, up to the specified number of bytes. "
		"If the socket has been disconnected, this function returns an empty string. "
		"Use struct::Socket.select to test if data is available to be read."
	)
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptSocket, recv)
	mSCRIPT_DEFINE_DOCSTRING(
		"Checks the status of the socket. "
		"Returns 1 if data is available to be read. "
		"Returns -1 if an error has occurred on the socket."
	)
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptSocket, select)
	mSCRIPT_DEFINE_DOCSTRING(
		"One of the C.SOCKERR constants describing the last error on the socket."
	)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptSocket, S32, error)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mScriptSocket, listen)
	mSCRIPT_S32(1)
mSCRIPT_DEFINE_DEFAULTS_END;


void mScriptContextAttachSocket(struct mScriptContext* context) {
	mScriptContextExportNamespace(context, "_socket", (struct mScriptKVPair[]) {
		mSCRIPT_KV_PAIR(create, &mScriptSocketCreate_Binding),
		mSCRIPT_KV_SENTINEL
	});
	mScriptContextSetDocstring(context, "_socket", "Basic TCP sockets library");
	mScriptContextSetDocstring(context, "_socket.create", "Creates a new socket object");
	mScriptContextExportConstants(context, "SOCKERR", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_SOCKERR, UNKNOWN_ERROR),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_SOCKERR, OK),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_SOCKERR, AGAIN),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_SOCKERR, ADDRESS_IN_USE),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_SOCKERR, CONNECTION_REFUSED),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_SOCKERR, DENIED),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_SOCKERR, FAILED),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_SOCKERR, NETWORK_UNREACHABLE),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_SOCKERR, NOT_FOUND),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_SOCKERR, NO_DATA),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_SOCKERR, OUT_OF_MEMORY),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_SOCKERR, TIMEOUT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_SOCKERR, UNSUPPORTED),
		mSCRIPT_KV_SENTINEL
	});
}
