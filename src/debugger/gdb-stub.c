#include "gdb-stub.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

enum GDBError {
	GDB_NO_ERROR = 0x00,
	GDB_BAD_ARGUMENTS = 0x06,
	GDB_UNSUPPORTED_COMMAND = 0x07
};

static void _gdbStubDeinit(struct ARMDebugger* debugger) {
	struct GDBStub* stub = (struct GDBStub*) debugger;
	if (stub->socket >= 0) {
		GDBStubShutdown(stub);
	}
}

static void _ack(struct GDBStub* stub) {
	char ack = '+';
	send(stub->connection, &ack, 1, 0);
}

static void _nak(struct GDBStub* stub) {
	char nak = '-';
	printf("Packet error\n");
	send(stub->connection, &nak, 1, 0);
}

static uint32_t _hex2int(const char* hex, int maxDigits) {
	uint32_t value = 0;
	uint8_t letter;

	while (maxDigits--) {
		letter = *hex - '0';
		if (letter > 9) {
			letter = *hex - 'a';
			if  (letter > 5) {
				break;
			}
			value *= 0x10;
			value += letter + 10;
		} else {
			value *= 0x10;
			value += letter;
		}
		++hex;
	}
	return value;
}

static void _int2hex8(uint8_t value, char* out) {
	static const char language[] = "0123456789abcdef";
	out[0] = language[value >> 4];
	out[1] = language[value & 0xF];
}

static void _int2hex32(uint32_t value, char* out) {
	static const char language[] = "0123456789abcdef";
	out[6] = language[value >> 28];
	out[7] = language[(value >> 24) & 0xF];
	out[4] = language[(value >> 20) & 0xF];
	out[5] = language[(value >> 16) & 0xF];
	out[2] = language[(value >> 12) & 0xF];
	out[3] = language[(value >> 8) & 0xF];
	out[0] = language[(value >> 4) & 0xF];
	out[1] = language[value & 0xF];
}

static void _sendMessage(struct GDBStub* stub) {
	if (stub->lineAck != GDB_ACK_OFF) {
		stub->lineAck = GDB_ACK_PENDING;
	}
	uint8_t checksum = 0;
	int i;
	char buffer = stub->outgoing[0];
	char swap;
	stub->outgoing[0] = '$';
	for (i = 1; i < GDB_STUB_MAX_LINE - 5; ++i) {
		checksum += buffer;
		swap = stub->outgoing[i];
		stub->outgoing[i] = buffer;
		buffer = swap;
		if (!buffer) {
			++i;
			break;
		}
	}
	stub->outgoing[i] = '#';
	_int2hex8(checksum, &stub->outgoing[i + 1]);
	stub->outgoing[i + 3] = 0;
	printf("> %s\n", stub->outgoing);
	send(stub->connection, stub->outgoing, i + 3, 0);
}

static void _error(struct GDBStub* stub, enum GDBError error) {
	snprintf(stub->outgoing, GDB_STUB_MAX_LINE - 1, "E%02x", error);
	_sendMessage(stub);
}

static void _readMemory(struct GDBStub* stub, const char* message) {
	const char* readAddress = message;
	unsigned i;
	for (i = 0; i < 8; ++i) {
		if (readAddress[i] == ',') {
			break;
		}
	}
	uint32_t address = _hex2int(readAddress, i);
	readAddress += i + 1;
	// TODO: expand this capacity
	for (i = 0; i < 1; ++i) {
		if (readAddress[i] == '#') {
			break;
		}
	}
	uint32_t size = _hex2int(readAddress, i);
	if (size > 4) {
		_error(stub, GDB_BAD_ARGUMENTS);
		return;
	}
	struct ARMMemory* memory = stub->d.memoryShim.original;
	int writeAddress = 0;
	for (i = 0; i < size; ++i, writeAddress += 2) {
		uint8_t byte = memory->load8(memory, address + i, 0);
		_int2hex8(byte, &stub->outgoing[writeAddress]);
	}
	stub->outgoing[writeAddress] = 0;
	_sendMessage(stub);
}

static void _readGPRs(struct GDBStub* stub, const char* message) {
	(void) (message);
	int r;
	int i = 0;
	for (r = 0; r < 16; ++r) {
		_int2hex32(stub->d.cpu->gprs[r], &stub->outgoing[i]);
		i += 8;
	}
	stub->outgoing[i] = 0;
	_sendMessage(stub);
}

static void _readRegister(struct GDBStub* stub, const char* message) {
	const char* readAddress = message;
	unsigned i;
	for (i = 0; i < 8; ++i) {
		if (readAddress[i] == '#') {
			break;
		}
	}
	uint32_t reg = _hex2int(readAddress, i);
	uint32_t value;
	if (reg < 0x10) {
		value = stub->d.cpu->gprs[reg];
	} else if (reg == 0x19) {
		value = stub->d.cpu->cpsr.packed;
	} else {
		stub->outgoing[0] = '\0';
		_sendMessage(stub);
		return;
	}
	_int2hex32(value, stub->outgoing);
	stub->outgoing[8] = 0;
	_sendMessage(stub);
}

size_t _parseGDBMessage(struct GDBStub* stub, const char* message) {
	uint8_t checksum = 0;
	int parsed = 1;
	printf("< %s\n", stub->line);
	switch (*message) {
	case '+':
		stub->lineAck = GDB_ACK_RECEIVED;
		return parsed;
	case '-':
		stub->lineAck = GDB_NAK_RECEIVED;
		return parsed;
	case '$':
		++message;
		break;
	default:
		_nak(stub);
		return parsed;
	}

	int i;
	char messageType = message[0];
	for (i = 0; message[i] && message[i] != '#'; ++i, ++parsed) {
		checksum += message[i];
	}
	if (!message[i]) {
		_nak(stub);
		return parsed;
	}
	++i;
	++parsed;
	if (!message[i]) {
		_nak(stub);
		return parsed;
	} else if (!message[i + 1]) {
		++parsed;
		_nak(stub);
		return parsed;
	}
	parsed += 2;
	int networkChecksum = _hex2int(&message[i], 2);
	if (networkChecksum != checksum) {
		printf("Checksum error: expected %02x, got %02x\n", checksum, networkChecksum);
		_nak(stub);
		return parsed;
	}

	_ack(stub);
	++message;
	switch (messageType) {
	case 'g':
		_readGPRs(stub, message);
		break;
	case 'm':
		_readMemory(stub, message);
		break;
	case 'p':
		_readRegister(stub, message);
		break;
	default:
		_error(stub, GDB_UNSUPPORTED_COMMAND);
		break;
	}
	return parsed;
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
		ssize_t position = 0;
		while (position < messageLen) {
			position += _parseGDBMessage(stub, &stub->line[position]);
		}
	}

connectionLost:
	// TODO: add logging support to the debugging interface
	printf("Connection lost\n");
	GDBStubHangup(stub);
}
