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

#include <mgba-util/image/png-io.h>
#include <mgba-util/string.h>
#include <mgba-util/table.h>
#include <mgba-util/threading.h>
#include <mgba-util/vector.h>
#include <mgba-util/vfs.h>

#ifdef USE_FFMPEG
#include "feature/ffmpeg/ffmpeg-decoder.h"
#include "feature/ffmpeg/ffmpeg-encoder.h"
#endif

#ifdef _MSC_VER
#include <mgba-util/platform/windows/getopt.h>
#else
#include <getopt.h>
#endif

#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_TEST 200
#define MAX_JOBS 128
#define LOG_THRESHOLD 1000000

static const struct option longOpts[] = {
	{ "4up",        no_argument, 0, '4' },
	{ "base",       required_argument, 0, 'b' },
	{ "diffs",      no_argument, 0, 'd' },
	{ "help",       no_argument, 0, 'h' },
	{ "jobs",       required_argument, 0, 'j' },
	{ "dry-run",    no_argument, 0, 'n' },
	{ "outdir",     required_argument, 0, 'o' },
	{ "quiet",      no_argument, 0, 'q' },
	{ "rebaseline", no_argument, 0, 'r' },
	{ "rebaseline-missing", no_argument, 0, 'R' },
	{ "verbose",    no_argument, 0, 'v' },
	{ "xbaseline",  no_argument, 0, 'x' },
	{ "version",    no_argument, 0, '\0' },
	{ 0, 0, 0, 0 }
};

static const char shortOpts[] = "4b:dhj:no:qRrvx";

enum CInemaStatus {
	CI_PASS,
	CI_FAIL,
	CI_XPASS,
	CI_XFAIL,
	CI_ERROR,
	CI_SKIP
};

enum CInemaRebaseline {
	CI_R_NONE = 0,
	CI_R_FAILING,
	CI_R_MISSING,
};

struct CInemaTest {
	char directory[PATH_MAX];
	char filename[MAX_TEST];
	char name[MAX_TEST];
	enum CInemaStatus status;
	unsigned failedFrames;
	uint64_t failedPixels;
	unsigned totalFrames;
	uint64_t totalDistance;
	uint64_t totalPixels;
	jmp_buf errorCtx;
};

struct CInemaImage {
	void* data;
	unsigned width;
	unsigned height;
	unsigned stride;
};

DECLARE_VECTOR(CInemaTestList, struct CInemaTest)
DEFINE_VECTOR(CInemaTestList, struct CInemaTest)

DECLARE_VECTOR(ImageList, void*)
DEFINE_VECTOR(ImageList, void*)

struct StringBuilder {
	struct StringList lines;
	struct StringList partial;
	unsigned repeat;
};

struct CInemaLogStream {
	struct StringBuilder err;
	struct StringBuilder out;
};

static bool showVersion = false;
static bool showUsage = false;
static char base[PATH_MAX] = {0};
static char outdir[PATH_MAX] = {'.'};
static bool dryRun = false;
static bool diffs = false;
static bool is4Up = false;
static enum CInemaRebaseline rebaseline = CI_R_NONE;
static enum CInemaRebaseline xbaseline = CI_R_NONE;
static int verbosity = 0;

static struct Table configTree;
static Mutex configMutex;

static int jobs = 1;
static size_t jobIndex = 0;
static Mutex jobMutex;
static Thread jobThreads[MAX_JOBS];
static int jobStatus;
static ThreadLocal logStream;
static ThreadLocal currentTest;

bool CInemaTestInit(struct CInemaTest*, const char* directory, const char* filename);
void CInemaTestRun(struct CInemaTest*);

bool CInemaConfigGetUInt(struct Table* configTree, const char* testName, const char* key, unsigned* value);
void CInemaConfigLoad(struct Table* configTree, const char* testName, struct mCore* core);

static void _log(struct mLogger* log, int category, enum mLogLevel level, const char* format, va_list args);

void CIflush(struct StringBuilder* list, FILE* file);

static char* _compileStringList(struct StringList* list) {
	size_t len = 0;
	size_t i;
	for (i = 0; i < StringListSize(list); ++i) {
		len += strlen(*StringListGetPointer(list, i));
	}
	char* string = calloc(len + 1, sizeof(char));
	char* cur = string;
	for (i = 0; i < StringListSize(list); ++i) {
		char* brick = *StringListGetPointer(list, i);
		size_t portion = strlen(brick);
		memcpy(cur, brick, portion);
		free(brick);
		cur += portion;
	}
	StringListClear(list);
	return string;
}

static void _logToStream(FILE* file, const char* format, va_list args) {
#ifdef HAVE_VASPRINTF
	struct CInemaLogStream* stream = ThreadLocalGetValue(logStream);
	if (!stream) {
		vfprintf(file, format, args);
	} else {
		struct StringBuilder* builder = &stream->out;
		if (file == stderr) {
			builder = &stream->err;
		}
		if (StringListSize(&builder->lines) > LOG_THRESHOLD) {
			CIflush(builder, file);
		}
		char** line = StringListAppend(&builder->partial);
		vasprintf(line, format, args);
		size_t len = strlen(*line);
		if (len && (*line)[len - 1] == '\n') {
			char* string = _compileStringList(&builder->partial);
			size_t linecount = StringListSize(&builder->lines);
			if (linecount && strcmp(string, *StringListGetPointer(&builder->lines, linecount - 1)) == 0) {
				++builder->repeat;
				free(string);
			} else {
				if (builder->repeat > 1) {
					asprintf(StringListAppend(&builder->lines), "The previous message was repeated %u times.\n", builder->repeat);
				}
				*StringListAppend(&builder->lines) = string;
				builder->repeat = 1;
			}
		}
	}
#else
	vfprintf(file, format, args);
#endif
}

