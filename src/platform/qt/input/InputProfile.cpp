/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputProfile.h"

#include "InputController.h"

#include <QSettings>

using namespace QGBA;

QList<InputProfile> InputProfile::s_profiles;

InputProfile::InputProfile(const QString& name)
	: m_profileName(name)
{
}

void InputProfile::loadDefaultProfiles() {
	loadProfiles(":/input/default-profiles.ini");
}

void InputProfile::loadProfiles(const QString& path) {
	QSettings profileIni(path, QSettings::IniFormat);

	for (const auto& group : profileIni.childGroups()) {
	}
	profileIni.beginGroup(PROFILE_SECTION);
	for (const auto& group : profileIni.childGroups()) {
		loadProfile(profileIni, group);
	}
	profileIni.endGroup();
}

void InputProfile::loadProfile(QSettings& profileIni, const QString& name) {
	profileIni.beginGroup(name);
	s_profiles.append(name);
	InputProfile& profile = s_profiles.last();
	for (const auto& group : profileIni.childGroups()) {
		profileIni.beginGroup(group);
		if (group == MATCH_SECTION) {
			for (const auto& key : profileIni.childKeys()) {
				profile.m_match.append(QRegExp(profileIni.value(key).toString()));
			}
		}
		for (const auto& key : profileIni.childKeys()) {
			InputItem* item = profile.m_inputIndex.itemAt(key);
			if (!item) {
				item = profile.m_inputIndex.addItem(QString(), key);
			}
			if (group == BUTTON_SECTION) {
				item->setButton(profileIni.value(key).toInt());
			}
			if (group == AXIS_SECTION) {
				QString axisDescription = profileIni.value(key).toString();
				GamepadAxisEvent::Direction direction = GamepadAxisEvent::POSITIVE;
				int axis = profileIni.value(key).toInt();
				if (axisDescription[0] == '-') {
					direction = GamepadAxisEvent::NEGATIVE;
					axis = -axis;
				}

				item->setAxis(axis, direction);
			}
			if (group == KEY_SECTION) {
				item->setShortcut(profileIni.value(key).toInt());
			}
		}
		profileIni.endGroup();
	}
	profile.m_inputIndex.rebuild();
	profileIni.endGroup();
}

const InputProfile* InputProfile::findProfile(const QString& name) {
	if (s_profiles.isEmpty()) {
		loadDefaultProfiles();
	}
	for (const InputProfile& profile : s_profiles) {
		for (const auto& match : profile.m_match) {
			if (match.exactMatch(name)) {
				return &profile;
			}
		}
	}
	return nullptr;
}

void InputProfile::apply(InputController* controller) const {
	controller->rebuildIndex(&m_inputIndex);
	controller->rebuildKeyIndex(&m_inputIndex);
	controller->registerTiltAxisX(m_tiltAxis.x);
	controller->registerTiltAxisY(m_tiltAxis.y);
	controller->registerGyroAxisX(m_gyroAxis.x);
	controller->registerGyroAxisY(m_gyroAxis.y);
	controller->setGyroSensitivity(m_gyroSensitivity);
}
