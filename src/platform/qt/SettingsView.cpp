/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "SettingsView.h"

#include "AudioProcessor.h"
#include "ConfigController.h"
#include "Display.h"
#include "GBAApp.h"
#include "GBAKeyEditor.h"
#include "InputController.h"
#include "ShaderSelector.h"
#include "ShortcutView.h"

#include <mgba/core/serialize.h>
#include <mgba/core/version.h>
#include <mgba/internal/gba/gba.h>

using namespace QGBA;

SettingsView::SettingsView(ConfigController* controller, InputController* inputController, ShortcutController* shortcutController, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	reloadConfig();

	if (m_ui.savegamePath->text().isEmpty()) {
		m_ui.savegameSameDir->setChecked(true);
	}
	connect(m_ui.savegameSameDir, &QAbstractButton::toggled, [this] (bool e) {
		if (e) {
			m_ui.savegamePath->clear();
		}
	});
	connect(m_ui.savegameBrowse, &QAbstractButton::pressed, [this] () {
		QString path = GBAApp::app()->getOpenDirectoryName(this, "Select directory");
		if (!path.isNull()) {
			m_ui.savegameSameDir->setChecked(false);
			m_ui.savegamePath->setText(path);
		}
	});

	if (m_ui.savestatePath->text().isEmpty()) {
		m_ui.savestateSameDir->setChecked(true);
	}
	connect(m_ui.savestateSameDir, &QAbstractButton::toggled, [this] (bool e) {
		if (e) {
			m_ui.savestatePath->clear();
		}
	});
	connect(m_ui.savestateBrowse, &QAbstractButton::pressed, [this] () {
		QString path = GBAApp::app()->getOpenDirectoryName(this, "Select directory");
		if (!path.isNull()) {
			m_ui.savestateSameDir->setChecked(false);
			m_ui.savestatePath->setText(path);
		}
	});

	if (m_ui.screenshotPath->text().isEmpty()) {
		m_ui.screenshotSameDir->setChecked(true);
	}
	connect(m_ui.screenshotSameDir, &QAbstractButton::toggled, [this] (bool e) {
		if (e) {
			m_ui.screenshotPath->clear();
		}
	});
	connect(m_ui.screenshotBrowse, &QAbstractButton::pressed, [this] () {
		QString path = GBAApp::app()->getOpenDirectoryName(this, "Select directory");
		if (!path.isNull()) {
			m_ui.screenshotSameDir->setChecked(false);
			m_ui.screenshotPath->setText(path);
		}
	});

	if (m_ui.patchPath->text().isEmpty()) {
		m_ui.patchSameDir->setChecked(true);
	}
	connect(m_ui.patchSameDir, &QAbstractButton::toggled, [this] (bool e) {
		if (e) {
			m_ui.patchPath->clear();
		}
	});
	connect(m_ui.patchBrowse, &QAbstractButton::pressed, [this] () {
		QString path = GBAApp::app()->getOpenDirectoryName(this, "Select directory");
		if (!path.isNull()) {
			m_ui.patchSameDir->setChecked(false);
			m_ui.patchPath->setText(path);
		}
	});
	connect(m_ui.clearCache, &QAbstractButton::pressed, this, &SettingsView::libraryCleared);

	// TODO: Move to reloadConfig()
	QVariant audioDriver = m_controller->getQtOption("audioDriver");
#ifdef BUILD_QT_MULTIMEDIA
	m_ui.audioDriver->addItem(tr("Qt Multimedia"), static_cast<int>(AudioProcessor::Driver::QT_MULTIMEDIA));
	if (!audioDriver.isNull() && audioDriver.toInt() == static_cast<int>(AudioProcessor::Driver::QT_MULTIMEDIA)) {
		m_ui.audioDriver->setCurrentIndex(m_ui.audioDriver->count() - 1);
	}
#endif

#ifdef BUILD_SDL
	m_ui.audioDriver->addItem(tr("SDL"), static_cast<int>(AudioProcessor::Driver::SDL));
	if (audioDriver.isNull() || audioDriver.toInt() == static_cast<int>(AudioProcessor::Driver::SDL)) {
		m_ui.audioDriver->setCurrentIndex(m_ui.audioDriver->count() - 1);
	}
#endif

	// TODO: Move to reloadConfig()
	QVariant displayDriver = m_controller->getQtOption("displayDriver");
	m_ui.displayDriver->addItem(tr("Software (Qt)"), static_cast<int>(Display::Driver::QT));
	if (!displayDriver.isNull() && displayDriver.toInt() == static_cast<int>(Display::Driver::QT)) {
		m_ui.displayDriver->setCurrentIndex(m_ui.displayDriver->count() - 1);
	}

#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(USE_EPOXY)
	m_ui.displayDriver->addItem(tr("OpenGL"), static_cast<int>(Display::Driver::OPENGL));
	if (displayDriver.isNull() || displayDriver.toInt() == static_cast<int>(Display::Driver::OPENGL)) {
		m_ui.displayDriver->setCurrentIndex(m_ui.displayDriver->count() - 1);
	}
#endif

#ifdef BUILD_GL
	m_ui.displayDriver->addItem(tr("OpenGL (force version 1.x)"), static_cast<int>(Display::Driver::OPENGL1));
	if (!displayDriver.isNull() && displayDriver.toInt() == static_cast<int>(Display::Driver::OPENGL1)) {
		m_ui.displayDriver->setCurrentIndex(m_ui.displayDriver->count() - 1);
	}
#endif

	connect(m_ui.gbaBiosBrowse, &QPushButton::clicked, [this]() {
		selectBios(m_ui.gbaBios);
	});
	connect(m_ui.gbBiosBrowse, &QPushButton::clicked, [this]() {
		selectBios(m_ui.gbBios);
	});
	connect(m_ui.gbcBiosBrowse, &QPushButton::clicked, [this]() {
		selectBios(m_ui.gbcBios);
	});

	GBAKeyEditor* editor = new GBAKeyEditor(inputController, InputController::KEYBOARD, QString(), this);
	m_ui.stackedWidget->addWidget(editor);
	m_ui.tabs->addItem(tr("Keyboard"));
	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, editor, &GBAKeyEditor::save);

	GBAKeyEditor* buttonEditor = nullptr;
