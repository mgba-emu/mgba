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

#include <mgba-util/png-io.h>
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
	CI_ERROR,
	CI_SKIP
};

struct CInemaTest {
	char directory[MAX_TEST];
	char filename[MAX_TEST];
	char name[MAX_TEST];
	enum CInemaStatus status;
	int failedFrames;
};

DECLARE_VECTOR(CInemaTestList, struct CInemaTest)
DEFINE_VECTOR(CInemaTestList, struct CInemaTest)

DECLARE_VECTOR(ImageList, void*)
DEFINE_VECTOR(ImageList, void*)

static bool showVersion = false;
static bool showUsage = false;
static char base[PATH_MAX] = {0};
static bool dryRun = false;
static int verbosity = 0;

bool CInemaTestInit(struct CInemaTest*, const char* directory, const char* filename);
void CInemaTestRun(struct CInemaTest*);

static void _log(struct mLogger* log, int category, enum mLogLevel level, const char* format, va_list args);

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
	CIerr(2, "Considering path %s\n", path);
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

void CInemaTestRun(struct CInemaTest* test) {
	struct VDir* dir = VDirOpen(test->directory);
	if (!dir) {
		CIerr(0, "Failed to open test directory\n");
		test->status = CI_ERROR;
		return;
	}
	struct VFile* rom = dir->openFile(dir, test->filename, O_RDONLY);
	if (!rom) {
		CIerr(0, "Failed to open test\n");
		test->status = CI_ERROR;
		return;
	}
	struct mCore* core = mCoreFindVF(rom);
	if (!core) {
		CIerr(0, "Failed to load test\n");
		test->status = CI_ERROR;
		rom->close(rom);
		return;
	}
	if (!core->init(core)) {
		CIerr(0, "Failed to init test\n");
		test->status = CI_ERROR;
		core->deinit(core);
		return;
	}
	unsigned width, height;
	core->desiredVideoDimensions(core, &width, &height);
	ssize_t bufferSize = width * height * BYTES_PER_PIXEL;
	void* buffer = malloc(bufferSize);
	if (!buffer) {
		CIerr(0, "Failed to allocate video buffer\n");
		test->status = CI_ERROR;
		core->deinit(core);
	}
	core->setVideoBuffer(core, buffer, width);
	mCoreConfigInit(&core->config, "cinema");

	core->loadROM(core, rom);
	core->reset(core);

	test->status = CI_PASS;

	unsigned minFrame = core->frameCounter(core);
	unsigned limit = 9999;
	size_t frame;
	for (frame = 0; limit; ++frame, --limit) {
		char baselineName[32];
		snprintf(baselineName, sizeof(baselineName), "baseline_%04" PRIz "u.png", frame);
		core->runFrame(core);
		unsigned frameCounter = core->frameCounter(core);
		if (frameCounter <= minFrame) {
			break;
		}
		CIerr(2, "Test frame: %u\n", frameCounter);

		struct VFile* baselineVF = dir->openFile(dir, baselineName, O_RDONLY);
		if (!baselineVF) {
			test->status = CI_FAIL;
		} else {
			png_structp png = PNGReadOpen(baselineVF, 0);
			png_infop info = png_create_info_struct(png);
			png_infop end = png_create_info_struct(png);
			if (!png || !info || !end || !PNGReadHeader(png, info)) {
				PNGReadClose(png, info, end);
				CIerr(1, "Failed to load %s\n", baselineName);
				test->status = CI_ERROR;
			} else {
				unsigned pwidth = png_get_image_width(png, info);
				unsigned pheight = png_get_image_height(png, info);
				unsigned twidth, theight;
				core->desiredVideoDimensions(core, &twidth, &theight);
				if (pheight != theight || pwidth != twidth) {
					PNGReadClose(png, info, end);
					CIerr(1, "Size mismatch for %s, expected %ux%u, got %ux%u\n", baselineName, pwidth, pheight, twidth, theight);
					test->status = CI_FAIL;
				} else {
					uint8_t* pixels = malloc(pwidth * pheight * BYTES_PER_PIXEL);
					if (!pixels) {
						CIerr(1, "Failed to allocate baseline buffer\n");
						test->status = CI_ERROR;
					} else {
						if (!PNGReadPixels(png, info, pixels, pwidth, pheight, pwidth) || !PNGReadFooter(png, end)) {
							CIerr(1, "Failed to read %s\n", baselineName);
							test->status = CI_ERROR;
						} else {
							uint8_t* testPixels = buffer;
							size_t x;
							size_t y;
							for (y = 0; y < theight; ++y) {
								for (x = 0; x < twidth; ++x) {
									size_t pix = pwidth * y + x;
									size_t tpix = width * y + x;
									int testR = testPixels[tpix * 4 + 0];
									int testG = testPixels[tpix * 4 + 1];
									int testB = testPixels[tpix * 4 + 2];
									int expectR = pixels[pix * 4 + 0];
									int expectG = pixels[pix * 4 + 1];
									int expectB = pixels[pix * 4 + 2];
									int r = expectR - testR;
									int g = expectG - testG;
									int b = expectB - testB;
									if (r | g | b) {
										CIerr(2, "Frame %u failed at pixel %" PRIz "ux%" PRIz "u with diff %i,%i,%i (expected %02x%02x%02x, got %02x%02x%02x)\n",
										    frameCounter, x, y, r, g, b,
										    expectR, expectG, expectB,
										    testR, testG, testB);
										test->status = CI_FAIL;
										++test->failedFrames;
									}
								}
							}
						}
					}
					PNGReadClose(png, info, end);
					free(pixels);
				}
			}
			baselineVF->close(baselineVF);
		}
	}

	free(buffer);
	core->deinit(core);
	dir->close(dir);
}

void _log(struct mLogger* log, int category, enum mLogLevel level, const char* format, va_list args) {
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

	struct mLogger logger = { .log = _log };
	mLogSetDefaultLogger(&logger);

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

	size_t i;
	for (i = 0; i < CInemaTestListSize(&tests); ++i) {
		struct CInemaTest* test = CInemaTestListGetPointer(&tests, i);
		if (dryRun) {
			CIlog(-1, "%s\n", test->name);
		} else {
			CIerr(1, "%s: ", test->name);
			CInemaTestRun(test);
			switch (test->status) {
			case CI_PASS:
				CIerr(1, "pass");
				break;
			case CI_FAIL:
				status = 1;
				CIerr(1, "fail");
				break;
			case CI_XPASS:
				CIerr(1, "xpass");
				break;
			case CI_XFAIL:
				CIerr(1, "xfail");
				break;
			case CI_SKIP:
				CIerr(1, "skip");
				break;
			case CI_ERROR:
				status = 1;
				CIerr(1, "error");
				break;
			}
			CIerr(1, "\n");
		}
	}

	CInemaTestListDeinit(&tests);

cleanup:
	return status;
}