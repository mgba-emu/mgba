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
#include "RotatedHeaderView.h"
#include "ShaderSelector.h"
#include "ShortcutView.h"

#include <mgba/core/serialize.h>
#include <mgba/core/version.h>
#include <mgba/internal/gba/gba.h>

using namespace QGBA;

#ifdef M_CORE_GB
QList<enum GBModel> SettingsView::s_gbModelList;
#endif

SettingsView::SettingsView(ConfigController* controller, InputController* inputController, ShortcutController* shortcutController, LogController* logController, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_controller(controller)
	, m_logModel(logController)
{
	m_ui.setupUi(this);

#ifdef M_CORE_GB
	if (s_gbModelList.isEmpty()) {
		// NB: Keep in sync with SettingsView.ui
		s_gbModelList.append(GB_MODEL_AUTODETECT);
		s_gbModelList.append(GB_MODEL_DMG);
		s_gbModelList.append(GB_MODEL_SGB);
		s_gbModelList.append(GB_MODEL_CGB);
		s_gbModelList.append(GB_MODEL_AGB);
	}
#endif

	reloadConfig();

	connect(m_ui.volume, static_cast<void (QSlider::*)(int)>(&QSlider::valueChanged), [this](int v) {
		if (v < m_ui.volumeFf->value()) {
			m_ui.volumeFf->setValue(v);
		}
	});

	connect(m_ui.mute, &QAbstractButton::toggled, [this](bool e) {
		if (e) {
			m_ui.muteFf->setChecked(e);
		}
	});

	connect(m_ui.nativeGB, &QAbstractButton::pressed, [this]() {
		m_ui.fpsTarget->setValue(double(GBA_ARM7TDMI_FREQUENCY) / double(VIDEO_TOTAL_LENGTH));
	});

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

	if (m_ui.cheatsPath->text().isEmpty()) {
		m_ui.cheatsSameDir->setChecked(true);
	}
	connect(m_ui.cheatsSameDir, &QAbstractButton::toggled, [this] (bool e) {
		if (e) {
			m_ui.cheatsPath->clear();
		}
	});
	connect(m_ui.cheatsBrowse, &QAbstractButton::pressed, [this] () {
		QString path = GBAApp::app()->getOpenDirectoryName(this, "Select directory");
		if (!path.isNull()) {
			m_ui.cheatsSameDir->setChecked(false);
			m_ui.cheatsPath->setText(path);
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

	// TODO: Move to reloadConfig()
	QVariant cameraDriver = m_controller->getQtOption("cameraDriver");
	m_ui.cameraDriver->addItem(tr("None (Still Image)"), static_cast<int>(InputController::CameraDriver::NONE));
	if (cameraDriver.isNull() || cameraDriver.toInt() == static_cast<int>(InputController::CameraDriver::NONE)) {
		m_ui.cameraDriver->setCurrentIndex(m_ui.cameraDriver->count() - 1);
		m_ui.camera->setEnabled(false);
	}

#ifdef BUILD_QT_MULTIMEDIA
	m_ui.cameraDriver->addItem(tr("Qt Multimedia"), static_cast<int>(InputController::CameraDriver::QT_MULTIMEDIA));
	if (!cameraDriver.isNull() && cameraDriver.toInt() == static_cast<int>(InputController::CameraDriver::QT_MULTIMEDIA)) {
		m_ui.cameraDriver->setCurrentIndex(m_ui.cameraDriver->count() - 1);
		m_ui.camera->setEnabled(true);
	}
	QList<QPair<QByteArray, QString>> cameras = inputController->listCameras();
	QByteArray currentCamera = m_controller->getQtOption("camera").toByteArray();
	for (const auto& camera : cameras) {
		m_ui.camera->addItem(camera.second, camera.first);
		if (camera.first == currentCamera) {
			m_ui.camera->setCurrentIndex(m_ui.camera->count() - 1);
		}
	}
#endif

#ifdef M_CORE_GBA
	connect(m_ui.gbaBiosBrowse, &QPushButton::clicked, [this]() {
		selectBios(m_ui.gbaBios);
	});
#else
	m_ui.gbaBiosBrowse->hide();
#endif

#ifdef M_CORE_GB
	connect(m_ui.gbBiosBrowse, &QPushButton::clicked, [this]() {
		selectBios(m_ui.gbBios);
	});
	connect(m_ui.gbcBiosBrowse, &QPushButton::clicked, [this]() {
		selectBios(m_ui.gbcBios);
	});
	connect(m_ui.sgbBiosBrowse, &QPushButton::clicked, [this]() {
		selectBios(m_ui.sgbBios);
	});

	QList<QColor> defaultColors;
	defaultColors.append(QColor(0xF8, 0xF8, 0xF8));
	defaultColors.append(QColor(0xA8, 0xA8, 0xA8));
	defaultColors.append(QColor(0x50, 0x50, 0x50));
	defaultColors.append(QColor(0x00, 0x00, 0x00));
	defaultColors.append(QColor(0xF8, 0xF8, 0xF8));
	defaultColors.append(QColor(0xA8, 0xA8, 0xA8));
	defaultColors.append(QColor(0x50, 0x50, 0x50));
	defaultColors.append(QColor(0x00, 0x00, 0x00));
	defaultColors.append(QColor(0xF8, 0xF8, 0xF8));
	defaultColors.append(QColor(0xA8, 0xA8, 0xA8));
	defaultColors.append(QColor(0x50, 0x50, 0x50));
	defaultColors.append(QColor(0x00, 0x00, 0x00));
	QList<QWidget*> colors{
		m_ui.color0,
		m_ui.color1,
		m_ui.color2,
		m_ui.color3,
		m_ui.color4,
		m_ui.color5,
		m_ui.color6,
		m_ui.color7,
		m_ui.color8,
		m_ui.color9,
		m_ui.color10,
		m_ui.color11
	};
	for (int colorId = 0; colorId < 12; ++colorId) {
		bool ok;
		uint color = m_controller->getOption(QString("gb.pal[%0]").arg(colorId)).toUInt(&ok);
		if (ok) {
			defaultColors[colorId] = QColor::fromRgb(color);
			m_gbColors[colorId] = color | 0xFF000000;
		} else {
			m_gbColors[colorId] = defaultColors[colorId].rgb() & ~0xFF000000;
		}
		m_colorPickers[colorId] = ColorPicker(colors[colorId], defaultColors[colorId]);
		connect(&m_colorPickers[colorId], &ColorPicker::colorChanged, this, [this, colorId](const QColor& color) {
			m_gbColors[colorId] = color.rgb();
		});
	}
#else
	m_ui.gbBiosBrowse->hide();
	m_ui.gbcBiosBrowse->hide();
	m_ui.sgbBiosBrowse->hide();
	m_ui.gb->hide();
#endif

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

	m_ui.loggingView->setModel(&m_logModel);
	m_ui.loggingView->setHorizontalHeader(new RotatedHeaderView(Qt::Horizontal));
	m_ui.loggingView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	m_ui.loggingView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	connect(m_ui.logFileBrowse, &QAbstractButton::pressed, [this] () {
		QString path = GBAApp::app()->getSaveFileName(this, "Select log file");
		if (!path.isNull()) {
			m_ui.logFile->setText(path);
		}
	});

	ShortcutView* shortcutView = new ShortcutView();
	shortcutView->setController(shortcutController);
	shortcutView->setInputController(inputController);
	m_ui.stackedWidget->addWidget(shortcutView);
	m_ui.tabs->addItem(tr("Shortcuts"));
}

SettingsView::~SettingsView() {
#if defined(BUILD_GL) || defined(BUILD_GLES2)
	setShaderSelector(nullptr);
#endif
}

void SettingsView::setShaderSelector(ShaderSelector* shaderSelector) {
#if defined(BUILD_GL) || defined(BUILD_GLES2)
	if (m_shader) {
		auto items = m_ui.tabs->findItems(tr("Shaders"), Qt::MatchFixedString);
		for (const auto& item : items) {
			m_ui.tabs->removeItemWidget(item);
		}
		m_ui.stackedWidget->removeWidget(m_shader);
		m_shader->setParent(nullptr);
	}
	m_shader = shaderSelector;
	if (shaderSelector) {
		m_ui.stackedWidget->addWidget(m_shader);
		m_ui.tabs->addItem(tr("Shaders"));
		connect(m_ui.buttonBox, &QDialogButtonBox::accepted, m_shader, &ShaderSelector::saved);
	}
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
	saveSetting("sgb.bios", m_ui.sgbBios);
	saveSetting("sgb.borders", m_ui.sgbBorders);
	saveSetting("useCgbColors", m_ui.useCgbColors);
	saveSetting("useBios", m_ui.useBios);
	saveSetting("skipBios", m_ui.skipBios);
	saveSetting("sampleRate", m_ui.sampleRate);
	saveSetting("videoSync", m_ui.videoSync);
	saveSetting("audioSync", m_ui.audioSync);
	saveSetting("frameskip", m_ui.frameskip);
	saveSetting("autofireThreshold", m_ui.autofireThreshold);
	saveSetting("lockAspectRatio", m_ui.lockAspectRatio);
	saveSetting("lockIntegerScaling", m_ui.lockIntegerScaling);
	saveSetting("interframeBlending", m_ui.interframeBlending);
	saveSetting("showOSD", m_ui.showOSD);
	saveSetting("volume", m_ui.volume);
	saveSetting("mute", m_ui.mute);
	saveSetting("fastForwardVolume", m_ui.volumeFf);
	saveSetting("fastForwardMute", m_ui.muteFf);
	saveSetting("rewindEnable", m_ui.rewind);
	saveSetting("rewindBufferCapacity", m_ui.rewindCapacity);
	saveSetting("resampleVideo", m_ui.resampleVideo);
	saveSetting("allowOpposingDirections", m_ui.allowOpposingDirections);
	saveSetting("suspendScreensaver", m_ui.suspendScreensaver);
	saveSetting("pauseOnFocusLost", m_ui.pauseOnFocusLost);
	saveSetting("pauseOnMinimize", m_ui.pauseOnMinimize);
	saveSetting("savegamePath", m_ui.savegamePath);
	saveSetting("savestatePath", m_ui.savestatePath);
	saveSetting("screenshotPath", m_ui.screenshotPath);
	saveSetting("patchPath", m_ui.patchPath);
	saveSetting("cheatsPath", m_ui.cheatsPath);
	saveSetting("libraryStyle", m_ui.libraryStyle->currentIndex());
	saveSetting("showLibrary", m_ui.showLibrary);
	saveSetting("preload", m_ui.preload);
	saveSetting("showFps", m_ui.showFps);
	saveSetting("cheatAutoload", m_ui.cheatAutoload);
	saveSetting("cheatAutosave", m_ui.cheatAutosave);
	saveSetting("showFilename", m_ui.showFilename);
	saveSetting("autoload", m_ui.autoload);
	saveSetting("autosave", m_ui.autosave);
	saveSetting("logToFile", m_ui.logToFile);
	saveSetting("logToStdout", m_ui.logToStdout);
	saveSetting("logFile", m_ui.logFile);
	saveSetting("useDiscordPresence", m_ui.useDiscordPresence);
	saveSetting("gba.audioHle", m_ui.audioHle);

	if (m_ui.audioBufferSize->currentText().toInt() > 8192) {
		m_ui.audioBufferSize->setCurrentText("8192");
	}
	saveSetting("audioBuffers", m_ui.audioBufferSize);

	if (m_ui.fastForwardUnbounded->isChecked()) {
		saveSetting("fastForwardRatio", "-1");
	} else {
		saveSetting("fastForwardRatio", m_ui.fastForwardRatio);
	}

	double nativeFps = double(GBA_ARM7TDMI_FREQUENCY) / double(VIDEO_TOTAL_LENGTH);
	if (fabs(nativeFps - m_ui.fpsTarget->value()) < 0.0001) {
		m_controller->setOption("fpsTarget", QVariant(nativeFps));
	} else {
		saveSetting("fpsTarget", m_ui.fpsTarget);
	}

	if (m_ui.fastForwardHeldUnbounded->isChecked()) {
		saveSetting("fastForwardHeldRatio", "-1");
	} else {
		saveSetting("fastForwardHeldRatio", m_ui.fastForwardHeldRatio);
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
		setShaderSelector(nullptr);
		emit displayDriverChanged();
	}

	QVariant cameraDriver = m_ui.cameraDriver->itemData(m_ui.cameraDriver->currentIndex());
	QVariant oldCameraDriver = m_controller->getQtOption("cameraDriver");
	if (cameraDriver != oldCameraDriver) {
		m_controller->setQtOption("cameraDriver", cameraDriver);
		if (cameraDriver.toInt() != static_cast<int>(InputController::CameraDriver::NONE) || !oldCameraDriver.isNull()) {
			emit cameraDriverChanged();
		}
	}

	QVariant camera = m_ui.camera->itemData(m_ui.camera->currentIndex());
	if (camera != m_controller->getQtOption("camera")) {
		m_controller->setQtOption("camera", camera);
		emit cameraChanged(camera.toByteArray());
	}

	QLocale language = m_ui.languages->itemData(m_ui.languages->currentIndex()).toLocale();
	if (language != m_controller->getQtOption("language").toLocale() && !(language.bcp47Name() == QLocale::system().bcp47Name() && m_controller->getQtOption("language").isNull())) {
		m_controller->setQtOption("language", language.bcp47Name());
		emit languageChanged();
	}

	int videoScale = m_controller->getOption("videoScale", 1).toInt();
	int hwaccelVideo = m_controller->getOption("hwaccelVideo").toInt();
	if (hwaccelVideo != m_ui.hwaccelVideo->currentIndex()) {
		emit videoRendererChanged();
	}
	saveSetting("videoScale", m_ui.videoScale);
	saveSetting("hwaccelVideo", m_ui.hwaccelVideo->currentIndex());

	m_logModel.save(m_controller);
	m_logModel.logger()->setLogFile(m_ui.logFile->text());
	m_logModel.logger()->logToFile(m_ui.logToFile->isChecked());
	m_logModel.logger()->logToStdout(m_ui.logToStdout->isChecked());

#ifdef M_CORE_GB
	GBModel modelGB = s_gbModelList[m_ui.gbModel->currentIndex()];
	m_controller->setOption("gb.model", GBModelToName(modelGB));

	GBModel modelSGB = s_gbModelList[m_ui.sgbModel->currentIndex()];
	m_controller->setOption("sgb.model", GBModelToName(modelSGB));

	GBModel modelCGB = s_gbModelList[m_ui.cgbModel->currentIndex()];
	m_controller->setOption("cgb.model", GBModelToName(modelCGB));

	for (int colorId = 0; colorId < 12; ++colorId) {
		if (!(m_gbColors[colorId] & 0xFF000000)) {
			continue;
		}
		QString color = QString("gb.pal[%0]").arg(colorId);
		m_controller->setOption(color.toUtf8().constData(), m_gbColors[colorId] & ~0xFF000000);

	}
#endif

	m_controller->write();

	emit pathsChanged();
	emit biosLoaded(PLATFORM_GBA, m_ui.gbaBios->text());
}

void SettingsView::reloadConfig() {	
	loadSetting("bios", m_ui.gbaBios);
	loadSetting("gba.bios", m_ui.gbaBios);
	loadSetting("gb.bios", m_ui.gbBios);
	loadSetting("gbc.bios", m_ui.gbcBios);
	loadSetting("sgb.bios", m_ui.sgbBios);
	loadSetting("sgb.borders", m_ui.sgbBorders, true);
	loadSetting("useCgbColors", m_ui.useCgbColors, true);
	loadSetting("useBios", m_ui.useBios);
	loadSetting("skipBios", m_ui.skipBios);
	loadSetting("audioBuffers", m_ui.audioBufferSize);
	loadSetting("sampleRate", m_ui.sampleRate);
	loadSetting("videoSync", m_ui.videoSync);
	loadSetting("audioSync", m_ui.audioSync);
	loadSetting("frameskip", m_ui.frameskip);
	loadSetting("fpsTarget", m_ui.fpsTarget);
	loadSetting("autofireThreshold", m_ui.autofireThreshold);
	loadSetting("lockAspectRatio", m_ui.lockAspectRatio);
	loadSetting("lockIntegerScaling", m_ui.lockIntegerScaling);
	loadSetting("interframeBlending", m_ui.interframeBlending);
	loadSetting("showOSD", m_ui.showOSD, true);
	loadSetting("volume", m_ui.volume, 0x100);
	loadSetting("mute", m_ui.mute, false);
	loadSetting("fastForwardVolume", m_ui.volumeFf, m_ui.volume->value());
	loadSetting("fastForwardMute", m_ui.muteFf, m_ui.mute->isChecked());
	loadSetting("rewindEnable", m_ui.rewind);
	loadSetting("rewindBufferCapacity", m_ui.rewindCapacity);
	loadSetting("resampleVideo", m_ui.resampleVideo);
	loadSetting("allowOpposingDirections", m_ui.allowOpposingDirections);
	loadSetting("suspendScreensaver", m_ui.suspendScreensaver);
	loadSetting("pauseOnFocusLost", m_ui.pauseOnFocusLost);
	loadSetting("pauseOnMinimize", m_ui.pauseOnMinimize);
	loadSetting("savegamePath", m_ui.savegamePath);
	loadSetting("savestatePath", m_ui.savestatePath);
	loadSetting("screenshotPath", m_ui.screenshotPath);
	loadSetting("patchPath", m_ui.patchPath);
	loadSetting("cheatsPath", m_ui.cheatsPath);
	loadSetting("showLibrary", m_ui.showLibrary);
	loadSetting("preload", m_ui.preload);
	loadSetting("showFps", m_ui.showFps, true);
	loadSetting("cheatAutoload", m_ui.cheatAutoload, true);
	loadSetting("cheatAutosave", m_ui.cheatAutosave, true);
	loadSetting("showFilename", m_ui.showFilename, false);
	loadSetting("autoload", m_ui.autoload, true);
	loadSetting("autosave", m_ui.autosave, false);
	loadSetting("logToFile", m_ui.logToFile);
	loadSetting("logToStdout", m_ui.logToStdout);
	loadSetting("logFile", m_ui.logFile);
	loadSetting("useDiscordPresence", m_ui.useDiscordPresence);
	loadSetting("gba.audioHle", m_ui.audioHle);

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

	double fastForwardHeldRatio = loadSetting("fastForwardHeldRatio").toDouble();
	if (fastForwardHeldRatio <= 0) {
		m_ui.fastForwardHeldUnbounded->setChecked(true);
		m_ui.fastForwardHeldRatio->setEnabled(false);
	} else {
		m_ui.fastForwardHeldUnbounded->setChecked(false);
		m_ui.fastForwardHeldRatio->setEnabled(true);
		m_ui.fastForwardHeldRatio->setValue(fastForwardHeldRatio);
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

	m_logModel.reset();

#ifdef M_CORE_GB
	QString modelGB = m_controller->getOption("gb.model");
	if (!modelGB.isNull()) {
		GBModel model = GBNameToModel(modelGB.toUtf8().constData());
		int index = s_gbModelList.indexOf(model);
		m_ui.gbModel->setCurrentIndex(index >= 0 ? index : 0);
	}

	QString modelSGB = m_controller->getOption("sgb.model");
	if (!modelSGB.isNull()) {
		GBModel model = GBNameToModel(modelSGB.toUtf8().constData());
		int index = s_gbModelList.indexOf(model);
		m_ui.sgbModel->setCurrentIndex(index >= 0 ? index : 0);
	}

	QString modelCGB = m_controller->getOption("cgb.model");
	if (!modelCGB.isNull()) {
		GBModel model = GBNameToModel(modelCGB.toUtf8().constData());
		int index = s_gbModelList.indexOf(model);
		m_ui.cgbModel->setCurrentIndex(index >= 0 ? index : 0);
	}
#endif

	int hwaccelVideo = m_controller->getOption("hwaccelVideo", 0).toInt();
	m_ui.hwaccelVideo->setCurrentIndex(hwaccelVideo);

	connect(m_ui.videoScale, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int value) {
		m_ui.videoScaleSize->setText(tr("(%1×%2)").arg(GBA_VIDEO_HORIZONTAL_PIXELS * value).arg(GBA_VIDEO_VERTICAL_PIXELS * value));
	});
	loadSetting("videoScale", m_ui.videoScale, 1);
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

void SettingsView::loadSetting(const char* key, QAbstractButton* field, bool defaultVal) {
	QString option = loadSetting(key);
	field->setChecked(option.isNull() ? defaultVal : option != "0");
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

void SettingsView::loadSetting(const char* key, QSlider* field, int defaultVal) {
	QString option = loadSetting(key);
	field->setValue(option.isNull() ? defaultVal : option.toInt());
}

void SettingsView::loadSetting(const char* key, QSpinBox* field, int defaultVal) {
	QString option = loadSetting(key);
	field->setValue(option.isNull() ? defaultVal : option.toInt());
}

QString SettingsView::loadSetting(const char* key) {
	return m_controller->getOption(key);
}
