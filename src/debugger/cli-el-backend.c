/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/debugger/cli-el-backend.h>

#include <mgba/core/config.h>
#include <mgba/core/version.h>
#include <mgba-util/threading.h>
#include <mgba-util/vfs.h>

#include <signal.h>

struct CLIDebuggerEditLineBackend {
	struct CLIDebuggerBackend d;

	EditLine* elstate;
	History* histate;

	int count;
	const char* prompt;
	bool doPrompt;
	Thread promptThread;
	Mutex promptMutex;
	Condition promptRead;
	Condition promptWrite;
	bool exitThread;
};

static struct CLIDebugger* _activeDebugger;

static THREAD_ENTRY _promptThread(void*);

static char* _prompt(EditLine* el) {
	UNUSED(el);
	return "> ";
}

static void _breakIntoDefault(int signal) {
	UNUSED(signal);
	struct mDebuggerEntryInfo info = {
		.target = &_activeDebugger->d
	};
	mDebuggerEnter(_activeDebugger->d.p, DEBUGGER_ENTER_MANUAL, &info);
}

static unsigned char _tabComplete(EditLine* elstate, int ch) {
	UNUSED(ch);
	const LineInfo* li = el_line(elstate);
	if (!li->buffer[0]) {
		return CC_ERROR;
	}

	struct CLIDebuggerEditLineBackend* elbe;
	el_get(elstate, EL_CLIENTDATA, &elbe);
	// TODO: not always true
	if (CLIDebuggerTabComplete(elbe->d.p, li->buffer, true, li->cursor - li->buffer)) {
		return CC_REDISPLAY;
	}
	return CC_ERROR;
}

