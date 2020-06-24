/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/version.h>
#include <mgba/feature/commandline.h>
#include <mgba/feature/video-logger.h>

#include <mgba-util/vector.h>
#include <mgba-util/vfs.h>

#ifdef _MSC_VER
#include <mgba-util/platform/windows/getopt.h>
#else
#include <getopt.h>
#endif

#include <stdlib.h>

#define MAX_TEST 200

static const struct option longOpts[] = {
	{ "base",      required_argument, 0, 'b' },
	{ "help",      no_argument, 0, 'h' },
	{ "quiet",     no_argument, 0, 'q' },
	{ "dry-run",   no_argument, 0, 'n' },
	{ "verbose",   no_argument, 0, 'v' },
	{ "version",   no_argument, 0, '\0' },
	{ 0, 0, 0, 0 }
};

static const char shortOpts[] = "b:hnqv";

enum CInemaStatus {
	CI_PASS,
	CI_FAIL,
	CI_XPASS,
	CI_XFAIL,
	CI_SKIP
};

struct CInemaTest {
	char directory[MAX_TEST];
	char filename[MAX_TEST];
	char name[MAX_TEST];
	struct mCoreConfig config;
	enum CInemaStatus status;
	int failedFrames;
};

DECLARE_VECTOR(CInemaTestList, struct CInemaTest)
DEFINE_VECTOR(CInemaTestList, struct CInemaTest)

static bool showVersion = false;
static bool showUsage = false;
static char base[PATH_MAX] = {0};
static bool dryRun = false;
static int verbosity = 0;

