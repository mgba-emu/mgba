/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoView.h"

#ifdef USE_FFMPEG

#include "GBAApp.h"
#include "LogController.h"
#include "utils.h"

#include <mgba-util/math.h>

#include <QMap>

using namespace QGBA;

QMap<QString, QString> VideoView::s_acodecMap;
QMap<QString, QString> VideoView::s_vcodecMap;
QMap<QString, QString> VideoView::s_containerMap;
QMap<QString, QStringList> VideoView::s_extensionMap;

bool VideoView::Preset::compatible(const Preset& other) const {
	if (!other.container.isNull() && !container.isNull() && other.container != container) {
		return false;
	}
	if (!other.acodec.isNull() && !acodec.isNull() && other.acodec != acodec) {
		return false;
	}
	if (!other.vcodec.isNull() && !vcodec.isNull() && other.vcodec != vcodec) {
		return false;
	}
	if (other.abr && abr && other.abr != abr) {
		return false;
	}
	if (other.vbr && vbr && other.vbr != vbr) {
		return false;
	}
	if (other.dims.width() && dims.width() && other.dims.width() != dims.width()) {
		return false;
	}
	if (other.dims.height() && dims.height() && other.dims.height() != dims.height()) {
		return false;
	}
	return true;
}

VideoView::VideoView(QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);

	if (s_acodecMap.empty()) {
		s_acodecMap["mp3"] = "libmp3lame";
		s_acodecMap["opus"] = "libopus";
		s_acodecMap["vorbis"] = "libvorbis";
		s_acodecMap["uncompressed"] = "pcm_s16le";
	}
	if (s_vcodecMap.empty()) {
		s_vcodecMap["dirac"] = "libschroedinger";
		s_vcodecMap["h264"] = "libx264";
		s_vcodecMap["h264 nvenc"] = "h264_nvenc";
		s_vcodecMap["hevc"] = "libx265";
		s_vcodecMap["hevc nvenc"] = "hevc_nvenc";
		s_vcodecMap["theora"] = "libtheora";
		s_vcodecMap["vp8"] = "libvpx";
		s_vcodecMap["vp9"] = "libvpx-vp9";
		s_vcodecMap["xvid"] = "libxvid";
	}
	if (s_containerMap.empty()) {
		s_containerMap["mkv"] = "matroska";
	}
	if (s_extensionMap.empty()) {
		s_extensionMap["matroska"] += ".mkv";
		s_extensionMap["matroska"] += ".mka";
		s_extensionMap["webm"] += ".webm";
		s_extensionMap["avi"] += ".avi";
		s_extensionMap["mp4"] += ".mp4";
		s_extensionMap["mp4"] += ".m4v";
		s_extensionMap["mp4"] += ".m4a";

		s_extensionMap["flac"] += ".flac";
		s_extensionMap["mpeg"] += ".mpg";
		s_extensionMap["mpeg"] += ".mpeg";
		s_extensionMap["mpegts"] += ".ts";
		s_extensionMap["mp3"] += ".mp3";
		s_extensionMap["ogg"] += ".ogg";
		s_extensionMap["ogv"] += ".ogv";
	}

	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &VideoView::close);
	connect(m_ui.start, &QAbstractButton::clicked, this, &VideoView::startRecording);
	connect(m_ui.stop, &QAbstractButton::clicked, this, &VideoView::stopRecording);

	connect(m_ui.selectFile, &QAbstractButton::clicked, this, &VideoView::selectFile);
	connect(m_ui.filename, &QLineEdit::textChanged, this, &VideoView::setFilename);

	connect(m_ui.audio, SIGNAL(activated(const QString&)), this, SLOT(setAudioCodec(const QString&)));
	connect(m_ui.video, SIGNAL(activated(const QString&)), this, SLOT(setVideoCodec(const QString&)));
	connect(m_ui.container, SIGNAL(activated(const QString&)), this, SLOT(setContainer(const QString&)));
	connect(m_ui.audio, SIGNAL(editTextChanged(const QString&)), this, SLOT(setAudioCodec(const QString&)));
	connect(m_ui.video, SIGNAL(editTextChanged(const QString&)), this, SLOT(setVideoCodec(const QString&)));
	connect(m_ui.container, SIGNAL(editTextChanged(const QString&)), this, SLOT(setContainer(const QString&)));

	connect(m_ui.abr, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &VideoView::setAudioBitrate);
	connect(m_ui.crf, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &VideoView::setVideoRateFactor);
	connect(m_ui.vbr, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &VideoView::setVideoBitrate);
	connect(m_ui.doVbr, &QAbstractButton::toggled, this, [this](bool set) {
		if (set) {
			setVideoBitrate(m_ui.vbr->value());
		}
	});
	connect(m_ui.doCrf, &QAbstractButton::toggled, this, [this](bool set) {
		if (set) {
			setVideoRateFactor(m_ui.crf->value());
		}
	});

	connect(m_ui.width, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &VideoView::setWidth);
	connect(m_ui.height, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &VideoView::setHeight);

	connect(m_ui.wratio, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &VideoView::setAspectWidth);
	connect(m_ui.hratio, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &VideoView::setAspectHeight);

	connect(m_ui.showAdvanced, &QAbstractButton::clicked, this, &VideoView::showAdvanced);

	FFmpegEncoderInit(&m_encoder);

	updatePresets();

	m_ui.presetYoutube->setChecked(true); // Use the Youtube preset by default
	showAdvanced(false);
}

