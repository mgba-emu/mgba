/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QFileInfo>
#include <QObject>
#include <QString>

struct mCoreConfig;
struct VFile;

namespace QGBA {

class CoreController;
class MultiplayerController;

class CoreManager : public QObject {
Q_OBJECT

public:
	void setConfig(const mCoreConfig*);
	void setMultiplayerController(MultiplayerController*);
	void setPreload(bool preload) { m_preload = preload; }

public slots:
	CoreController* loadGame(const QString& path);
	CoreController* loadGame(VFile* vf, const QString& path, const QString& base);
	CoreController* loadBIOS(int platform, const QString& path);

signals:
	void coreLoaded(CoreController*);

private:
	const mCoreConfig* m_config = nullptr;
	MultiplayerController* m_multiplayer = nullptr;
	bool m_preload = false;
};

}
