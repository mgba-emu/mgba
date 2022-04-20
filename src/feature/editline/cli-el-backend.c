/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cli-el-backend.h"

#include <mgba/core/config.h>
#include <mgba/core/version.h>
#include <mgba-util/vfs.h>

#include <signal.h>

static struct CLIDebugger* _activeDebugger;

static char* _prompt(EditLine* el) {
	UNUSED(el);
	return "> ";
}

static void _breakIntoDefault(int signal) {
	UNUSED(signal);
	mDebuggerEnter(&_activeDebugger->d, DEBUGGER_ENTER_MANUAL, 0);
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
void _CLIDebuggerEditLinePrintf(struct CLIDebuggerBackend* be, const char* fmt, ...) {
	UNUSED(be);
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

void _CLIDebuggerEditLineInit(struct CLIDebuggerBackend* be) {
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

	_activeDebugger = be->p;
	signal(SIGINT, _breakIntoDefault);
}

void _CLIDebuggerEditLineDeinit(struct CLIDebuggerBackend* be) {
	struct CLIDebuggerEditLineBackend* elbe = (struct CLIDebuggerEditLineBackend*) be;
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

const char* _CLIDebuggerEditLineReadLine(struct CLIDebuggerBackend* be, size_t* len) {
	struct CLIDebuggerEditLineBackend* elbe = (struct CLIDebuggerEditLineBackend*) be;
	int count;
	*len = 0;
	const char* line = el_gets(elbe->elstate, &count);
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
void _CLIDebuggerEditLineLineAppend(struct CLIDebuggerBackend* be, const char* line) {
	struct CLIDebuggerEditLineBackend* elbe = (struct CLIDebuggerEditLineBackend*) be;
	el_insertstr(elbe->elstate, line);
}

const char* _CLIDebuggerEditLineHistoryLast(struct CLIDebuggerBackend* be, size_t* len) {
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

void _CLIDebuggerEditLineHistoryAppend(struct CLIDebuggerBackend* be, const char* line) {
	struct CLIDebuggerEditLineBackend* elbe = (struct CLIDebuggerEditLineBackend*) be;
	HistEvent ev;
	history(elbe->histate, &ev, H_ENTER, line);
}

struct CLIDebuggerBackend* CLIDebuggerEditLineBackendCreate(void) {
	struct CLIDebuggerEditLineBackend* elbe = calloc(1, sizeof(*elbe));
	elbe->d.printf = _CLIDebuggerEditLinePrintf;
	elbe->d.init = _CLIDebuggerEditLineInit;
	elbe->d.deinit = _CLIDebuggerEditLineDeinit;
	elbe->d.readline = _CLIDebuggerEditLineReadLine;
	elbe->d.lineAppend = _CLIDebuggerEditLineLineAppend;
	elbe->d.historyLast = _CLIDebuggerEditLineHistoryLast;
	elbe->d.historyAppend = _CLIDebuggerEditLineHistoryAppend;
	elbe->d.interrupt = NULL;
	return &elbe->d;
}