void VideoView::updatePresets() {
	m_presets.clear();

	addPreset(m_ui.preset4K, { maintainAspect(QSize(3840, 2160)) });
	addPreset(m_ui.preset1080, { maintainAspect(QSize(1920, 1080)) });
	addPreset(m_ui.preset720, { maintainAspect(QSize(1280, 720)) });
	addPreset(m_ui.preset480, { maintainAspect(QSize(720, 480)) });

	if (m_nativeWidth && m_nativeHeight) {
		addPreset(m_ui.presetNative, { QSize(m_nativeWidth, m_nativeHeight) });
		m_ui.presetNative->setEnabled(true);
	}

	addPreset(m_ui.presetHQ, {
		"MP4",
		"H.264",
		"AAC",
		-18,
		384,
		maintainAspect({ 1920, 1080 })
	});

	addPreset(m_ui.presetYoutube, {
		"MP4",
		"H.264",
		"AAC",
		-20,
		256,
		maintainAspect({ 1280, 720 })
	});

	addPreset(m_ui.presetWebM, {
		"WebM",
		"VP9",
		"Opus",
		800,
		128
	});

	addPreset(m_ui.presetMP4, {
		"MP4",
		"H.264",
		"AAC",
		-22,
		128
	});

	if (m_nativeWidth && m_nativeHeight) {
		addPreset(m_ui.presetLossless, {
			"MKV",
			"libx264rgb",
			"WavPack",
			-1,
			0,
			{ m_nativeWidth, m_nativeHeight }
		});
	}
}

VideoView::~VideoView() {
	stopRecording();
	free(m_audioCodecCstr);
	free(m_videoCodecCstr);
	free(m_containerCstr);
}

void VideoView::setController(std::shared_ptr<CoreController> controller) {
	CoreController* controllerPtr = controller.get();
	connect(controllerPtr, &CoreController::frameAvailable, this, [this, controllerPtr]() {
		setNativeResolution(controllerPtr->screenDimensions());
	});
	connect(controllerPtr, &CoreController::stopping, this, &VideoView::stopRecording);
	connect(this, &VideoView::recordingStarted, controllerPtr, &CoreController::setAVStream);
	connect(this, &VideoView::recordingStopped, controllerPtr, &CoreController::clearAVStream, Qt::DirectConnection);

	setNativeResolution(controllerPtr->screenDimensions());
}

void VideoView::startRecording() {
	if (QFileInfo(m_filename).suffix().isEmpty()) {
		changeExtension();
	}
	if (!validateSettings()) {
		return;
	}
	if (!FFmpegEncoderOpen(&m_encoder, m_filename.toUtf8().constData())) {
		LOG(QT, ERROR) << tr("Failed to open output video file: %1").arg(m_filename);
		return;
	}
	m_ui.start->setEnabled(false);
	m_ui.stop->setEnabled(true);
	emit recordingStarted(&m_encoder.d);
}