#ifdef BUILD_SDL
	inputController->recalibrateAxes();
	const char* profile = inputController->profileForType(SDL_BINDING_BUTTON);
	buttonEditor = new GBAKeyEditor(inputController, SDL_BINDING_BUTTON, profile);
	m_ui.stackedWidget->addWidget(buttonEditor);
	m_ui.tabs->addItem(tr("Controllers"));
	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, buttonEditor, &GBAKeyEditor::save);
#endif

	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &SettingsView::updateConfig);
	connect(m_ui.buttonBox, &QDialogButtonBox::clicked, [this, editor, buttonEditor](QAbstractButton* button) {
		if (m_ui.buttonBox->buttonRole(button) == QDialogButtonBox::ApplyRole) {
			updateConfig();
			editor->save();
			if (buttonEditor) {
				buttonEditor->save();
			}
		}
	});

	m_ui.languages->setItemData(0, QLocale("en"));
	QDir ts(":/translations/");
	for (auto name : ts.entryList()) {
		if (!name.endsWith(".qm") || !name.startsWith(binaryName)) {
			continue;
		}
		QLocale locale(name.remove(QString("%0-").arg(binaryName)).remove(".qm"));
		m_ui.languages->addItem(locale.nativeLanguageName(), locale);
		if (locale.bcp47Name() == QLocale().bcp47Name()) {
			m_ui.languages->setCurrentIndex(m_ui.languages->count() - 1);
		}
	}

	ShortcutView* shortcutView = new ShortcutView();
	shortcutView->setController(shortcutController);
	shortcutView->setInputController(inputController);
	m_ui.stackedWidget->addWidget(shortcutView);
	m_ui.tabs->addItem(tr("Shortcuts"));
}

