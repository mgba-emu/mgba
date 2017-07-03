/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_INPUT_PROFILE
#define QGBA_INPUT_PROFILE

#include "GamepadAxisEvent.h"
#include "InputIndex.h"

#include <mgba/core/core.h>
#include <mgba/gba/interface.h>

#include <QRegExp>

class QSettings;

namespace QGBA {

class InputController;

class InputProfile {
public:
	constexpr static const char* const PROFILE_SECTION = "profiles";
	constexpr static const char* const MATCH_SECTION = "match";
	constexpr static const char* const KEY_SECTION = "keys";
	constexpr static const char* const BUTTON_SECTION = "buttons";
	constexpr static const char* const AXIS_SECTION = "axes";
	constexpr static const char* const HAT_SECTION = "hats";

	static const InputProfile* findProfile(const QString& name);
	static void loadProfiles(const QString& path);

	void apply(InputController*) const;
private:
	InputProfile(const QString&);

	struct Coord {
		int x;
		int y;
	};

	static void loadDefaultProfiles();
	static void loadProfile(QSettings&, const QString& name);

	static QList<InputProfile> s_profiles;

	QString m_profileName;
	QList<QRegExp> m_match;

	Coord m_tiltAxis = { 2, 3 };
	Coord m_gyroAxis = { 0, 1 };
	float m_gyroSensitivity = 2e+09f;
	InputIndex m_inputIndex;
};

}

#endif