void VideoView::stopRecording() {
	emit recordingStopped();
	FFmpegEncoderClose(&m_encoder);
	m_ui.stop->setEnabled(false);
	validateSettings();
}

void VideoView::setNativeResolution(const QSize& dims) {
	if (dims.width() == m_nativeWidth && dims.height() == m_nativeHeight) {
		return;
	}
	m_nativeWidth = dims.width();
	m_nativeHeight = dims.height();
	m_ui.presetNative->setText(tr("Native (%0x%1)").arg(m_nativeWidth).arg(m_nativeHeight));
	QSize newSize = maintainAspect(QSize(m_width, m_height));
	m_width = newSize.width();
	m_height = newSize.height();
	updateAspectRatio(m_nativeWidth, m_nativeHeight, false);
	updatePresets();
	for (auto iterator = m_presets.constBegin(); iterator != m_presets.constEnd(); ++iterator) {
		if (iterator.key()->isChecked()) {
			setPreset(*iterator);
			break;
		}
	}
}

void VideoView::selectFile() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Select output file"));
	if (!filename.isEmpty()) {
		m_ui.filename->setText(filename);
		changeExtension();
	}
}

void VideoView::setFilename(const QString& fname) {
	m_filename = fname;
	validateSettings();
}

void VideoView::setAudioCodec(const QString& codec) {
	free(m_audioCodecCstr);
	m_audioCodec = sanitizeCodec(codec, s_acodecMap);
	if (m_audioCodec == "none") {
		m_audioCodecCstr = nullptr;
	} else {
		m_audioCodecCstr = strdup(m_audioCodec.toUtf8().constData());
	}
	if (!FFmpegEncoderSetAudio(&m_encoder, m_audioCodecCstr, 128 * 1024)) {
		free(m_audioCodecCstr);
		m_audioCodecCstr = nullptr;
		m_audioCodec = QString();
	}
	validateSettings();
	uncheckIncompatible();
}

void VideoView::setVideoCodec(const QString& codec) {
	free(m_videoCodecCstr);
	m_videoCodec = sanitizeCodec(codec, s_vcodecMap);
	if (m_videoCodec == "none") {
		m_videoCodecCstr = nullptr;
	} else {
		m_videoCodecCstr = strdup(m_videoCodec.toUtf8().constData());
	}
	if (!FFmpegEncoderSetVideo(&m_encoder, m_videoCodecCstr, 1024 * 1024, 0)) {
		free(m_videoCodecCstr);
		m_videoCodecCstr = nullptr;
		m_videoCodec = QString();
	}
	validateSettings();
	uncheckIncompatible();
}

void VideoView::setContainer(const QString& container) {
	free(m_containerCstr);
	m_container = sanitizeCodec(container, s_containerMap);
	m_containerCstr = strdup(m_container.toUtf8().constData());
	if (!FFmpegEncoderSetContainer(&m_encoder, m_containerCstr)) {
		free(m_containerCstr);
		m_containerCstr = nullptr;
		m_container = QString();
	}
	changeExtension();
	validateSettings();
	uncheckIncompatible();
}

void VideoView::setAudioBitrate(int br) {
	m_abr = br * 1000;
	FFmpegEncoderSetAudio(&m_encoder, m_audioCodecCstr, m_abr);
	validateSettings();
	uncheckIncompatible();
}

void VideoView::setVideoBitrate(int br) {
	m_vbr = br > 0 ? br * 1000 : br;
	FFmpegEncoderSetVideo(&m_encoder, m_videoCodecCstr, m_vbr, 0);
	validateSettings();
	uncheckIncompatible();
}

void VideoView::setVideoRateFactor(int rf) {
	setVideoBitrate(-rf);
}

void VideoView::setWidth(int width) {
	m_width = width;
	updateAspectRatio(width, 0, false);
	FFmpegEncoderSetDimensions(&m_encoder, m_width, m_height);
	uncheckIncompatible();
}