SettingsView::~SettingsView() {
#if defined(BUILD_GL) || defined(BUILD_GLES2)
	if (m_shader) {
		m_ui.stackedWidget->removeWidget(m_shader);
		m_shader->setParent(nullptr);
	}
#endif
}

void SettingsView::setShaderSelector(ShaderSelector* shaderSelector) {
#if defined(BUILD_GL) || defined(BUILD_GLES2)
	m_shader = shaderSelector;
	m_ui.stackedWidget->addWidget(m_shader);
	m_ui.tabs->addItem(tr("Shaders"));
	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, m_shader, &ShaderSelector::saved);
#endif
}

void SettingsView::selectBios(QLineEdit* bios) {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select BIOS"));
	if (!filename.isEmpty()) {
		bios->setText(filename);
	}
}

void SettingsView::updateConfig() {
	saveSetting("gba.bios", m_ui.gbaBios);
	saveSetting("gb.bios", m_ui.gbBios);
	saveSetting("gbc.bios", m_ui.gbcBios);
	saveSetting("useBios", m_ui.useBios);
	saveSetting("skipBios", m_ui.skipBios);
	saveSetting("audioBuffers", m_ui.audioBufferSize);
	saveSetting("sampleRate", m_ui.sampleRate);
	saveSetting("videoSync", m_ui.videoSync);
	saveSetting("audioSync", m_ui.audioSync);
	saveSetting("frameskip", m_ui.frameskip);
	saveSetting("fpsTarget", m_ui.fpsTarget);
	saveSetting("lockAspectRatio", m_ui.lockAspectRatio);
	saveSetting("lockIntegerScaling", m_ui.lockIntegerScaling);
	saveSetting("volume", m_ui.volume);
	saveSetting("mute", m_ui.mute);
	saveSetting("rewindEnable", m_ui.rewind);
	saveSetting("rewindBufferCapacity", m_ui.rewindCapacity);
	saveSetting("rewindSave", m_ui.rewindSave);
	saveSetting("resampleVideo", m_ui.resampleVideo);
	saveSetting("allowOpposingDirections", m_ui.allowOpposingDirections);
	saveSetting("suspendScreensaver", m_ui.suspendScreensaver);
	saveSetting("pauseOnFocusLost", m_ui.pauseOnFocusLost);
	saveSetting("savegamePath", m_ui.savegamePath);
	saveSetting("savestatePath", m_ui.savestatePath);
	saveSetting("screenshotPath", m_ui.screenshotPath);
	saveSetting("patchPath", m_ui.patchPath);
	saveSetting("libraryStyle", m_ui.libraryStyle->currentIndex());
	saveSetting("showLibrary", m_ui.showLibrary);
	saveSetting("preload", m_ui.preload);

	if (m_ui.fastForwardUnbounded->isChecked()) {
		saveSetting("fastForwardRatio", "-1");
	} else {
		saveSetting("fastForwardRatio", m_ui.fastForwardRatio);
	}

	switch (m_ui.idleOptimization->currentIndex() + IDLE_LOOP_IGNORE) {
	case IDLE_LOOP_IGNORE:
		saveSetting("idleOptimization", "ignore");
		break;
	case IDLE_LOOP_REMOVE:
		saveSetting("idleOptimization", "remove");
		break;
	case IDLE_LOOP_DETECT:
		saveSetting("idleOptimization", "detect");
		break;
	}

	int loadState = SAVESTATE_RTC;
	loadState |= m_ui.loadStateScreenshot->isChecked() ? SAVESTATE_SCREENSHOT : 0;
	loadState |= m_ui.loadStateSave->isChecked() ? SAVESTATE_SAVEDATA : 0;
	loadState |= m_ui.loadStateCheats->isChecked() ? SAVESTATE_CHEATS : 0;
	saveSetting("loadStateExtdata", loadState);

	int saveState = SAVESTATE_RTC | SAVESTATE_METADATA;
	saveState |= m_ui.saveStateScreenshot->isChecked() ? SAVESTATE_SCREENSHOT : 0;
	saveState |= m_ui.saveStateSave->isChecked() ? SAVESTATE_SAVEDATA : 0;
	saveState |= m_ui.saveStateCheats->isChecked() ? SAVESTATE_CHEATS : 0;
	saveSetting("saveStateExtdata", saveState);

	QVariant audioDriver = m_ui.audioDriver->itemData(m_ui.audioDriver->currentIndex());
	if (audioDriver != m_controller->getQtOption("audioDriver")) {
		m_controller->setQtOption("audioDriver", audioDriver);
		AudioProcessor::setDriver(static_cast<AudioProcessor::Driver>(audioDriver.toInt()));
		emit audioDriverChanged();
	}

	QVariant displayDriver = m_ui.displayDriver->itemData(m_ui.displayDriver->currentIndex());
	if (displayDriver != m_controller->getQtOption("displayDriver")) {
		m_controller->setQtOption("displayDriver", displayDriver);
		Display::setDriver(static_cast<Display::Driver>(displayDriver.toInt()));
		emit displayDriverChanged();
	}

	QLocale language = m_ui.languages->itemData(m_ui.languages->currentIndex()).toLocale();
	if (language != m_controller->getQtOption("language").toLocale() && !(language.bcp47Name() == QLocale::system().bcp47Name() && m_controller->getQtOption("language").isNull())) {
		m_controller->setQtOption("language", language.bcp47Name());
		emit languageChanged();
	}

	m_controller->write();

	emit pathsChanged();
	emit biosLoaded(PLATFORM_GBA, m_ui.gbaBios->text());
}

