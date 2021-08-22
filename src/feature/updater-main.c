/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/config.h>
#include <mgba/feature/updater.h>
#include <mgba-util/vfs.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#include <synchapi.h>

#define mkdir(X, Y) _mkdir(X)
#elif defined(_POSIX_C_SOURCE)
#include <unistd.h>
#endif

#ifndef W_OK
#define W_OK 02
#endif

bool extractArchive(struct VDir* archive, const char* root) {
	char path[PATH_MAX] = {0};
	struct VDirEntry* vde;
	uint8_t block[8192];
	ssize_t size;
	while ((vde = archive->listNext(archive))) {
		struct VFile* vfIn;
		struct VFile* vfOut;
		const char* fname = strchr(vde->name(vde), '/');
		if (!fname) {
			continue;
		}
		snprintf(path, sizeof(path), "%s/%s", root, &fname[1]);
		switch (vde->type(vde)) {
		case VFS_DIRECTORY:
			printf("mkdir   %s\n", fname);
			if (mkdir(path, 0755) < 0 && errno != EEXIST) {
				return false;
			}
			break;
		case VFS_FILE:
			printf("extract %s\n", fname);
			vfIn = archive->openFile(archive, vde->name(vde), O_RDONLY);
			errno = 0;
			vfOut = VFileOpen(path, O_WRONLY | O_CREAT | O_TRUNC);
			if (!vfOut && errno == EACCES) {
#ifdef _WIN32
				Sleep(1000);
#else
				sleep(1);
#endif
				vfOut = VFileOpen(path, O_WRONLY | O_CREAT | O_TRUNC);
			}
			if (!vfOut) {
				vfIn->close(vfIn);
				return false;
			}
			while ((size = vfIn->read(vfIn, block, sizeof(block))) > 0) {
				vfOut->write(vfOut, block, size);
			}
			vfOut->close(vfOut);
			vfIn->close(vfIn);
			if (size < 0) {
				return false;
			}
			break;
		case VFS_UNKNOWN:
			return false;
		}
	}
	return true;
}

int main(int argc, char* argv[]) {
	UNUSED(argc);
	UNUSED(argv);
	struct mCoreConfig config;
	char updateArchive[PATH_MAX] = {0};
	const char* root;
	int ok = 1;

	mCoreConfigInit(&config, "updater");
	if (!mCoreConfigLoad(&config)) {
		puts("Failed to load config");
	} else if (!mUpdateGetArchivePath(&config, updateArchive, sizeof(updateArchive)) || !(root = mUpdateGetRoot(&config))) {
		puts("No pending update found");
	} else if (access(root, W_OK)) {
		puts("Cannot write to update path");
	} else {
		bool isPortable = mCoreConfigIsPortable();
		struct VDir* archive = VDirOpenArchive(updateArchive);
		if (!archive) {
			puts("Cannot open update archive");
		} else {
			puts("Extracting update");
			if (extractArchive(archive, root)) {
				puts("Complete");
				ok = 0;
				mUpdateDeregister(&config);
			} else {
				puts("An error occurred");
			}
			archive->close(archive);
			unlink(updateArchive);
		}
		if (!isPortable) {
			char portableIni[PATH_MAX] = {0};
			snprintf(portableIni, sizeof(portableIni), "%s/portable.ini", root);
			unlink(portableIni);
		}
	}
	const char* bin = mUpdateGetCommand(&config);
	mCoreConfigDeinit(&config);
	if (ok == 0) {
		const char* argv[] = { bin, NULL };
#ifdef _WIN32
		_execv(bin, argv);
#elif defined(_POSIX_C_SOURCE)
		execv(bin, argv);
#endif
	}
	return 1;
}

#ifdef _WIN32
#include <mgba-util/string.h>
#include <mgba-util/vector.h>

int wmain(int argc, wchar_t* argv[]) {
	struct StringList argv8;
	StringListInit(&argv8, argc);
	for (int i = 0; i < argc; ++i) {
		*StringListAppend(&argv8) = utf16to8((uint16_t*) argv[i], wcslen(argv[i]) * 2);
	}
	int ret = main(argc, StringListGetPointer(&argv8, 0));

	size_t i;
	for (i = 0; i < StringListSize(&argv8); ++i) {
		free(*StringListGetPointer(&argv8, i));
	}
	return ret;
}
#endif