void VideoView::setHeight(int height) {
	m_height = height;
	updateAspectRatio(0, height, false);
	FFmpegEncoderSetDimensions(&m_encoder, m_width, m_height);
	uncheckIncompatible();
}

void VideoView::setAspectWidth(int) {
	updateAspectRatio(0, m_height, true);
	FFmpegEncoderSetDimensions(&m_encoder, m_width, m_height);
	uncheckIncompatible();
}

void VideoView::setAspectHeight(int) {
	updateAspectRatio(m_width, 0, true);
	FFmpegEncoderSetDimensions(&m_encoder, m_width, m_height);
	uncheckIncompatible();
}

void VideoView::showAdvanced(bool show) {
	m_ui.advancedBox->setVisible(show);
}

bool VideoView::validateSettings() {
	bool valid = !m_filename.isNull() && !FFmpegEncoderIsOpen(&m_encoder);
	if (m_audioCodec.isNull()) {
		valid = false;
		m_ui.audio->setStyleSheet("QComboBox { color: red; }");
	} else {
		m_ui.audio->setStyleSheet("");
		if (!FFmpegEncoderSetAudio(&m_encoder, m_audioCodecCstr, m_abr)) {
			m_ui.abr->setStyleSheet("QSpinBox { color: red; }");
		} else {
			m_ui.abr->setStyleSheet("");
		}
	}

	if (m_videoCodec.isNull()) {
		valid = false;
		m_ui.video->setStyleSheet("QComboBox { color: red; }");
	} else {
		m_ui.video->setStyleSheet("");
		if (!FFmpegEncoderSetVideo(&m_encoder, m_videoCodecCstr, m_vbr, 0)) {
			if (m_ui.doVbr->isChecked()) {
				m_ui.vbr->setStyleSheet("QSpinBox { color: red; }");
			} else {
				m_ui.vbr->setStyleSheet("");
			}
			if (m_ui.doCrf->isChecked()) {
				m_ui.crf->setStyleSheet("QSpinBox { color: red; }");
			} else {
				m_ui.crf->setStyleSheet("");
			}
		} else {
			m_ui.vbr->setStyleSheet("");
			m_ui.crf->setStyleSheet("");
		}
	}

	if (m_container.isNull()) {
		valid = false;
		m_ui.container->setStyleSheet("QComboBox { color: red; }");
	} else {
		m_ui.container->setStyleSheet("");
	}

	// This |valid| check is necessary as if one of the cstrs
	// is null, the encoder likely has a dangling pointer
	if (valid && !FFmpegEncoderVerifyContainer(&m_encoder)) {
		valid = false;
	}

	m_ui.start->setEnabled(valid);

	return valid;
}

void VideoView::updateAspectRatio(int width, int height, bool force) {
	if (m_ui.lockRatio->isChecked() || force) {
		if (width) {
			height = m_ui.hratio->value() * width / m_ui.wratio->value();
		} else if (height) {
			width = m_ui.wratio->value() * height / m_ui.hratio->value();
		}

		m_width = width;
		m_height = height;
		safelySet(m_ui.width, m_width);
		safelySet(m_ui.height, m_height);
	} else {
		int w = m_width;
		int h = m_height;
		reduceFraction(&h, &w);
		safelySet(m_ui.wratio, w);
		safelySet(m_ui.hratio, h);
	}
}

void VideoView::uncheckIncompatible() {
	if (m_updatesBlocked) {
		return;
	}

	Preset current = {
		m_container,
		m_videoCodec,
		m_audioCodec,
		m_vbr > 0 ? m_vbr / 1000 : m_vbr,
		m_abr / 1000,
		{ m_width, m_height }
	};

	m_ui.presets->setExclusive(false);
	m_ui.resolutions->setExclusive(false);
	for (auto iterator = m_presets.constBegin(); iterator != m_presets.constEnd(); ++iterator) {
		Preset next = *iterator;
		next.container = sanitizeCodec(next.container, s_containerMap);
		next.acodec = sanitizeCodec(next.acodec, s_acodecMap);
		next.vcodec = sanitizeCodec(next.vcodec, s_vcodecMap);
		if (!current.compatible(next)) {
			safelyCheck(iterator.key(), false);
		}
	}
	m_ui.presets->setExclusive(true);
	m_ui.resolutions->setExclusive(true);

	if (current.compatible(m_presets[m_ui.presetNative])) {
		safelyCheck(m_ui.presetNative);
	}
	if (current.compatible(m_presets[m_ui.preset480])) {
		safelyCheck(m_ui.preset480);
	}
	if (current.compatible(m_presets[m_ui.preset720])) {
		safelyCheck(m_ui.preset720);
	}
	if (current.compatible(m_presets[m_ui.preset1080])) {
		safelyCheck(m_ui.preset1080);
	}
}