void SettingsView::reloadConfig() {	
	loadSetting("bios", m_ui.gbaBios);
	loadSetting("gba.bios", m_ui.gbaBios);
	loadSetting("gb.bios", m_ui.gbBios);
	loadSetting("gbc.bios", m_ui.gbcBios);
	loadSetting("useBios", m_ui.useBios);
	loadSetting("skipBios", m_ui.skipBios);
	loadSetting("audioBuffers", m_ui.audioBufferSize);
	loadSetting("sampleRate", m_ui.sampleRate);
	loadSetting("videoSync", m_ui.videoSync);
	loadSetting("audioSync", m_ui.audioSync);
	loadSetting("frameskip", m_ui.frameskip);
	loadSetting("fpsTarget", m_ui.fpsTarget);
	loadSetting("lockAspectRatio", m_ui.lockAspectRatio);
	loadSetting("lockIntegerScaling", m_ui.lockIntegerScaling);
	loadSetting("volume", m_ui.volume);
	loadSetting("mute", m_ui.mute);
	loadSetting("rewindEnable", m_ui.rewind);
	loadSetting("rewindBufferCapacity", m_ui.rewindCapacity);
	loadSetting("rewindSave", m_ui.rewindSave);
	loadSetting("resampleVideo", m_ui.resampleVideo);
	loadSetting("allowOpposingDirections", m_ui.allowOpposingDirections);
	loadSetting("suspendScreensaver", m_ui.suspendScreensaver);
	loadSetting("pauseOnFocusLost", m_ui.pauseOnFocusLost);
	loadSetting("savegamePath", m_ui.savegamePath);
	loadSetting("savestatePath", m_ui.savestatePath);
	loadSetting("screenshotPath", m_ui.screenshotPath);
	loadSetting("patchPath", m_ui.patchPath);
	loadSetting("showLibrary", m_ui.showLibrary);
	loadSetting("preload", m_ui.preload);

	m_ui.libraryStyle->setCurrentIndex(loadSetting("libraryStyle").toInt());

	double fastForwardRatio = loadSetting("fastForwardRatio").toDouble();
	if (fastForwardRatio <= 0) {
		m_ui.fastForwardUnbounded->setChecked(true);
		m_ui.fastForwardRatio->setEnabled(false);
	} else {
		m_ui.fastForwardUnbounded->setChecked(false);
		m_ui.fastForwardRatio->setEnabled(true);
		m_ui.fastForwardRatio->setValue(fastForwardRatio);
	}

	QString idleOptimization = loadSetting("idleOptimization");
	if (idleOptimization == "ignore") {
		m_ui.idleOptimization->setCurrentIndex(0);
	} else if (idleOptimization == "remove") {
		m_ui.idleOptimization->setCurrentIndex(1);
	} else if (idleOptimization == "detect") {
		m_ui.idleOptimization->setCurrentIndex(2);
	}

	bool ok;
	int loadState = loadSetting("loadStateExtdata").toInt(&ok);
	if (!ok) {
		loadState = SAVESTATE_SCREENSHOT | SAVESTATE_RTC;
	}
	m_ui.loadStateScreenshot->setChecked(loadState & SAVESTATE_SCREENSHOT);
	m_ui.loadStateSave->setChecked(loadState & SAVESTATE_SAVEDATA);
	m_ui.loadStateCheats->setChecked(loadState & SAVESTATE_CHEATS);

	int saveState = loadSetting("saveStateExtdata").toInt(&ok);
	if (!ok) {
		saveState = SAVESTATE_SCREENSHOT | SAVESTATE_SAVEDATA | SAVESTATE_CHEATS | SAVESTATE_RTC | SAVESTATE_METADATA;
	}
	m_ui.saveStateScreenshot->setChecked(saveState & SAVESTATE_SCREENSHOT);
	m_ui.saveStateSave->setChecked(saveState & SAVESTATE_SAVEDATA);
	m_ui.saveStateCheats->setChecked(saveState & SAVESTATE_CHEATS);
}