ATTRIBUTE_FORMAT(printf, 2, 3)
static void CLIDebuggerEditLinePrintf(struct CLIDebuggerBackend* be, const char* fmt, ...) {
	UNUSED(be);
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

static void CLIDebuggerEditLineInit(struct CLIDebuggerBackend* be) {
	struct CLIDebuggerEditLineBackend* elbe = (struct CLIDebuggerEditLineBackend*) be;
	// TODO: get argv[0]
	elbe->elstate = el_init(binaryName, stdin, stdout, stderr);
	el_set(elbe->elstate, EL_PROMPT, _prompt);
	el_set(elbe->elstate, EL_EDITOR, "emacs");

	el_set(elbe->elstate, EL_CLIENTDATA, elbe);
	el_set(elbe->elstate, EL_ADDFN, "tab-complete", "Tab completion", _tabComplete);
	el_set(elbe->elstate, EL_BIND, "\t", "tab-complete", 0);
	elbe->histate = history_init();
	HistEvent ev;
	history(elbe->histate, &ev, H_SETSIZE, 200);
	el_set(elbe->elstate, EL_HIST, history, elbe->histate);

	char path[PATH_MAX + 1];
	mCoreConfigDirectory(path, PATH_MAX);
	if (path[0]) {
		strncat(path, PATH_SEP, PATH_MAX);
		strncat(path, "cli_history.log", PATH_MAX);
		struct VFile* vf = VFileOpen(path, O_RDONLY);
		if (vf) {
			char line[512];
			while (vf->readline(vf, line, sizeof(line)) > 0) {
				history(elbe->histate, &ev, H_ENTER, line);
			}
			vf->close(vf);
		}
	}

	MutexInit(&elbe->promptMutex);
	ConditionInit(&elbe->promptRead);
	ConditionInit(&elbe->promptWrite);
	elbe->prompt = NULL;
	elbe->exitThread = false;
	elbe->doPrompt = false;
	ThreadCreate(&elbe->promptThread, _promptThread, elbe);

	_activeDebugger = be->p;
	signal(SIGINT, _breakIntoDefault);
}

static void CLIDebuggerEditLineDeinit(struct CLIDebuggerBackend* be) {
	struct CLIDebuggerEditLineBackend* elbe = (struct CLIDebuggerEditLineBackend*) be;
	MutexLock(&elbe->promptMutex);
	elbe->exitThread = true;
	ConditionWake(&elbe->promptWrite);
	MutexUnlock(&elbe->promptMutex);
	ThreadJoin(&elbe->promptThread);

	char path[PATH_MAX + 1];
	mCoreConfigDirectory(path, PATH_MAX);
	if (path[0]) {
		strncat(path, PATH_SEP, PATH_MAX);
		strncat(path, "cli_history.log", PATH_MAX);
		struct VFile* vf = VFileOpen(path, O_WRONLY | O_CREAT | O_TRUNC);
		if (vf) {
			HistEvent ev = {0};
			if (history(elbe->histate, &ev, H_FIRST) >= 0) {
				do {
					if (!ev.str || ev.str[0] == '\n') {
						continue;
					}
					vf->write(vf, ev.str, strlen(ev.str));
				} while (history(elbe->histate, &ev, H_NEXT) >= 0);
			}
			vf->close(vf);
		}
	}
	history_end(elbe->histate);
	el_end(elbe->elstate);
	free(elbe);
}

static THREAD_ENTRY _promptThread(void* context) {
	struct CLIDebuggerEditLineBackend* elbe = context;

	MutexLock(&elbe->promptMutex);
	while (!elbe->exitThread) {
		if (elbe->doPrompt) {
			MutexUnlock(&elbe->promptMutex);
			elbe->prompt = el_gets(elbe->elstate, &elbe->count);
			MutexLock(&elbe->promptMutex);
			elbe->doPrompt = false;
			ConditionWake(&elbe->promptRead);
		}
		ConditionWait(&elbe->promptWrite, &elbe->promptMutex);
	}
	MutexUnlock(&elbe->promptMutex);
	THREAD_EXIT(0);
}

static int CLIDebuggerEditLinePoll(struct CLIDebuggerBackend* be, int32_t timeoutMs) {
	struct CLIDebuggerEditLineBackend* elbe = (struct CLIDebuggerEditLineBackend*) be;
	int gotPrompt = 0;
	MutexLock(&elbe->promptMutex);
	if (!elbe->prompt) {
		elbe->doPrompt = true;
		ConditionWake(&elbe->promptWrite);
		ConditionWaitTimed(&elbe->promptRead, &elbe->promptMutex, timeoutMs);
	}
	if (elbe->prompt) {
		gotPrompt = 1;
	}
	MutexUnlock(&elbe->promptMutex);

	return gotPrompt;
}

static const char* CLIDebuggerEditLineReadLine(struct CLIDebuggerBackend* be, size_t* len) {
	struct CLIDebuggerEditLineBackend* elbe = (struct CLIDebuggerEditLineBackend*) be;
	*len = 0;
	if (CLIDebuggerEditLinePoll(be, -1) != 1) {
		return NULL;
	}
	MutexLock(&elbe->promptMutex);
	int count = elbe->count;
	const char* line = elbe->prompt;
	elbe->prompt = NULL;
	MutexUnlock(&elbe->promptMutex);
	if (line) {
		if (count > 1) {
			// Crop off newline
			*len = (size_t) count - 1;
		} else if (count == 1) {
			*len = 1;
		}
	}
	return line;
}

static void CLIDebuggerEditLineLineAppend(struct CLIDebuggerBackend* be, const char* line) {
	struct CLIDebuggerEditLineBackend* elbe = (struct CLIDebuggerEditLineBackend*) be;
	el_insertstr(elbe->elstate, line);
}

static const char* CLIDebuggerEditLineHistoryLast(struct CLIDebuggerBackend* be, size_t* len) {
	struct CLIDebuggerEditLineBackend* elbe = (struct CLIDebuggerEditLineBackend*) be;
	HistEvent ev;
	if (history(elbe->histate, &ev, H_FIRST) < 0) {
		*len = 0;
		return NULL;
	}
	const char* newline = strchr(ev.str, '\n');
	if (!newline) {
		*len = strlen(ev.str);
	} else {
		*len = newline - ev.str;
	}

	return ev.str;
}

static void CLIDebuggerEditLineHistoryAppend(struct CLIDebuggerBackend* be, const char* line) {
	struct CLIDebuggerEditLineBackend* elbe = (struct CLIDebuggerEditLineBackend*) be;
	HistEvent ev;
	history(elbe->histate, &ev, H_ENTER, line);
}

struct CLIDebuggerBackend* CLIDebuggerEditLineBackendCreate(void) {
	struct CLIDebuggerEditLineBackend* elbe = calloc(1, sizeof(*elbe));
	elbe->d.printf = CLIDebuggerEditLinePrintf;
	elbe->d.init = CLIDebuggerEditLineInit;
	elbe->d.deinit = CLIDebuggerEditLineDeinit;
	elbe->d.poll = CLIDebuggerEditLinePoll;
	elbe->d.readline = CLIDebuggerEditLineReadLine;
	elbe->d.lineAppend = CLIDebuggerEditLineLineAppend;
	elbe->d.historyLast = CLIDebuggerEditLineHistoryLast;
	elbe->d.historyAppend = CLIDebuggerEditLineHistoryAppend;
	elbe->d.interrupt = NULL;
	return &elbe->d;
}
