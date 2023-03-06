/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CoreManager.h"

#include "CoreController.h"
#include "LogController.h"
#include "VFileDevice.h"

#include <QDir>

#ifdef M_CORE_GBA
#include <mgba/gba/core.h>
#endif

#include <mgba/core/core.h>
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

using namespace QGBA;

void CoreManager::setConfig(const mCoreConfig* config) {
	m_config = config;
}

void CoreManager::setMultiplayerController(MultiplayerController* multiplayer) {
	m_multiplayer = multiplayer;
}

CoreController* CoreManager::loadGame(const QString& path) {
	QFileInfo info(path);
	if (!info.isReadable()) {
		// Open specific file in archive
		QString fname = info.fileName();
		QString base = info.path();
		if (base.endsWith("/") || base.endsWith(QDir::separator())) {
			base.chop(1);
		}
		VDir* dir = VDirOpenArchive(base.toUtf8().constData());
		if (dir) {
			VFile* vf = dir->openFile(dir, fname.toUtf8().constData(), O_RDONLY);
			if (vf) {
				struct VFile* vfclone = VFileDevice::openMemory(vf->size(vf));
				VFileDevice::copyFile(vf, vfclone);
				vf = vfclone;
			}
			dir->close(dir);
			return loadGame(vf, fname, base);
		} else {
			LOG(QT, ERROR) << tr("Failed to open game file: %1").arg(path);
		}
		return nullptr;
	}
	VFile* vf = nullptr;
	VDir* archive = VDirOpenArchive(path.toUtf8().constData());
	if (archive) {
		// Open first file in archive
		VFile* vfOriginal = VDirFindFirst(archive, [](VFile* vf) {
			return mCoreIsCompatible(vf) != mPLATFORM_NONE;
		});
		if (vfOriginal) {
			ssize_t size = vfOriginal->size(vfOriginal);
			if (size > 0) {
				struct VFile* vfclone = VFileDevice::openMemory(vfOriginal->size(vfOriginal));
				VFileDevice::copyFile(vfOriginal, vfclone);
				vf = vfclone;
			}
			vfOriginal->close(vfOriginal);
		}
		archive->close(archive);
	}
	QDir dir(info.dir());
	if (!vf) {
		// Open bare file
		vf = VFileOpen(info.canonicalFilePath().toUtf8().constData(), O_RDONLY);
	}
	return loadGame(vf, info.fileName(), dir.canonicalPath());
}

CoreController* CoreManager::loadGame(VFile* vf, const QString& path, const QString& base) {
	if (!vf) {
		return nullptr;
	}

	mCore* core = mCoreFindVF(vf);
	if (!core) {
		vf->close(vf);
		LOG(QT, ERROR) << tr("Could not load game. Are you sure it's in the correct format?");
		return nullptr;
	}

	core->init(core);
	mCoreInitConfig(core, nullptr);

	if (m_config) {
		mCoreLoadForeignConfig(core, m_config);
	}

	if (m_preload) {
		mCorePreloadVF(core, vf);
	} else {
		core->loadROM(core, vf);
	}

	QByteArray bytes(path.toUtf8());
	separatePath(bytes.constData(), nullptr, core->dirs.baseName, nullptr);

	QFileInfo info(base);
	if (info.isDir()) {
		info = QFileInfo(base + "/" + path);
	}
	bytes = info.dir().canonicalPath().toUtf8();
	mDirectorySetAttachBase(&core->dirs, VDirOpen(bytes.constData()));
	if (!mCoreAutoloadSave(core)) {
		LOG(QT, ERROR) << tr("Failed to open save file; in-game saves cannot be updated. Please ensure the save directory is writable without additional privileges (e.g. UAC on Windows).");
	}
	mCoreAutoloadCheats(core);

	CoreController* cc = new CoreController(core);
	if (m_multiplayer) {
		cc->setMultiplayerController(m_multiplayer);
	}
	emit coreLoaded(cc);
	return cc;
}

CoreController* CoreManager::loadBIOS(int platform, const QString& path) {
	QFileInfo info(path);
	VFile* vf = VFileOpen(info.canonicalFilePath().toUtf8().constData(), O_RDONLY);
	if (!vf) {
		return nullptr;
	}

	mCore* core = nullptr;
	switch (platform) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		core = GBACoreCreate();
		break;
#endif
	default:
		vf->close(vf);
		return nullptr;
	}
	if (!core) {
		vf->close(vf);
		return nullptr;
	}

	core->init(core);
	mCoreInitConfig(core, nullptr);

	if (m_config) {
		mCoreLoadForeignConfig(core, m_config);
	}

	core->loadBIOS(core, vf, 0);

	mCoreConfigSetOverrideIntValue(&core->config, "useBios", 1);
	mCoreConfigSetOverrideIntValue(&core->config, "skipBios", 0);

	QByteArray bytes(info.baseName().toUtf8());
	strlcpy(core->dirs.baseName, bytes.constData(), sizeof(core->dirs.baseName));

	bytes = info.dir().canonicalPath().toUtf8();
	mDirectorySetAttachBase(&core->dirs, VDirOpen(bytes.constData()));

	CoreController* cc = new CoreController(core);
	if (m_multiplayer) {
		cc->setMultiplayerController(m_multiplayer);
	}
	emit coreLoaded(cc);
	return cc;
}
