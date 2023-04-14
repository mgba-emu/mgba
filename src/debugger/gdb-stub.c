/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/debugger/gdb-stub.h>

#include <mgba/core/core.h>
#include <mgba/internal/arm/debugger/debugger.h>
#include <mgba/internal/arm/isa-inlines.h>
#include <mgba/internal/gba/memory.h>
#include <mgba-util/string.h>

#include <signal.h>

#ifndef SIGTRAP
#define SIGTRAP 5 /* Win32 Signals do not include SIGTRAP */
#endif

#define SOCKET_TIMEOUT 50

enum GDBError {
	GDB_NO_ERROR = 0x00,
	GDB_BAD_ARGUMENTS = 0x06,
	GDB_UNSUPPORTED_COMMAND = 0x07
};

enum {
	MACH_O_ARM = 12,
	MACH_O_ARM_V4T = 5
};

static const char* TARGET_XML = "<target version=\"1.0\">"
                                "<architecture>armv4t</architecture>"
                                "<osabi>none</osabi>"
                                "<feature name=\"org.gnu.gdb.arm.core\">"
                                "<reg name=\"r0\" bitsize=\"32\" type=\"uint32\"/>"
                                "<reg name=\"r1\" bitsize=\"32\" type=\"uint32\"/>"
                                "<reg name=\"r2\" bitsize=\"32\" type=\"uint32\"/>"
                                "<reg name=\"r3\" bitsize=\"32\" type=\"uint32\"/>"
                                "<reg name=\"r4\" bitsize=\"32\" type=\"uint32\"/>"
                                "<reg name=\"r5\" bitsize=\"32\" type=\"uint32\"/>"
                                "<reg name=\"r6\" bitsize=\"32\" type=\"uint32\"/>"
                                "<reg name=\"r7\" bitsize=\"32\" type=\"uint32\"/>"
                                "<reg name=\"r8\" bitsize=\"32\" type=\"uint32\"/>"
                                "<reg name=\"r9\" bitsize=\"32\" type=\"uint32\"/>"
                                "<reg name=\"r10\" bitsize=\"32\" type=\"uint32\"/>"
                                "<reg name=\"r11\" bitsize=\"32\" type=\"uint32\"/>"
                                "<reg name=\"r12\" bitsize=\"32\" type=\"uint32\"/>"
                                "<reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>"
                                "<reg name=\"lr\" bitsize=\"32\"/>"
                                "<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>"
                                "<flags id=\"cpsr_flags\" size=\"4\">"
                                "<field name=\"N\" start=\"31\" end=\"31\"/>"
                                "<field name=\"Z\" start=\"30\" end=\"30\"/>"
                                "<field name=\"C\" start=\"29\" end=\"29\"/>"
                                "<field name=\"V\" start=\"28\" end=\"28\"/>"
                                "<field name=\"I\" start=\"7\" end=\"7\"/>"
                                "<field name=\"F\" start=\"6\" end=\"6\"/>"
                                "<field name=\"T\" start=\"5\" end=\"5\"/>"
                                "<field name=\"M\" start=\"0\" end=\"4\"/>"
                                "</flags>"
                                "<reg name=\"cpsr\" bitsize=\"32\" regnum=\"25\" type=\"cpsr_flags\"/>"
                                "</feature>"
                                "</target>";

static void _sendMessage(struct GDBStub* stub);

static void _gdbStubDeinit(struct mDebugger* debugger) {
	struct GDBStub* stub = (struct GDBStub*) debugger;
	if (!SOCKET_FAILED(stub->socket)) {
		GDBStubShutdown(stub);
	}
}

