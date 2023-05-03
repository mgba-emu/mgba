/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "ForwarderGenerator.h"

struct VDir;
struct VFile;

namespace QGBA {

class ForwarderGeneratorVita final : public ForwarderGenerator {
Q_OBJECT

public:
	ForwarderGeneratorVita();

	QList<QPair<QString, QSize>> imageTypes() const override;
	System system() const override { return System::VITA; }
	QString extension() const override { return QLatin1String("vpk"); }

	void rebuild(const QString& source, const QString& target) override;

private:
	bool copyAssets(const QString& vpk, VDir* out);
	QString makeSerial() const;
	void writeSfo(VFile* out);
	void injectImage(VDir* out, const char* name, int index);
};

}