void SettingsView::saveSetting(const char* key, const QAbstractButton* field) {
	m_controller->setOption(key, field->isChecked());
	m_controller->updateOption(key);
}

void SettingsView::saveSetting(const char* key, const QComboBox* field) {
	saveSetting(key, field->lineEdit());
}

void SettingsView::saveSetting(const char* key, const QDoubleSpinBox* field) {
	saveSetting(key, field->value());
}

void SettingsView::saveSetting(const char* key, const QLineEdit* field) {
	saveSetting(key, field->text());
}

void SettingsView::saveSetting(const char* key, const QSlider* field) {
	saveSetting(key, field->value());
}

void SettingsView::saveSetting(const char* key, const QSpinBox* field) {
	saveSetting(key, field->value());
}

void SettingsView::saveSetting(const char* key, const QVariant& field) {
	m_controller->setOption(key, field);
	m_controller->updateOption(key);
}

void SettingsView::loadSetting(const char* key, QAbstractButton* field) {
	QString option = loadSetting(key);
	field->setChecked(!option.isNull() && option != "0");
}

void SettingsView::loadSetting(const char* key, QComboBox* field) {
	loadSetting(key, field->lineEdit());
}

void SettingsView::loadSetting(const char* key, QDoubleSpinBox* field) {
	QString option = loadSetting(key);
	field->setValue(option.toDouble());
}

void SettingsView::loadSetting(const char* key, QLineEdit* field) {
	QString option = loadSetting(key);
	field->setText(option);
}

void SettingsView::loadSetting(const char* key, QSlider* field) {
	QString option = loadSetting(key);
	field->setValue(option.toInt());
}

void SettingsView::loadSetting(const char* key, QSpinBox* field) {
	QString option = loadSetting(key);
	field->setValue(option.toInt());
}

QString SettingsView::loadSetting(const char* key) {
	return m_controller->getOption(key);
}