static void _gdbStubEntered(struct mDebugger* debugger, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	struct GDBStub* stub = (struct GDBStub*) debugger;
	switch (reason) {
	case DEBUGGER_ENTER_MANUAL:
		snprintf(stub->outgoing, GDB_STUB_MAX_LINE - 4, "S%02x", SIGINT);
		break;
	case DEBUGGER_ENTER_BREAKPOINT:
		if (stub->supportsHwbreak && stub->supportsSwbreak && info) {
			snprintf(stub->outgoing, GDB_STUB_MAX_LINE - 4, "T%02x%cwbreak:;", SIGTRAP, info->type.bp.breakType == BREAKPOINT_SOFTWARE ? 's' : 'h');
		} else {
			snprintf(stub->outgoing, GDB_STUB_MAX_LINE - 4, "S%02xk", SIGTRAP);
		}
		break;
	case DEBUGGER_ENTER_WATCHPOINT:
		if (info) {
			const char* type = 0;

			if (stub->watchpointsBehavior != GDB_WATCHPOINT_STANDARD_LOGIC && info->type.wp.watchType & WATCHPOINT_WRITE) {
				// We send S05 instead of T05watch because it bypasses GDB's internal logic to check if
				// the value changed and to bypass a step into by GDB. This allows to control the change
				// logic even when using savestates which we already handle in the core debugger logic
				snprintf(stub->outgoing, GDB_STUB_MAX_LINE - 4, "S%02x", SIGTRAP);
				_sendMessage(stub);
				return;
			}

			switch (info->type.wp.watchType) {
			case WATCHPOINT_WRITE:
			case WATCHPOINT_WRITE_CHANGE:
				type = "watch";
				break;
			case WATCHPOINT_READ:
				type = "rwatch";
				break;
			case WATCHPOINT_RW:
				type = "awatch";
				break;
			case WATCHPOINT_CHANGE:
				break;
			}
			snprintf(stub->outgoing, GDB_STUB_MAX_LINE - 4, "T%02x%s:%08x;", SIGTRAP, type, info->address);
		} else {
			snprintf(stub->outgoing, GDB_STUB_MAX_LINE - 4, "S%02x", SIGTRAP);
		}
		break;
	case DEBUGGER_ENTER_ILLEGAL_OP:
		snprintf(stub->outgoing, GDB_STUB_MAX_LINE - 4, "S%02x", SIGILL);
		break;
	case DEBUGGER_ENTER_ATTACHED:
	case DEBUGGER_ENTER_STACK:
		return;
	}
	_sendMessage(stub);
}

static void _gdbStubPoll(struct mDebugger* debugger) {
	struct GDBStub* stub = (struct GDBStub*) debugger;
	--stub->untilPoll;
	if (stub->untilPoll > 0) {
		return;
	}
	stub->untilPoll = GDB_STUB_INTERVAL;
	stub->shouldBlock = false;
	GDBStubUpdate(stub);
}

static void _gdbStubWait(struct mDebugger* debugger) {
	struct GDBStub* stub = (struct GDBStub*) debugger;
	stub->shouldBlock = true;
	GDBStubUpdate(stub);
}

static void _gdbStubUpdate(struct mDebugger* debugger) {
	struct GDBStub* stub = (struct GDBStub*) debugger;
	stub->shouldBlock = false;
	GDBStubUpdate(stub);
}

static void _ack(struct GDBStub* stub) {
	char ack = '+';
	SocketSend(stub->connection, &ack, 1);
}

static void _nak(struct GDBStub* stub) {
	char nak = '-';
	mLOG(DEBUGGER, WARN, "Packet error");
	SocketSend(stub->connection, &nak, 1);
}