void VideoView::changeExtension() {
	if (m_filename.isEmpty()) {
		return;
	}

	if (!s_extensionMap.contains(m_container)) {
		return;
	}

	QStringList extensions = s_extensionMap.value(m_container);
	QString filename = m_filename;
	int index = m_filename.lastIndexOf(".");
	if (index >= 0) {
		if (extensions.contains(filename.mid(index))) {
			// This extension is already valid
			return;
		}
		filename.truncate(index);
	}
	filename += extensions.front();

	m_ui.filename->setText(filename);
}

QString VideoView::sanitizeCodec(const QString& codec, const QMap<QString, QString>& mapping) {
	QString sanitized = codec.toLower();
	sanitized = sanitized.remove(QChar('.'));
	sanitized = sanitized.remove(QChar('('));
	sanitized = sanitized.remove(QChar(')'));
	if (mapping.contains(sanitized)) {
		sanitized = mapping[sanitized];
	}
	return sanitized;
}

void VideoView::safelyCheck(QAbstractButton* button, bool set) {
	QSignalBlocker blocker(button);
	bool autoExclusive = button->autoExclusive();
	button->setAutoExclusive(false);
	button->setChecked(set);
	button->setAutoExclusive(autoExclusive);
}

void VideoView::safelySet(QSpinBox* box, int value) {
	QSignalBlocker blocker(box);
	box->setValue(value);
}

void VideoView::safelySet(QComboBox* box, const QString& value) {
	QSignalBlocker blocker(box);
	box->lineEdit()->setText(value);
}

void VideoView::addPreset(QAbstractButton* button, const Preset& preset) {
	m_presets[button] = preset;
	button->disconnect();
	connect(button, &QAbstractButton::pressed, [this, preset]() {
		setPreset(preset);
	});
}

void VideoView::setPreset(const Preset& preset) {
	m_updatesBlocked = true;
	if (!preset.container.isNull()) {
		setContainer(preset.container);
		safelySet(m_ui.container, preset.container);
	}
	if (!preset.acodec.isNull()) {
		setAudioCodec(preset.acodec);
		safelySet(m_ui.audio, preset.acodec);
	}
	if (!preset.vcodec.isNull()) {
		setVideoCodec(preset.vcodec);
		safelySet(m_ui.video, preset.vcodec);
	}
	if (preset.abr) {
		setAudioBitrate(preset.abr);
		safelySet(m_ui.abr, preset.abr);
	}
	if (preset.vbr) {
		int vbr = preset.vbr;
		if (vbr == -1) {
			vbr = 0;
		}
		setVideoBitrate(vbr);
		if (vbr > 0) {
			safelySet(m_ui.vbr, vbr);
			m_ui.doVbr->setChecked(true);
		} else {
			safelySet(m_ui.crf, -vbr);
			m_ui.doCrf->setChecked(true);
		}
	}
	if (preset.dims.width() > 0) {
		setWidth(preset.dims.width());
		safelySet(m_ui.width, preset.dims.width());
	}
	if (preset.dims.height() > 0) {
		setHeight(preset.dims.height());
		safelySet(m_ui.height, preset.dims.height());
	}
	m_updatesBlocked = false;

	uncheckIncompatible();
	validateSettings();
}

QSize VideoView::maintainAspect(const QSize& size) {
	QSize ds = size;
	lockAspectRatio(QSize(m_nativeWidth, m_nativeHeight), ds);
	return ds;
}

#endif