ATTRIBUTE_FORMAT(printf, 2, 3) void CIlog(int minlevel, const char* format, ...) {
	if (verbosity < minlevel) {
		return;
	}
	va_list args;
	va_start(args, format);
	_logToStream(stdout, format, args);
	va_end(args);
}

ATTRIBUTE_FORMAT(printf, 2, 3) void CIerr(int minlevel, const char* format, ...) {
	if (verbosity < minlevel) {
		return;
	}
	va_list args;
	va_start(args, format);
	_logToStream(stderr, format, args);
	va_end(args);
}

void CIflush(struct StringBuilder* builder, FILE* out) {
	if (StringListSize(&builder->partial)) {
		*StringListAppend(&builder->lines) = _compileStringList(&builder->partial);
	}
#ifdef HAVE_VASPRINTF
	if (builder->repeat > 1) {
		asprintf(StringListAppend(&builder->lines), "The previous message was repeated %u times.\n", builder->repeat);
	}
#endif

	char* string = _compileStringList(&builder->lines);
	builder->repeat = 0;
	fputs(string, out);
	free(string);
	fflush(out);
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
		case '4':
			is4Up = true;
			break;
		case 'b':
			strlcpy(base, optarg, sizeof(base));
			// TODO: Verify path exists
			break;
		case 'd':
			diffs = true;
			break;
		case 'h':
			showUsage = true;
			break;
		case 'j':
			jobs = atoi(optarg);
			if (jobs > MAX_JOBS) {
				jobs = MAX_JOBS;
			}
			if (jobs < 1) {
				jobs = 1;
			}
			break;
		case 'n':
			dryRun = true;
			break;
		case 'o':
			strlcpy(outdir, optarg, sizeof(outdir));
			// TODO: Make directory
			break;
		case 'q':
			--verbosity;
			break;
		case 'r':
			rebaseline = CI_R_FAILING;
			break;
		case 'R':
			rebaseline = CI_R_MISSING;
			break;
		case 'v':
			++verbosity;
			break;
		case 'x':
			xbaseline = CI_R_FAILING;
			break;
		default:
			return false;
		}
	}

	return true;
}

