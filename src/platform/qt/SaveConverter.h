/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QDialog>

#include "CoreController.h"
#include "utils.h"

#ifdef M_CORE_GBA
#include <mgba/gba/core.h>
#include <mgba/internal/gba/savedata.h>
#endif
#ifdef M_CORE_GB
#include <mgba/gb/core.h>
#include <mgba/gb/interface.h>
#endif

#include <mgba/core/serialize.h>

#include "ui_SaveConverter.h"

struct VFile;

namespace QGBA {

class SaveConverter : public QDialog {
Q_OBJECT

public:
	SaveConverter(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);

	static mPlatform getStatePlatform(VFile*);
	static QByteArray getState(VFile*, mPlatform);
	static QByteArray getExtdata(VFile*, mPlatform, mStateExtdataTag);

public slots:
	void convert();

private slots:
	void refreshInputTypes();
	void refreshOutputTypes();
	void checkCanConvert();

private:
#ifdef M_CORE_GBA
	struct GBASave {
		SavedataType type;
	};
#endif
#ifdef M_CORE_GB
	struct GBSave {
		GBMemoryBankControllerType type;
	};
#endif
	enum class Container {
		NONE = 0,
		SAVESTATE,
		SHARKPORT,
		GSV
	};
	struct AnnotatedSave {
		AnnotatedSave();
		AnnotatedSave(mPlatform, std::shared_ptr<VFileDevice>, Endian = Endian::NONE, Container = Container::NONE);
#ifdef M_CORE_GBA
		AnnotatedSave(SavedataType, std::shared_ptr<VFileDevice>, Endian = Endian::NONE, Container = Container::NONE);
#endif
#ifdef M_CORE_GB
		AnnotatedSave(GBMemoryBankControllerType, std::shared_ptr<VFileDevice>, Endian = Endian::NONE, Container = Container::NONE);
#endif

		AnnotatedSave asRaw() const;
		operator QString() const;
		bool operator==(const AnnotatedSave&) const;

		QList<AnnotatedSave> possibleConversions() const;
		QByteArray convertTo(const AnnotatedSave&) const;

		Container container;
		mPlatform platform;
		ssize_t size;
		std::shared_ptr<VFileDevice> backing;
		Endian endianness;
		union {
#ifdef M_CORE_GBA
			GBASave gba;
#endif
#ifdef M_CORE_GB
			GBSave gb;
#endif
		};
	};

	void detectFromSavestate(VFile*);
	void detectFromSize(std::shared_ptr<VFileDevice>);
	void detectFromHeaders(std::shared_ptr<VFileDevice>);

	Ui::SaveConverter m_ui;

	std::shared_ptr<CoreController> m_controller;
	QList<AnnotatedSave> m_validSaves;
	QList<AnnotatedSave> m_validOutputs;
};

}