static uint32_t _hex2int(const char* hex, int maxDigits) {
	uint32_t value = 0;
	uint8_t letter;

	while (maxDigits--) {
		letter = *hex - '0';
		if (letter > 9) {
			letter = *hex - 'a';
			if (letter > 5) {
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

static uint32_t _readHex(const char* in, unsigned* out) {
	unsigned i;
	for (i = 0; i < 8; ++i) {
		if (in[i] == ',' || in[i] == ':' || in[i] == '=') {
			break;
		}
	}
	*out += i;
	return _hex2int(in, i);
}

static void _sendMessage(struct GDBStub* stub) {
	if (stub->lineAck != GDB_ACK_OFF) {
		stub->lineAck = GDB_ACK_PENDING;
	}
	uint8_t checksum = 0;
	int i = 1;
	char buffer = stub->outgoing[0];
	char swap;
	stub->outgoing[0] = '$';
	if (buffer) {
		for (; i < GDB_STUB_MAX_LINE - 5; ++i) {
			checksum += buffer;
			swap = stub->outgoing[i];
			stub->outgoing[i] = buffer;
			buffer = swap;
			if (!buffer) {
				++i;
				break;
			}
		}
	}
	stub->outgoing[i] = '#';
	_int2hex8(checksum, &stub->outgoing[i + 1]);
	stub->outgoing[i + 3] = 0;
	mLOG(DEBUGGER, DEBUG, "> %s", stub->outgoing);
	SocketSend(stub->connection, stub->outgoing, i + 3);
}

static void _error(struct GDBStub* stub, enum GDBError error) {
	snprintf(stub->outgoing, GDB_STUB_MAX_LINE - 4, "E%02x", error);
	_sendMessage(stub);
}

static void _writeHostInfo(struct GDBStub* stub) {
	snprintf(stub->outgoing, GDB_STUB_MAX_LINE - 4, "cputype:%u;cpusubtype:%u:ostype:none;vendor:none;endian:little;ptrsize:4;", MACH_O_ARM, MACH_O_ARM_V4T);
	_sendMessage(stub);
}

static void _continue(struct GDBStub* stub, const char* message) {
	stub->d.state = DEBUGGER_CALLBACK;
	stub->untilPoll = GDB_STUB_INTERVAL;
	// TODO: parse message
	UNUSED(message);
}

static void _step(struct GDBStub* stub, const char* message) {
	stub->d.core->step(stub->d.core);
	snprintf(stub->outgoing, GDB_STUB_MAX_LINE - 4, "S%02x", SIGTRAP);
	_sendMessage(stub);
	// TODO: parse message
	UNUSED(message);
}

static void _writeMemoryBinary(struct GDBStub* stub, const char* message) {
	const char* readAddress = message;
	unsigned i = 0;
	uint32_t address = _readHex(readAddress, &i);
	readAddress += i + 1;

	i = 0;
	uint32_t size = _readHex(readAddress, &i);
	readAddress += i + 1;

	if (size > 512) {
		_error(stub, GDB_BAD_ARGUMENTS);
		return;
	}

	struct ARMCore* cpu = stub->d.core->cpu;
	for (i = 0; i < size; i++) {
		uint8_t byte = *readAddress;
		++readAddress;

		// Parse escape char
		if (byte == 0x7D) {
			byte = *readAddress ^ 0x20;
			++readAddress;
		}

		GBAPatch8(cpu, address + i, byte, 0);
	}

	strncpy(stub->outgoing, "OK", GDB_STUB_MAX_LINE - 4);
	_sendMessage(stub);
}


static void _writeMemory(struct GDBStub* stub, const char* message) {
	const char* readAddress = message;
	unsigned i = 0;
	uint32_t address = _readHex(readAddress, &i);
	readAddress += i + 1;

	i = 0;
	uint32_t size = _readHex(readAddress, &i);
	readAddress += i + 1;

	if (size > 512) {
		_error(stub, GDB_BAD_ARGUMENTS);
		return;
	}

	struct ARMCore* cpu = stub->d.core->cpu;
	for (i = 0; i < size; ++i, readAddress += 2) {
		uint8_t byte = _hex2int(readAddress, 2);
		GBAPatch8(cpu, address + i, byte, 0);
	}

	strncpy(stub->outgoing, "OK", GDB_STUB_MAX_LINE - 4);
	_sendMessage(stub);
}

static void _readMemory(struct GDBStub* stub, const char* message) {
	const char* readAddress = message;
	unsigned i = 0;
	uint32_t address = _readHex(readAddress, &i);
	readAddress += i + 1;
	uint32_t size = _readHex(readAddress, &i);
	if (size > 512) {
		_error(stub, GDB_BAD_ARGUMENTS);
		return;
	}
	struct ARMCore* cpu = stub->d.core->cpu;
	int writeAddress = 0;
	for (i = 0; i < size; ++i, writeAddress += 2) {
		uint8_t byte = cpu->memory.load8(cpu, address + i, 0);
		_int2hex8(byte, &stub->outgoing[writeAddress]);
	}
	stub->outgoing[writeAddress] = 0;
	_sendMessage(stub);
}

static void _writeGPRs(struct GDBStub* stub, const char* message) {
	struct ARMCore* cpu = stub->d.core->cpu;
	const char* readAddress = message;

	int r;
	for (r = 0; r <= ARM_PC; ++r) {
		cpu->gprs[r] = _hex2int(readAddress, 8);
		readAddress += 8;
	}
	if (cpu->executionMode == MODE_ARM) {
		ARMWritePC(cpu);
	} else {
		ThumbWritePC(cpu);
	}

	strncpy(stub->outgoing, "OK", GDB_STUB_MAX_LINE - 4);
	_sendMessage(stub);
}

static void _readGPRs(struct GDBStub* stub, const char* message) {
	struct ARMCore* cpu = stub->d.core->cpu;
	UNUSED(message);
	int r;
	int i = 0;

	// General purpose registers
	for (r = 0; r < ARM_PC; ++r) {
		_int2hex32(cpu->gprs[r], &stub->outgoing[i]);
		i += 8;
	}

	// Program counter
	_int2hex32(cpu->gprs[ARM_PC] - (cpu->cpsr.t ? WORD_SIZE_THUMB : WORD_SIZE_ARM), &stub->outgoing[i]);
	i += 8;

	// CPU status
	_int2hex32(cpu->cpsr.packed, &stub->outgoing[i]);
	i += 8;

	stub->outgoing[i] = 0;
	_sendMessage(stub);
}

static void _writeRegister(struct GDBStub* stub, const char* message) {
	struct ARMCore* cpu = stub->d.core->cpu;
	const char* readAddress = message;

	unsigned i = 0;
	uint32_t reg = _readHex(readAddress, &i);
	readAddress += i + 1;

	uint32_t value = _readHex(readAddress, &i);

	LOAD_32BE(value, 0, &value);

	if (reg <= ARM_PC) {
		cpu->gprs[reg] = value;
		if (reg == ARM_PC) {
			if (cpu->executionMode == MODE_ARM) {
				ARMWritePC(cpu);
			} else {
				ThumbWritePC(cpu);
			}
		}
	} else if (reg == 0x19) {
		cpu->cpsr.packed = value;
	} else {
		stub->outgoing[0] = '\0';
		_sendMessage(stub);
		return;
	}

	strncpy(stub->outgoing, "OK", GDB_STUB_MAX_LINE - 4);
	_sendMessage(stub);
}

static void _readRegister(struct GDBStub* stub, const char* message) {
	struct ARMCore* cpu = stub->d.core->cpu;
	const char* readAddress = message;
	unsigned i = 0;
	uint32_t reg = _readHex(readAddress, &i);
	uint32_t value;
	if (reg < 0x10) {
		value = cpu->gprs[reg];
	} else if (reg == 0x19) {
		value = cpu->cpsr.packed;
	} else {
		stub->outgoing[0] = '\0';
		_sendMessage(stub);
		return;
	}
	_int2hex32(value, stub->outgoing);
	stub->outgoing[8] = '\0';
	_sendMessage(stub);
}

static void _processQSupportedCommand(struct GDBStub* stub, const char* message) {
	const char* terminator = strrchr(message, '#');
	stub->supportsSwbreak = false;
	stub->supportsHwbreak = false;
	while (message < terminator) {
		const char* end = strchr(message, ';');
		size_t len;
		if (end && end < terminator) {
			len = end - message;
		} else {
			len = terminator - message;
		}
		if (!strncmp(message, "swbreak+", len)) {
			stub->supportsSwbreak = true;
		} else if (!strncmp(message, "hwbreak+", len)) {
			stub->supportsHwbreak = true;
		} else if (!strncmp(message, "swbreak-", len)) {
			stub->supportsSwbreak = false;
		} else if (!strncmp(message, "hwbreak-", len)) {
			stub->supportsHwbreak = false;
		}
		if (!end) {
			break;
		}
		message = end + 1;
	}
	strncpy(stub->outgoing, "swbreak+;hwbreak+;qXfer:features:read+;qXfer:memory-map:read+", GDB_STUB_MAX_LINE - 4);
}

static void _processQXferCommand(struct GDBStub* stub, const char* params, const char* data) {
	unsigned offset = 0;
	unsigned length = 0;

	unsigned index = 0;
	for (index = 0; params[index] != ','; ++index) {
		offset <<= 4;
		offset |= _hex2int(&params[index], 1);
	}

	++index;
	unsigned int paramsLength = strlen(params);
	for (; index + 3 < paramsLength; ++index) {
		length <<= 4;
		length |= _hex2int(&params[index], 1);
	}

	length += 1;

	if (length + 4 > GDB_STUB_MAX_LINE) {
		length = GDB_STUB_MAX_LINE - 4;
	}

	if (strlen(data) < length + offset) {
		length = strlen(data) - offset + 1;
		stub->outgoing[0] = 'l';
	} else {
		stub->outgoing[0] = 'm';
	}
	strlcpy(&stub->outgoing[1], &data[offset], length);
}

static void _generateMemoryMapXml(struct GDBStub* stub, char* memoryMap) {
	size_t index = 0;
	strncpy(memoryMap, "<memory-map version=\"1.0\">", 27);
	index += strlen(memoryMap);
	const struct mCoreMemoryBlock* blocks;
	size_t nBlocks = stub->d.core->listMemoryBlocks(stub->d.core, &blocks);
	size_t i;
	for (i = 0; i < nBlocks; ++i) {
		if (!(blocks[i].flags & mCORE_MEMORY_MAPPED)) {
			continue;
		}

		const char* type = blocks[i].flags & (mCORE_MEMORY_WRITE | mCORE_MEMORY_WORM) ? "ram" : "rom";
		index += snprintf(&memoryMap[index], GDB_STUB_MAX_LINE - index, "<memory type=\"%s\" start=\"0x%08x\" length=\"0x%08x\"/>", type, blocks[i].start, blocks[i].size);
	}
	int amountLeft = GDB_STUB_MAX_LINE - index;
	strncpy(&memoryMap[index], "</memory-map>", amountLeft);
}

static void _processQReadCommand(struct GDBStub* stub, const char* message) {
	stub->outgoing[0] = '\0';
	if (!strncmp("HostInfo#", message, 9)) {
		_writeHostInfo(stub);
		return;
	}
	if (!strncmp("Attached#", message, 9)) {
		strncpy(stub->outgoing, "1", GDB_STUB_MAX_LINE - 4);
	} else if (!strncmp("VAttachOrWaitSupported#", message, 23)) {
		strncpy(stub->outgoing, "OK", GDB_STUB_MAX_LINE - 4);
	} else if (!strncmp("C#", message, 2)) {
		strncpy(stub->outgoing, "QC1", GDB_STUB_MAX_LINE - 4);
	} else if (!strncmp("fThreadInfo#", message, 12)) {
		strncpy(stub->outgoing, "m1", GDB_STUB_MAX_LINE - 4);
	} else if (!strncmp("sThreadInfo#", message, 12)) {
		strncpy(stub->outgoing, "l", GDB_STUB_MAX_LINE - 4);
	} else if (!strncmp("Xfer:features:read:target.xml:", message, 30)) {
		_processQXferCommand(stub, message + 30, TARGET_XML);
	} else if (!strncmp("Xfer:memory-map:read::", message, 22)) {
		if (strlen(stub->memoryMapXml) == 0) {
			_generateMemoryMapXml(stub, stub->memoryMapXml);
		}
		_processQXferCommand(stub, message + 22, stub->memoryMapXml);
	} else if (!strncmp("Supported:", message, 10)) {
		_processQSupportedCommand(stub, message + 10);
	}
	_sendMessage(stub);
}

static void _processQWriteCommand(struct GDBStub* stub, const char* message) {
	stub->outgoing[0] = '\0';
	if (!strncmp("StartNoAckMode#", message, 16)) {
		stub->lineAck = GDB_ACK_OFF;
		strncpy(stub->outgoing, "OK", GDB_STUB_MAX_LINE - 4);
	}
	_sendMessage(stub);
}

static void _processVWriteCommand(struct GDBStub* stub, const char* message) {
	UNUSED(message);
	stub->outgoing[0] = '\0';
	_sendMessage(stub);
}

static void _processVReadCommand(struct GDBStub* stub, const char* message) {
	stub->outgoing[0] = '\0';
	if (!strncmp("Attach", message, 6)) {
		strncpy(stub->outgoing, "1", GDB_STUB_MAX_LINE - 4);
		mDebuggerEnter(&stub->d, DEBUGGER_ENTER_MANUAL, 0);
	}
	_sendMessage(stub);
}

static void _setBreakpoint(struct GDBStub* stub, const char* message) {
	const char* readAddress = &message[2];
	unsigned i = 0;
	uint32_t address = _readHex(readAddress, &i);
	readAddress += i + 1;

	struct mBreakpoint breakpoint = {
		.address = address,
		.type = BREAKPOINT_HARDWARE
	};
	struct mWatchpoint watchpoint = {
		.minAddress = address,
		.maxAddress = address + 1
	};

	switch (message[0]) {
	case '0':
	case '1':
		stub->d.platform->setBreakpoint(stub->d.platform, &breakpoint);
		break;
	case '2':
		watchpoint.type = stub->watchpointsBehavior == GDB_WATCHPOINT_OVERRIDE_LOGIC_ANY_WRITE ? WATCHPOINT_WRITE : WATCHPOINT_WRITE_CHANGE;
		stub->d.platform->setWatchpoint(stub->d.platform, &watchpoint);
		break;
	case '3':
		watchpoint.type = WATCHPOINT_READ;
		stub->d.platform->setWatchpoint(stub->d.platform, &watchpoint);
		break;
	case '4':
		watchpoint.type = WATCHPOINT_RW;
		stub->d.platform->setWatchpoint(stub->d.platform, &watchpoint);
		break;
	default:
		stub->outgoing[0] = '\0';
		_sendMessage(stub);
		return;
	}
	strncpy(stub->outgoing, "OK", GDB_STUB_MAX_LINE - 4);
	_sendMessage(stub);
}

static void _clearBreakpoint(struct GDBStub* stub, const char* message) {
	const char* readAddress = &message[2];
	unsigned i = 0;
	uint32_t address = _readHex(readAddress, &i);
	struct mBreakpointList breakpoints;
	struct mWatchpointList watchpoints;

	size_t index;
	switch (message[0]) {
	case '0':
	case '1':
		mBreakpointListInit(&breakpoints, 0);
		stub->d.platform->listBreakpoints(stub->d.platform, &breakpoints);
		for (index = 0; index < mBreakpointListSize(&breakpoints); ++index) {
			if (mBreakpointListGetPointer(&breakpoints, index)->address != address) {
				continue;
			}
			stub->d.platform->clearBreakpoint(stub->d.platform, mBreakpointListGetPointer(&breakpoints, index)->id);
		}
		mBreakpointListDeinit(&breakpoints);
		break;
	case '2':
	case '3':
	case '4':
		mWatchpointListInit(&watchpoints, 0);
		stub->d.platform->listWatchpoints(stub->d.platform, &watchpoints);
		for (index = 0; index < mWatchpointListSize(&watchpoints); ++index) {
			struct mWatchpoint* watchpoint = mWatchpointListGetPointer(&watchpoints, index);
			if (address >= watchpoint->minAddress && address < watchpoint->maxAddress) {
				continue;
			}
			stub->d.platform->clearBreakpoint(stub->d.platform, watchpoint->id);
		}
		mWatchpointListDeinit(&watchpoints);
		break;
	default:
		break;
	}
	strncpy(stub->outgoing, "OK", GDB_STUB_MAX_LINE - 4);
	_sendMessage(stub);
}

size_t _parseGDBMessage(struct GDBStub* stub, const char* message) {
	uint8_t checksum = 0;
	int parsed = 1;
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
	case '\x03':
		mDebuggerEnter(&stub->d, DEBUGGER_ENTER_MANUAL, 0);
		return parsed;
	default:
		_nak(stub);
		return parsed;
	}

	int i;
	char messageType = message[0];
	for (i = 0; message[i] != '#'; ++i, ++parsed) {
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
		mLOG(DEBUGGER, WARN, "Checksum error: expected %02x, got %02x", checksum, networkChecksum);
		_nak(stub);
		return parsed;
	}

	_ack(stub);
	++message;
	switch (messageType) {
	case '?':
		snprintf(stub->outgoing, GDB_STUB_MAX_LINE - 4, "S%02x", SIGINT);
		_sendMessage(stub);
		break;
	case 'c':
		_continue(stub, message);
		break;
	case 'G':
		_writeGPRs(stub, message);
		break;
	case 'g':
		_readGPRs(stub, message);
		break;
	case 'H':
	case 'T':
		// This is faked because we only have one thread
		strncpy(stub->outgoing, "OK", GDB_STUB_MAX_LINE - 4);
		_sendMessage(stub);
		break;
	case 'M':
		_writeMemory(stub, message);
		break;
	case 'm':
		_readMemory(stub, message);
		break;
	case 'P':
		_writeRegister(stub, message);
		break;
	case 'p':
		_readRegister(stub, message);
		break;
	case 'Q':
		_processQWriteCommand(stub, message);
		break;
	case 'q':
		_processQReadCommand(stub, message);
		break;
	case 's':
		_step(stub, message);
		break;
	case 'V':
		_processVWriteCommand(stub, message);
		break;
	case 'v':
		_processVReadCommand(stub, message);
		break;
	case 'X':
		_writeMemoryBinary(stub, message);
		break;
	case 'Z':
		_setBreakpoint(stub, message);
		break;
	case 'z':
		_clearBreakpoint(stub, message);
		break;
	default:
		_error(stub, GDB_UNSUPPORTED_COMMAND);
		break;
	}
	return parsed;
}

void GDBStubCreate(struct GDBStub* stub) {
	stub->socket = INVALID_SOCKET;
	stub->connection = INVALID_SOCKET;
	stub->d.init = NULL;
	stub->d.deinit = _gdbStubDeinit;
	stub->d.paused = _gdbStubWait;
	stub->d.update = _gdbStubUpdate;
	stub->d.entered = _gdbStubEntered;
	stub->d.custom = _gdbStubPoll;
	stub->d.interrupt = NULL;
	stub->d.type = DEBUGGER_GDB;
	stub->untilPoll = GDB_STUB_INTERVAL;
	stub->lineAck = GDB_ACK_PENDING;
	stub->shouldBlock = false;
}

bool GDBStubListen(struct GDBStub* stub, int port, const struct Address* bindAddress, enum GDBWatchpointsBehvaior watchpointsBehavior) {
	if (!SOCKET_FAILED(stub->socket)) {
		GDBStubShutdown(stub);
	}
	stub->socket = SocketOpenTCP(port, bindAddress);
	if (SOCKET_FAILED(stub->socket)) {
		mLOG(DEBUGGER, ERROR, "Couldn't open socket");
		return false;
	}
	if (!SocketSetBlocking(stub->socket, false)) {
		goto cleanup;
	}
	int err = SocketListen(stub->socket, 1);
	if (err) {
		goto cleanup;
	}

	stub->watchpointsBehavior = watchpointsBehavior;
	memset(stub->memoryMapXml, 0, GDB_STUB_MAX_LINE);

	return true;

cleanup:
	mLOG(DEBUGGER, ERROR, "Couldn't listen on port");
	SocketClose(stub->socket);
	stub->socket = INVALID_SOCKET;
	return false;
}

void GDBStubHangup(struct GDBStub* stub) {
	if (!SOCKET_FAILED(stub->connection)) {
		SocketClose(stub->connection);
		stub->connection = INVALID_SOCKET;
	}
	if (stub->d.state == DEBUGGER_PAUSED) {
		stub->d.state = DEBUGGER_RUNNING;
	}
}

void GDBStubShutdown(struct GDBStub* stub) {
	GDBStubHangup(stub);
	if (!SOCKET_FAILED(stub->socket)) {
		SocketClose(stub->socket);
		stub->socket = INVALID_SOCKET;
	}
}

void GDBStubUpdate(struct GDBStub* stub) {
	if (stub->socket == INVALID_SOCKET) {
		if (stub->d.state == DEBUGGER_PAUSED) {
			stub->d.state = DEBUGGER_RUNNING;
		}
		return;
	}
	if (stub->connection == INVALID_SOCKET) {
		if (stub->shouldBlock) {
			Socket reads = stub->socket;
			SocketPoll(1, &reads, 0, 0, SOCKET_TIMEOUT);
		}
		stub->connection = SocketAccept(stub->socket, 0);
		if (!SOCKET_FAILED(stub->connection)) {
			if (!SocketSetBlocking(stub->connection, false)) {
				goto connectionLost;
			}
			mDebuggerEnter(&stub->d, DEBUGGER_ENTER_ATTACHED, 0);
		} else if (SocketWouldBlock()) {
			return;
		} else {
			goto connectionLost;
		}
		SocketSetTCPPush(stub->connection, 1);
	}
	while (true) {
		if (stub->shouldBlock) {
			Socket reads = stub->connection;
			SocketPoll(1, &reads, 0, 0, SOCKET_TIMEOUT);
		}
		ssize_t messageLen = SocketRecv(stub->connection, stub->line, GDB_STUB_MAX_LINE - 1);
		if (messageLen == 0) {
			goto connectionLost;
		}
		if (messageLen == -1) {
			if (SocketWouldBlock()) {
				return;
			}
			goto connectionLost;
		}
		stub->line[messageLen] = '\0';
		mLOG(DEBUGGER, DEBUG, "< %s", stub->line);
		ssize_t position = 0;
		while (position < messageLen) {
			position += _parseGDBMessage(stub, &stub->line[position]);
		}
	}

connectionLost:
	mLOG(DEBUGGER, WARN, "Connection lost");
	GDBStubHangup(stub);
}