static void usageCInema(const char* arg0) {
	printf("usage: %s [-dhnqrRv] [-j JOBS] [-b BASE] [-o DIR] [--version] [test...]\n", arg0);
	puts("  -b, --base BASE            Path to the CInema base directory");
	puts("  -d, --diffs                Output image diffs from failures");
	puts("  -h, --help                 Print this usage and exit");
	puts("  -j, --jobs JOBS            Run a number of jobs in parallel");
	puts("  -n, --dry-run              List all collected tests instead of running them");
	puts("  -o, --output DIR           Path to output applicable results");
	puts("  -q, --quiet                Decrease log verbosity (can be repeated)");
	puts("  -r, --rebaseline           Rewrite the baseline for failing tests");
	puts("  -R, --rebaseline-missing   Write missing baselines tests only");
	puts("  -v, --verbose              Increase log verbosity (can be repeated)");
	puts("  -x, --xbaseline            Rewrite the xfail baselines for failing tests");
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
			CIerr(3, "Found potential test %s\n", subpath);
			struct VFile* vf = dir->openFile(dir, entry->name(entry), O_RDONLY);
			if (vf) {
				if (mCoreIsCompatible(vf) != mPLATFORM_NONE || mVideoLogIsCompatible(vf) != mPLATFORM_NONE) {
					struct CInemaTest* test = CInemaTestListAppend(tests);
					if (!CInemaTestInit(test, path, entry->name(entry))) {
						CIerr(3, "Failed to create test\n");
						CInemaTestListResize(tests, -1);
					} else {
						CIerr(2, "Found test %s\n", test->name);
					}
				} else {
					CIerr(3, "Not a compatible file\n");
				}
				vf->close(vf);
			} else {
				CIerr(3, "Failed to open file\n");
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

static void testToPath(const char* testName, char* path) {
	strlcpy(path, base, PATH_MAX);

	bool dotSeen = true;
	size_t i;
	for (i = strlen(path); testName[0] && i < PATH_MAX; ++testName) {
		if (testName[0] == '.') {
			dotSeen = true;
		} else {
			if (dotSeen) {
				strlcpy(&path[i], PATH_SEP, PATH_MAX - i);
				i += strlen(PATH_SEP);
				dotSeen = false;
				if (!i) {
					break;
				}
			}
			path[i] = testName[0];
			++i;
		}
	}
	if (i == PATH_MAX) {
		--i;
	}
	path[i] = '\0';
}

static bool globTests(struct CInemaTestList* tests, const char* glob, const char* ancestors) {
	bool success = true;
	const char* next = strpbrk(glob, "*.");

	char path[PATH_MAX];
	if (!next) {
		testToPath(glob, path);
		return collectTests(tests, path);
	} else if (next[0] == '.') {
		char subtest[MAX_TEST];
		if (!ancestors) {
			strlcpy(subtest, glob, next - glob + 1);
		} else {
			size_t len = strlen(ancestors) + (next - glob) + 2;
			if (len > sizeof(subtest)) {
				len = sizeof(subtest);
			}
			snprintf(subtest, len, "%s.%s", ancestors, glob);
		}
		return globTests(tests, next + 1, subtest);
	} else if (next[0] == '*') {
		char globBuffer[MAX_TEST];
		const char* subglob;

		next = strchr(next, '.');
		if (!next) {
			subglob = glob;
		} else {
			size_t len = next - glob + 1;
			if (len > sizeof(globBuffer)) {
				len = sizeof(globBuffer);
			}
			strncpy(globBuffer, glob, len - 1);
			subglob = globBuffer;
		}
		bool hasMoreGlobs = next && strchr(next, '*');

		struct VDir* dir;
		if (ancestors) {
			testToPath(ancestors, path);
			dir = VDirOpen(path);
		} else {
			dir = VDirOpen(base);
		}
		if (!dir) {
			return false;
		}

		struct VDirEntry* dirent = dir->listNext(dir);
		while (dirent) {
			const char* name = dirent->name(dirent);
			if (dirent->type(dirent) != VFS_DIRECTORY || strncmp(name, ".", 2) == 0 || strncmp(name, "..", 3) == 0) {
				dirent = dir->listNext(dir);
				continue;
			}
			if (wildcard(subglob, name)) {
				char newgen[MAX_TEST];
				if (ancestors) {
					snprintf(newgen, sizeof(newgen), "%s.%s", ancestors, name);
				} else {
					strlcpy(newgen, name, sizeof(newgen));
				}
				if (next && hasMoreGlobs) {
					globTests(tests, next + 1, newgen);
				} else {
					testToPath(newgen, path);
					collectTests(tests, path);
				}
			}
			dirent = dir->listNext(dir);
		}

		return true;
	} else {
		abort();
	}
}

static void _loadConfigTree(struct Table* configTree, const char* testName) {
	char key[MAX_TEST];
	strlcpy(key, testName, sizeof(key));

	struct mCoreConfig* config;
	while (!(config = HashTableLookup(configTree, key))) {
		char path[PATH_MAX];
		config = malloc(sizeof(*config));
		mCoreConfigInit(config, "cinema");
		testToPath(key, path);
		strncat(path, PATH_SEP, sizeof(path) - 1);
		strncat(path, "config.ini", sizeof(path) - 1);
		mCoreConfigLoadPath(config, path);
		HashTableInsert(configTree, key, config);
		char* pos = strrchr(key, '.');
		if (pos) {
			pos[0] = '\0';
		} else if (key[0]) {
			key[0] = '\0';
		} else {
			break;
		}
	}
}

static void _unloadConfigTree(const char* key, void* value, void* user) {
	UNUSED(key);
	UNUSED(user);
	mCoreConfigDeinit(value);
}

static const char* CInemaConfigGet(struct Table* configTree, const char* testName, const char* key) {
	_loadConfigTree(configTree, testName);

	char testKey[MAX_TEST];
	strlcpy(testKey, testName, sizeof(testKey));

	struct mCoreConfig* config;
	while (true) {
		config = HashTableLookup(configTree, testKey);
		if (!config) {
			continue;
		}
		const char* str = ConfigurationGetValue(&config->configTable, "testinfo", key);
		if (str) {
			return str;
		}
		char* pos = strrchr(testKey, '.');
		if (pos) {
			pos[0] = '\0';
		} else if (testKey[0]) {
			testKey[0] = '\0';
		} else {
			break;
		}
	}
	return NULL;
}

bool CInemaConfigGetUInt(struct Table* configTree, const char* testName, const char* key, unsigned* out) {
	const char* charValue = CInemaConfigGet(configTree, testName, key);
	if (!charValue) {
		return false;
	}
	char* end;
	unsigned long value = strtoul(charValue, &end, 10);
	if (*end) {
		return false;
	}
	*out = value;
	return true;
}

void CInemaConfigLoad(struct Table* configTree, const char* testName, struct mCore* core) {
	_loadConfigTree(configTree, testName);

	char testKey[MAX_TEST] = {0};
	char* keyEnd = testKey;
	const char* pos;
	while (true) {
		pos = strchr(testName, '.');
		size_t maxlen = sizeof(testKey) - (keyEnd - testKey) - 1;
		size_t len;
		if (pos) {
			len = pos - testName;
		} else {
			len = strlen(testName);
		}
		if (len > maxlen) {
			len = maxlen;
		}
		strncpy(keyEnd, testName, len);
		keyEnd += len;

		struct mCoreConfig* config = HashTableLookup(configTree, testKey);
		if (config) {
			core->loadConfig(core, config);
		}
		if (!pos) {
			break;
		}
		testName = pos + 1;
		keyEnd[0] = '.';
		++keyEnd;
	}
}

bool CInemaTestInit(struct CInemaTest* test, const char* directory, const char* filename) {
	if (strncmp(base, directory, strlen(base)) != 0) {
		return false;
	}
	memset(test, 0, sizeof(*test));
	strlcpy(test->directory, directory, sizeof(test->directory));
	strlcpy(test->filename, filename, sizeof(test->filename));
	directory += strlen(base) + 1;
	strlcpy(test->name, directory, sizeof(test->name));
	char* str = strstr(test->name, PATH_SEP);
	while (str) {
		str[0] = '.';
		str = strstr(str, PATH_SEP);
	}
	return true;
}

static bool _loadBaselinePNG(struct VDir* dir, const char* type, struct CInemaImage* image, size_t frame, enum CInemaStatus* status) {
	char baselineName[32];
	snprintf(baselineName, sizeof(baselineName), "%s_%04" PRIz "u.png", type, frame);
	struct VFile* baselineVF = dir->openFile(dir, baselineName, O_RDONLY);
	if (!baselineVF) {
		if (*status == CI_PASS) {
			*status = CI_FAIL;
		}
		return false;
	}

	png_structp png = PNGReadOpen(baselineVF, 0);
	png_infop info = png_create_info_struct(png);
	png_infop end = png_create_info_struct(png);
	if (!png || !info || !end || !PNGReadHeader(png, info)) {
		PNGReadClose(png, info, end);
		baselineVF->close(baselineVF);
		CIerr(1, "Failed to load %s\n", baselineName);
		*status = CI_ERROR;
		return false;
	}

	unsigned pwidth = png_get_image_width(png, info);
	unsigned pheight = png_get_image_height(png, info);
	if (pheight != image->height || pwidth != image->width) {
		PNGReadClose(png, info, end);
		baselineVF->close(baselineVF);
		CIlog(1, "Size mismatch for %s, expected %ux%u, got %ux%u\n", baselineName, pwidth, pheight, image->width, image->height);
		if (*status == CI_PASS) {
			*status = CI_FAIL;
		}
		return false;
	}

	image->data = malloc(pwidth * pheight * BYTES_PER_PIXEL);
	if (!image->data) {
		CIerr(1, "Failed to allocate baseline buffer\n");
		*status = CI_ERROR;
		PNGReadClose(png, info, end);
		baselineVF->close(baselineVF);
		return false;
	}
	if (!PNGReadPixels(png, info, image->data, pwidth, pheight, pwidth) || !PNGReadFooter(png, end)) {
		CIerr(1, "Failed to read %s\n", baselineName);
		*status = CI_ERROR;
		free(image->data);
		return false;
	}
	PNGReadClose(png, info, end);
	baselineVF->close(baselineVF);
	image->stride = pwidth;
	return true;
}

#ifdef USE_FFMPEG
struct CInemaStream {
	struct mAVStream d;
	struct CInemaImage* image;
	enum CInemaStatus* status;
};

static void _cinemaDimensionsChanged(struct mAVStream* stream, unsigned width, unsigned height) {
	struct CInemaStream* cistream = (struct CInemaStream*) stream;
	if (height != cistream->image->height || width != cistream->image->width) {
		CIlog(1, "Size mismatch for video, expected %ux%u, got %ux%u\n", width, height, cistream->image->width, cistream->image->height);
		if (*cistream->status == CI_PASS) {
			*cistream->status = CI_FAIL;
		}
	}
}

static void _cinemaVideoFrame(struct mAVStream* stream, const color_t* pixels, size_t stride) {
	struct CInemaStream* cistream = (struct CInemaStream*) stream;
	cistream->image->stride = stride;
	size_t bufferSize = cistream->image->stride * cistream->image->height * BYTES_PER_PIXEL;
	cistream->image->data = malloc(bufferSize);
	memcpy(cistream->image->data, pixels, bufferSize);
}
#endif

static struct VDir* _makeOutDir(const char* testName) {
	char path[PATH_MAX] = {0};
	strlcpy(path, outdir, sizeof(path));
	char* pathEnd = path + strlen(path);
	const char* pos;
	while (true) {
		pathEnd[0] = PATH_SEP[0];
		++pathEnd;
		pos = strchr(testName, '.');
		size_t maxlen = sizeof(path) - (pathEnd - path) - 1;
		size_t len;
		if (pos) {
			len = pos - testName;
		} else {
			len = strlen(testName);
		}
		if (len > maxlen) {
			len = maxlen;
		}
		strncpy(pathEnd, testName, len);
		pathEnd += len;

#ifndef _WIN32
		mkdir(path, 0777);
#else
		mkdir(path);
#endif

		if (!pos) {
			break;
		}
		testName = pos + 1;
	}
	return VDirOpen(path);
}

static void _writeImage(struct VFile* vf, const struct CInemaImage* image) {
	png_structp png = PNGWriteOpen(vf);
	png_infop info = PNGWriteHeader(png, image->width, image->height, mCOLOR_NATIVE);
	if (!PNGWritePixels(png, image->width, image->height, image->stride, image->data, mCOLOR_NATIVE)) {
		CIerr(0, "Could not write output image\n");
	}
	PNGWriteClose(png, info);

	vf->close(vf);
}

static void _writeDiff(const char* testName, const struct CInemaImage* image, size_t frame, const char* type) {
	struct VDir* dir = _makeOutDir(testName);
	if (!dir) {
		CIerr(0, "Could not open directory for %s\n", testName);
		return;
	}
	char name[32];
	snprintf(name, sizeof(name), "%s_%05" PRIz "u.png", type, frame);
	struct VFile* vf = dir->openFile(dir, name, O_CREAT | O_TRUNC | O_WRONLY);
	if (!vf) {
		CIerr(0, "Could not open output file %s\n", name);
		dir->close(dir);
		return;
	}
	_writeImage(vf, image);
	dir->close(dir);
}

static void _writeBaseline(struct VDir* dir, const char* type, const struct CInemaImage* image, size_t frame) {
	char baselineName[32];
	snprintf(baselineName, sizeof(baselineName), "%s_%04" PRIz "u.png", type, frame);
	struct VFile* baselineVF = dir->openFile(dir, baselineName, O_CREAT | O_TRUNC | O_WRONLY);
	if (baselineVF) {
		_writeImage(baselineVF, image);
	} else {
		CIerr(0, "Could not open output file %s\n", baselineName);
	}
}

static bool _updateInput(struct mCore* core, size_t frame, const char** input) {
	if (!*input || !*input[0]) {
		return false;
	}
	char* end;
	uint32_t start = strtoul(*input, &end, 10);
	if (end[0] != ':') {
		return false;
	}
	if (start != frame) {
		return true;
	}
	++end;
	*input = end;
	uint32_t keys = strtoul(*input, &end, 16);
	if (end[0] == ',') {
		++end;
	}
	*input = end;
	core->setKeys(core, keys);
	return true;
}

static bool _compareImages(struct CInemaTest* restrict test, const struct CInemaImage* restrict image, const struct CInemaImage* restrict expected, int* restrict max, uint8_t** restrict outdiff) {
	const uint8_t* testPixels = image->data;
	const uint8_t* expectPixels = expected->data;
	uint8_t* diff = NULL;
	size_t x;
	size_t y;
	bool failed = false;
	for (y = 0; y < image->height; ++y) {
		for (x = 0; x < image->width; ++x) {
			size_t pix = expected->stride * y + x;
			size_t tpix = image->stride * y + x;
#ifndef __BIG_ENDIAN__
			int testR = testPixels[tpix * 4 + 0];
			int testG = testPixels[tpix * 4 + 1];
			int testB = testPixels[tpix * 4 + 2];
			int expectR = expectPixels[pix * 4 + 0];
			int expectG = expectPixels[pix * 4 + 1];
			int expectB = expectPixels[pix * 4 + 2];
#else
			int testB = testPixels[tpix * 4 + 1];
			int testG = testPixels[tpix * 4 + 2];
			int testR = testPixels[tpix * 4 + 3];
			int expectB = expectPixels[pix * 4 + 1];
			int expectG = expectPixels[pix * 4 + 2];
			int expectR = expectPixels[pix * 4 + 3];
#endif
			int r = expectR - testR;
			int g = expectG - testG;
			int b = expectB - testB;
			if (r | g | b) {
				failed = true;
				if (outdiff && !diff) {
					diff = calloc(expected->stride * expected->height, BYTES_PER_PIXEL);
					*outdiff = diff;
				}
				test->status = CI_FAIL;
				if (r < 0) {
					r = -r;
				}
				if (g < 0) {
					g = -g;
				}
				if (b < 0) {
					b = -b;
				}

				if (diff) {
					if (r > *max) {
						*max = r;
					}
					if (g > *max) {
						*max = g;
					}
					if (b > *max) {
						*max = b;
					}
#ifndef __BIG_ENDIAN__
					diff[pix * 4 + 0] = r;
					diff[pix * 4 + 1] = g;
					diff[pix * 4 + 2] = b;
#else
					diff[pix * 4 + 1] = b;
					diff[pix * 4 + 2] = g;
					diff[pix * 4 + 3] = r;
#endif
				}

				if (test) {
					test->totalDistance += r + g + b;
					++test->failedPixels;
				}
			}
		}
	}
	return !failed;
}

static void _write4UpDiff(const struct CInemaImage* expected, const struct CInemaImage* result, const char* name, uint8_t* diff, int frame, int max) {
	struct CInemaImage out = {
		.width = expected->width * 2,
		.height = expected->height * 2,
		.stride = expected->width * 2,
	};
	out.data = malloc(out.height * out.stride * 4);
	uint32_t* outdata = out.data;
	size_t x;
	size_t y;
	for (y = 0; y < expected->height; ++y) {
		size_t base = y * out.stride;
		size_t inbase = y * expected->stride;
		memcpy(&outdata[base], &((uint32_t*) expected->data)[inbase], expected->width * 4);
		memcpy(&outdata[base + expected->width], &((uint32_t*) result->data)[y * result->stride], expected->width * 4);
		memcpy(&outdata[base + expected->height * out.stride], &diff[inbase * 4], expected->width * 4);
		for (x = 0; x < expected->width; ++x) {
			size_t pix = (expected->stride * y + x) * 4;
			size_t outpix = base + expected->height * out.stride + expected->width + x;
			uint8_t* outdiff = (uint8_t*) &outdata[outpix];
#ifndef __BIG_ENDIAN__
			outdiff[0] = diff[pix + 0] * 255 / max;
			outdiff[1] = diff[pix + 1] * 255 / max;
			outdiff[2] = diff[pix + 2] * 255 / max;
#else
			outdiff[1] = diff[pix + 1] * 255 / max;
			outdiff[2] = diff[pix + 2] * 255 / max;
			outdiff[3] = diff[pix + 3] * 255 / max;
#endif
		}
	}
	_writeDiff(name, &out, frame, "4up");
	free(out.data);
}

static void _writeDiffSet(const struct CInemaImage* expected, const char* name, uint8_t* diff, int frame, int max, bool xfail) {
	struct CInemaImage outdiff = {
		.data = diff,
		.width = expected->width,
		.height = expected->height,
		.stride = expected->stride,
	};

	if (xfail) {
		_writeDiff(name, expected, frame, "xexpected");
		_writeDiff(name, &outdiff, frame, "xdiff");
	} else {
		_writeDiff(name, expected, frame, "expected");
		_writeDiff(name, &outdiff, frame, "diff");
	}

	size_t x;
	size_t y;
	for (y = 0; y < outdiff.height; ++y) {
		for (x = 0; x < outdiff.width; ++x) {
			size_t pix = outdiff.stride * y + x;
#ifndef __BIG_ENDIAN__
			diff[pix * 4 + 0] = diff[pix * 4 + 0] * 255 / max;
			diff[pix * 4 + 1] = diff[pix * 4 + 1] * 255 / max;
			diff[pix * 4 + 2] = diff[pix * 4 + 2] * 255 / max;
#else
			diff[pix * 4 + 1] = diff[pix * 4 + 1] * 255 / max;
			diff[pix * 4 + 2] = diff[pix * 4 + 2] * 255 / max;
			diff[pix * 4 + 3] = diff[pix * 4 + 3] * 255 / max;
#endif
		}
	}
	if (xfail) {
		_writeDiff(name, &outdiff, frame, "xnormalized");
	} else {
		_writeDiff(name, &outdiff, frame, "normalized");
	}
}

#ifdef USE_FFMPEG
static void _replayBaseline(struct CInemaTest* test, struct FFmpegEncoder* encoder, const struct CInemaImage* image, int frame) {
	char baselineName[PATH_MAX];
	snprintf(baselineName, sizeof(baselineName), "%s" PATH_SEP ".baseline.avi", test->directory);

	if (!FFmpegEncoderOpen(encoder, baselineName)) {
		CIerr(1, "Failed to save baseline video\n");
		test->status = CI_ERROR;
		return;
	}
	encoder->d.videoDimensionsChanged(&encoder->d, image->width, image->height);

	snprintf(baselineName, sizeof(baselineName), "%s" PATH_SEP "baseline.avi", test->directory);

	struct CInemaImage buffer = {
		.data = NULL,
		.width = image->width,
		.height = image->height,
		.stride = image->width,
	};
	struct FFmpegDecoder decoder;
	struct CInemaStream stream = {0};
	stream.d.postVideoFrame = _cinemaVideoFrame;
	stream.d.videoDimensionsChanged = _cinemaDimensionsChanged;
	stream.status = &test->status;
	stream.image = &buffer;

	FFmpegDecoderInit(&decoder);
	decoder.out = &stream.d;

	if (!FFmpegDecoderOpen(&decoder, baselineName)) {
		CIerr(1, "Failed to load baseline video\n");
		test->status = CI_ERROR;
		return;
	}

	int i;
	for (i = 0; i < frame; ++i) {
		while (!buffer.data) {
			if (!FFmpegDecoderRead(&decoder)) {
				CIlog(1, "Failed to read more frames. EOF?\n");
				test->status = CI_FAIL;
				break;
			}
		}
		encoder->d.postVideoFrame(&encoder->d, buffer.data, buffer.stride);
		free(buffer.data);
		buffer.data = NULL;
	}
	FFmpegDecoderClose(&decoder);
}
#endif

void CInemaTestRun(struct CInemaTest* test) {
	unsigned ignore = 0;
	MutexLock(&configMutex);
	CInemaConfigGetUInt(&configTree, test->name, "ignore", &ignore);
	MutexUnlock(&configMutex);
	if (ignore) {
		test->status = CI_SKIP;
		return;
	}

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
	struct CInemaImage image;
	core->baseVideoSize(core, &image.width, &image.height);
	ssize_t bufferSize = image.width * image.height * BYTES_PER_PIXEL;
	image.data = malloc(bufferSize);
	image.stride = image.width;
	if (!image.data) {
		CIerr(0, "Failed to allocate video buffer\n");
		test->status = CI_ERROR;
		core->deinit(core);
	}
	core->setVideoBuffer(core, image.data, image.stride);
	mCoreConfigInit(&core->config, "cinema");

	unsigned limit = 3600;
	unsigned skip = 0;
	unsigned fail = 0;
	unsigned video = 0;
	const char* input = NULL;

	MutexLock(&configMutex);
	CInemaConfigGetUInt(&configTree, test->name, "frames", &limit);
	CInemaConfigGetUInt(&configTree, test->name, "skip", &skip);
	CInemaConfigGetUInt(&configTree, test->name, "fail", &fail);
	CInemaConfigGetUInt(&configTree, test->name, "video", &video);
	input = CInemaConfigGet(&configTree, test->name, "input");
	CInemaConfigLoad(&configTree, test->name, core);
	MutexUnlock(&configMutex);

	struct VFile* save = VFileMemChunk(NULL, 0);
	core->loadROM(core, rom);
	if (!core->loadSave(core, save)) {
		save->close(save);
	}
	core->rtc.override = RTC_FAKE_EPOCH;
	core->rtc.value = 1200000000;
	core->reset(core);

	test->status = CI_PASS;

	unsigned minFrame = core->frameCounter(core);
	size_t frame;
	for (frame = 0; frame < skip; ++frame) {
		core->runFrame(core);
	}
	core->currentVideoSize(core, &image.width, &image.height);

#ifdef USE_FFMPEG
	struct FFmpegDecoder decoder;
	struct FFmpegEncoder encoder;
	struct CInemaStream stream = {0};

	char baselineName[PATH_MAX];
	snprintf(baselineName, sizeof(baselineName), "%s" PATH_SEP "baseline.avi", test->directory);
	bool exists = access(baselineName, 0) == 0;

	if (video) {
		FFmpegEncoderInit(&encoder);
		FFmpegDecoderInit(&decoder);

		FFmpegEncoderSetAudio(&encoder, NULL, 0);
		FFmpegEncoderSetVideo(&encoder, "zmbv", 0, 0);
		FFmpegEncoderSetContainer(&encoder, "avi");
		FFmpegEncoderSetDimensions(&encoder, image.width, image.height);

		if (rebaseline && !exists) {
			if (!FFmpegEncoderOpen(&encoder, baselineName)) {
				CIerr(1, "Failed to save baseline video\n");
			} else {
				core->setAVStream(core, &encoder.d);
			}
		}

		if (exists) {
			stream.d.postVideoFrame = _cinemaVideoFrame;
			stream.d.videoDimensionsChanged = _cinemaDimensionsChanged;
			stream.status = &test->status;
			decoder.out = &stream.d;

			if (!FFmpegDecoderOpen(&decoder, baselineName)) {
				CIerr(1, "Failed to load baseline video\n");
			}
		} else if (!rebaseline) {
			test->status = CI_FAIL;
		}
	}
#else
	if (video) {
		CIerr(0, "Failed to run video test without ffmpeg linked in\n");
		test->status = CI_ERROR;
	}
#endif

	bool xdiff = false;
	for (frame = 0; limit; ++frame, --limit) {
		_updateInput(core, frame, &input);
		if (setjmp(test->errorCtx)) {
			break;
		}
		core->runFrame(core);
		++test->totalFrames;
		unsigned frameCounter = core->frameCounter(core);
		if (frameCounter <= minFrame) {
			break;
		}
		if (test->status == CI_ERROR) {
			break;
		}
		CIlog(3, "Test frame: %u\n", frameCounter);
		core->currentVideoSize(core, &image.width, &image.height);
		uint8_t* diff = NULL;
		struct CInemaImage expected = {
			.data = NULL,
			.width = image.width,
			.height = image.height,
			.stride = image.width,
		};
		bool baselineFound;
		if (video) {
			baselineFound = false;
#ifdef USE_FFMPEG
			if (FFmpegDecoderIsOpen(&decoder)) {
				stream.image = &expected;
				while (!expected.data) {
					if (!FFmpegDecoderRead(&decoder)) {
						CIlog(1, "Failed to read more frames. EOF?\n");
						test->status = CI_FAIL;
						if (rebaseline && !FFmpegEncoderIsOpen(&encoder)) {
							_replayBaseline(test, &encoder, &image, frame);
							if (test->status == CI_ERROR) {
								break;
							}
							encoder.d.postVideoFrame(&encoder.d, image.data, image.stride);
							core->setAVStream(core, &encoder.d);
						}
						break;
					}
				}
				baselineFound = expected.data;
			}
#endif
		} else {
			baselineFound = _loadBaselinePNG(dir, "baseline", &expected, frame, &test->status);
		}
		if (test->status == CI_ERROR) {
			break;
		}
		bool failed = false;
		if (baselineFound) {
			int max = 0;
			failed = !_compareImages(test, &image, &expected, &max, diffs ? &diff : NULL);
			if (failed) {
				++test->failedFrames;
#ifdef USE_FFMPEG
				if (video && exists && rebaseline && !FFmpegEncoderIsOpen(&encoder)) {
					_replayBaseline(test, &encoder, &image, frame);
					if (test->status == CI_ERROR) {
						break;
					}
					encoder.d.postVideoFrame(&encoder.d, image.data, image.stride);
					core->setAVStream(core, &encoder.d);
				}
#endif
			}
			test->totalPixels += image.height * image.width;
			if (rebaseline == CI_R_FAILING && !video && failed) {
				_writeBaseline(dir, "baseline", &image, frame);
			}
			if (diff) {
				if (failed) {
					if (is4Up) {
						_write4UpDiff(&expected, &image, test->name, diff, frame, max);
					} else {
						_writeDiff(test->name, &image, frame, "result");
						_writeDiffSet(&expected, test->name, diff, frame, max, false);
					}
				}
				free(diff);
				diff = NULL;
			}
			free(expected.data);
		} else if (rebaseline && !video) {
			_writeBaseline(dir, "baseline", &image, frame);
		} else if (!rebaseline) {
			test->status = CI_FAIL;
		}

		if (fail && failed) {
			if (video) {
				// TODO
				baselineFound = false;
			} else {
				baselineFound = _loadBaselinePNG(dir, "xbaseline", &expected, frame, &test->status);
			}

			if (baselineFound) {
				int max = 0;
				failed = !_compareImages(test, &image, &expected, &max, diffs ? &diff : NULL);
				if (diff) {
					if (failed) {
						_writeDiffSet(&expected, test->name, diff, frame, max, true);
					}
					free(diff);
					diff = NULL;
				}
				if (failed) {
					if (xbaseline == CI_R_FAILING && !video) {
						_writeBaseline(dir, "xbaseline", &image, frame);
					}
					xdiff = true;
				}
				free(expected.data);
			} else if (xbaseline && !video) {
				_writeBaseline(dir, "xbaseline", &image, frame);
			}
		}
	}

#ifdef USE_FFMPEG
	if (video) {
		if (FFmpegEncoderIsOpen(&encoder)) {
			FFmpegEncoderClose(&encoder);
			if (exists && rebaseline) {
				char tmpBaselineName[PATH_MAX];
				snprintf(tmpBaselineName, sizeof(tmpBaselineName), "%s" PATH_SEP ".baseline.avi", test->directory);
#ifdef _WIN32
				MoveFileEx(tmpBaselineName, baselineName, MOVEFILE_REPLACE_EXISTING);
#else
				rename(tmpBaselineName, baselineName);
#endif
			}
		}
		if (FFmpegDecoderIsOpen(&decoder)) {
			FFmpegDecoderClose(&decoder);
		}
	}
#endif

	if (fail) {
		if (test->status == CI_FAIL && !xdiff) {
			test->status = CI_XFAIL;
		} else if (test->status == CI_PASS) {
			test->status = CI_XPASS;
		}
	}

	free(image.data);
	mCoreConfigDeinit(&core->config);
	core->deinit(core);
	dir->close(dir);
}

static bool CInemaTask(struct CInemaTestList* tests, size_t i) {
	bool success = true;
	struct CInemaTest* test = CInemaTestListGetPointer(tests, i);
	if (dryRun) {
		CIlog(-1, "%s\n", test->name);
	} else {
		CIlog(1, "%s: ", test->name);
		fflush(stdout);
		ThreadLocalSetKey(currentTest, test);
		CInemaTestRun(test);
		ThreadLocalSetKey(currentTest, NULL);

		switch (test->status) {
		case CI_PASS:
			CIlog(1, "pass\n");
			break;
		case CI_FAIL:
			success = false;
			CIlog(1, "fail\n");
			break;
		case CI_XPASS:
			CIlog(1, "xpass\n");
			break;
		case CI_XFAIL:
			CIlog(1, "xfail\n");
			break;
		case CI_SKIP:
			CIlog(1, "skip\n");
			break;
		case CI_ERROR:
			success = false;
			CIlog(1, "error\n");
			break;
		}
		if (test->failedFrames) {
			CIlog(2, "\tfailed frames: %u/%u (%1.3g%%)\n", test->failedFrames, test->totalFrames, test->failedFrames / (test->totalFrames * 0.01));
			CIlog(2, "\tfailed pixels: %" PRIu64 "/%" PRIu64 " (%1.3g%%)\n", test->failedPixels, test->totalPixels, test->failedPixels / (test->totalPixels * 0.01));
			CIlog(2, "\tdistance: %" PRIu64 "/%" PRIu64 " (%1.3g%%)\n", test->totalDistance, test->totalPixels * 765, test->totalDistance / (test->totalPixels * 7.65));
		}
	}
	return success;
}

static THREAD_ENTRY CInemaJob(void* context) {
	struct CInemaTestList* tests = context;
	struct CInemaLogStream stream;
	StringListInit(&stream.out.lines, 0);
	StringListInit(&stream.out.partial, 0);
	stream.out.repeat = 0;
	StringListInit(&stream.err.lines, 0);
	StringListInit(&stream.err.partial, 0);
	stream.err.repeat = 0;
	ThreadLocalSetKey(logStream, &stream);

	bool success = true;
	while (true) {
		size_t i;
		MutexLock(&jobMutex);
		i = jobIndex;
		++jobIndex;
		MutexUnlock(&jobMutex);
		if (i >= CInemaTestListSize(tests)) {
			break;
		}
		if (!CInemaTask(tests, i)) {
			success = false;
		}
		CIflush(&stream.out, stdout);
		CIflush(&stream.err, stderr);
	}
	MutexLock(&jobMutex);
	if (!success) {
		jobStatus = 1;
	}
	MutexUnlock(&jobMutex);

	CIflush(&stream.out, stdout);
	StringListDeinit(&stream.out.lines);
	StringListDeinit(&stream.out.partial);

	CIflush(&stream.err, stderr);
	StringListDeinit(&stream.err.lines);
	StringListDeinit(&stream.err.partial);
}

void _log(struct mLogger* log, int category, enum mLogLevel level, const char* format, va_list args) {
	UNUSED(log);
	if (level == mLOG_FATAL) {
		struct CInemaTest* test = ThreadLocalGetValue(currentTest);
		test->status = CI_ERROR;
	}
	if (verbosity < 0) {
		return;
	}
	int mask = mLOG_FATAL;
	if (verbosity >= 1) {
		mask |= mLOG_ERROR;
	}
	if (verbosity >= 2) {
		mask |= mLOG_WARN;
	}
	if (verbosity >= 4) {
		mask |= mLOG_INFO;
	}
	if (verbosity >= 5) {
		mask |= mLOG_ALL;
	}
	if (!(mask & level)) {
		return;
	}

	char buffer[256];
	vsnprintf(buffer, sizeof(buffer), format, args);
	CIerr(0, "[%s] %s\n", mLogCategoryName(category), buffer);
}

#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
static void _signalHandler(int signal, siginfo_t* info, void* context) {
	UNUSED(info);
	UNUSED(context);
	struct CInemaTest* test = ThreadLocalGetValue(currentTest);
	if (test) {
		test->status = CI_ERROR;
		CIerr(0, "Test %s crashed with signal %i\n", test->name, signal);
	} else {
		CIerr(0, "Thread crashed with signal %i\n", signal);
	}
	longjmp(test->errorCtx, -1);
}
#endif

int main(int argc, char** argv) {
	ThreadLocalInitKey(&logStream);
	ThreadLocalSetKey(logStream, NULL);
	putenv("TZ=UTC");

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
		CIlog(0, "Could not determine CInema test base. Please specify manually.");
		status = 1;
		goto cleanup;
	}
#ifndef _WIN32
	char* rbase = realpath(base, NULL);
	if (rbase) {
		strlcpy(base, rbase, sizeof(base));
		free(rbase);
	}
#endif

	struct CInemaTestList tests;
	CInemaTestListInit(&tests, 0);

	struct mLogger logger = { .log = _log };
	mLogSetDefaultLogger(&logger);
#ifdef USE_FFMPEG
	if (verbosity < 2) {
		av_log_set_level(AV_LOG_ERROR);
	}
#endif

	if (argc > 0) {
		size_t i;
		for (i = 0; i < (size_t) argc; ++i) {
			if (strchr(argv[i], '*')) {
				if (!globTests(&tests, argv[i], NULL)) {
					status = 1;
					break;
				}
				continue;
			}
			char path[PATH_MAX + 1] = {0};
			testToPath(argv[i], path);

			if (!collectTests(&tests, path)) {
				status = 1;
				break;
			}
		}
	} else if (!collectTests(&tests, base)) {
		status = 1;
	}

	if (CInemaTestListSize(&tests) == 0) {
		CIlog(1, "No tests found.\n");
		status = 1;
	} else {
		reduceTestList(&tests);
	}

	HashTableInit(&configTree, 0, free);
	MutexInit(&configMutex);
	ThreadLocalInitKey(&currentTest);
	ThreadLocalSetKey(currentTest, NULL);

	if (jobs == 1) {
		size_t i;
		for (i = 0; i < CInemaTestListSize(&tests); ++i) {
			bool success = CInemaTask(&tests, i);
			if (!success) {
				status = 1;
			}
		}
	} else {
		MutexInit(&jobMutex);
		int i;

#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
		struct sigaction sa = {
			.sa_flags = SA_SIGINFO,
			.sa_sigaction = _signalHandler,
		};
		sigemptyset(&sa.sa_mask);

		sigaction(SIGSEGV, &sa, NULL);
		sigaction(SIGBUS, &sa, NULL);
		sigaction(SIGFPE, &sa, NULL);
		sigaction(SIGILL, &sa, NULL);
#endif

		for (i = 0; i < jobs; ++i) {
			ThreadCreate(&jobThreads[i], CInemaJob, &tests);
		}
		for (i = 0; i < jobs; ++i) {
			ThreadJoin(&jobThreads[i]);
		}

#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
		signal(SIGSEGV, SIG_DFL);
		signal(SIGBUS, SIG_DFL);
		signal(SIGFPE, SIG_DFL);
		signal(SIGILL, SIG_DFL);
#endif

		MutexDeinit(&jobMutex);
		status = jobStatus;
	}

	MutexDeinit(&configMutex);
	HashTableEnumerate(&configTree, _unloadConfigTree, NULL);
	HashTableDeinit(&configTree);
	CInemaTestListDeinit(&tests);

cleanup:
	return status;
}