ATTRIBUTE_FORMAT(printf, 2, 3) void CIlog(int minlevel, const char* format, ...) {
	if (verbosity < minlevel) {
		return;
	}
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

ATTRIBUTE_FORMAT(printf, 2, 3) void CIerr(int minlevel, const char* format, ...) {
	if (verbosity < minlevel) {
		return;
	}
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

bool CInemaTestInit(struct CInemaTest*, const char* directory, const char* filename);
void CInemaTestDeinit(struct CInemaTest*);

static bool parseCInemaArgs(int argc, char* const* argv) {
	int ch;
	int index = 0;
	while ((ch = getopt_long(argc, argv, shortOpts, longOpts, &index)) != -1) {
		const struct option* opt = &longOpts[index];
		switch (ch) {
		case '\0':
			if (strcmp(opt->name, "version") == 0) {
				showVersion = true;
			} else {
				return false;
			}
			break;
		case 'b':
			strncpy(base, optarg, sizeof(base));
			// TODO: Verify path exists
			break;
		case 'h':
			showUsage = true;
			break;
		case 'n':
			dryRun = true;
			break;
		case 'q':
			--verbosity;
			break;
		case 'v':
			++verbosity;
			break;
		default:
			return false;
		}
	}

	return true;
}

static void usageCInema(const char* arg0) {
	printf("usage: %s [-h] [-b BASE] [--version] [test...]\n", arg0);
	puts("  -b, --base                 Path to the CInema base directory");
	puts("  -h, --help                 Print this usage and exit");
	puts("  -n, --dry-run              List all collected tests instead of running them");
	puts("  -q, --quiet                Decrease log verbosity (can be repeated)");
	puts("  -v, --verbose              Increase log verbosity (can be repeated)");
	puts("  --version                  Print version and exit");
}

static bool determineBase(int argc, char* const* argv) {
	// TODO: Better dynamic detection
	separatePath(__FILE__, base, NULL, NULL);
	strncat(base, PATH_SEP ".." PATH_SEP ".." PATH_SEP ".." PATH_SEP "cinema", sizeof(base) - strlen(base) - 1);
	return true;
}

static bool collectTests(struct CInemaTestList* tests, const char* path) {
	CIerr(1, "Considering path %s\n", path);
	struct VDir* dir = VDirOpen(path);
	if (!dir) {
		return false;
	}
	struct VDirEntry* entry = dir->listNext(dir);
	while (entry) {
		char subpath[PATH_MAX];
		snprintf(subpath, sizeof(subpath), "%s" PATH_SEP "%s", path, entry->name(entry));
		if (entry->type(entry) == VFS_DIRECTORY && strncmp(entry->name(entry), ".", 2) != 0 && strncmp(entry->name(entry), "..", 3) != 0) {
			if (!collectTests(tests, subpath)) {
				dir->close(dir);
				return false;
			}
		} else if (entry->type(entry) == VFS_FILE && strncmp(entry->name(entry), "test.", 5) == 0) {
			CIerr(2, "Found potential test %s\n", subpath);
			struct VFile* vf = dir->openFile(dir, entry->name(entry), O_RDONLY);
			if (vf) {
				if (mCoreIsCompatible(vf) != PLATFORM_NONE || mVideoLogIsCompatible(vf) != PLATFORM_NONE) {
					struct CInemaTest* test = CInemaTestListAppend(tests);
					if (!CInemaTestInit(test, path, entry->name(entry))) {
						CIerr(2, "Failed to create test\n");
						CInemaTestListResize(tests, -1);
					} else {
						CIerr(1, "Found test %s\n", test->name);
					}
				} else {
					CIerr(2, "Not a compatible file\n");
				}
				vf->close(vf);
			} else {
				CIerr(2, "Failed to open file\n");
			}
		}
		entry = dir->listNext(dir);
	}
	dir->close(dir);
	return true;
}

static int _compareNames(const void* a, const void* b) {
	const struct CInemaTest* ta = a;
	const struct CInemaTest* tb = b;

	return strncmp(ta->name, tb->name, sizeof(ta->name));
}

static void reduceTestList(struct CInemaTestList* tests) {
	qsort(CInemaTestListGetPointer(tests, 0), CInemaTestListSize(tests), sizeof(struct CInemaTest), _compareNames);

	size_t i;
	for (i = 1; i < CInemaTestListSize(tests);) {
		struct CInemaTest* cur = CInemaTestListGetPointer(tests, i);
		struct CInemaTest* prev = CInemaTestListGetPointer(tests, i - 1);
		if (strncmp(cur->name, prev->name, sizeof(cur->name)) != 0) {
			++i;
			continue;
		}
		CInemaTestDeinit(cur);
		CInemaTestListShift(tests, i, 1);
	}
}

bool CInemaTestInit(struct CInemaTest* test, const char* directory, const char* filename) {
	if (strncmp(base, directory, strlen(base)) != 0) {
		return false;
	}
	strncpy(test->directory, directory, sizeof(test->directory));
	strncpy(test->filename, filename, sizeof(test->filename));
	directory += strlen(base) + 1;
	strncpy(test->name, directory, sizeof(test->name));
	char* str = strstr(test->name, PATH_SEP);
	while (str) {
		str[0] = '.';
		str = strstr(str, PATH_SEP);
	}
	return true;
}

void CInemaTestDeinit(struct CInemaTest* test) {
	// TODO: Write
}

int main(int argc, char** argv) {
	int status = 0;
	if (!parseCInemaArgs(argc, argv)) {
		status = 1;
		goto cleanup;
	}

	if (showVersion) {
		version(argv[0]);
		goto cleanup;
	}

	if (showUsage) {
		usageCInema(argv[0]);
		goto cleanup;
	}

	argc -= optind;
	argv += optind;

	if (!base[0] && !determineBase(argc, argv)) {
		CIerr(0, "Could not determine CInema test base. Please specify manually.");
		status = 1;
		goto cleanup;
	}
#ifndef _WIN32
	char* rbase = realpath(base, NULL);
	strncpy(base, rbase, PATH_MAX);
	free(rbase);
#endif

	struct CInemaTestList tests;
	CInemaTestListInit(&tests, 0);

	if (argc > 0) {
		size_t i;
		for (i = 0; i < (size_t) argc; ++i) {
			char path[PATH_MAX + 1] = {0};
			char* arg = argv[i];
			strncpy(path, base, sizeof(path));

			bool dotSeen = true;
			size_t j;
			for (arg = argv[i], j = strlen(path); arg[0] && j < sizeof(path); ++arg) {
				if (arg[0] == '.') {
					dotSeen = true;
				} else {
					if (dotSeen) {
						strncpy(&path[j], PATH_SEP, sizeof(path) - j);
						j += strlen(PATH_SEP);
						dotSeen = false;
						if (!j) {
							break;
						}
					}
					path[j] = arg[0];
					++j;
				}
			}

			if (!collectTests(&tests, path)) {
				status = 1;
				break;
			}
		}
	} else if (!collectTests(&tests, base)) {
		status = 1;
	}

	if (CInemaTestListSize(&tests) == 0) {
		CIerr(1, "No tests found.");
		status = 1;
	} else {
		reduceTestList(&tests);
	}

	if (dryRun) {
		size_t i;
		for (i = 0; i < CInemaTestListSize(&tests); ++i) {
			struct CInemaTest* test = CInemaTestListGetPointer(&tests, i);
			CIlog(-1, "%s\n", test->name);
		}
	} else {
		// TODO: Write
	}

	size_t i;
	for (i = 0; i < CInemaTestListSize(&tests); ++i) {
		struct CInemaTest* test = CInemaTestListGetPointer(&tests, i);
		CInemaTestDeinit(test);
	}

	CInemaTestListDeinit(&tests);

cleanup:
	return status;
}