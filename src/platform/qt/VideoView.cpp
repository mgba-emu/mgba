/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoView.h"

#ifdef USE_FFMPEG

#include "GBAApp.h"
#include "LogController.h"

#include <QMap>

using namespace QGBA;

QMap<QString, QString> VideoView::s_acodecMap;
QMap<QString, QString> VideoView::s_vcodecMap;
QMap<QString, QString> VideoView::s_containerMap;

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

	connect(m_ui.abr, SIGNAL(valueChanged(int)), this, SLOT(setAudioBitrate(int)));
	connect(m_ui.vbr, SIGNAL(valueChanged(int)), this, SLOT(setVideoBitrate(int)));

	connect(m_ui.width, SIGNAL(valueChanged(int)), this, SLOT(setWidth(int)));
	connect(m_ui.height, SIGNAL(valueChanged(int)), this, SLOT(setHeight(int)));

	connect(m_ui.wratio, SIGNAL(valueChanged(int)), this, SLOT(setAspectWidth(int)));
	connect(m_ui.hratio, SIGNAL(valueChanged(int)), this, SLOT(setAspectHeight(int)));

	connect(m_ui.showAdvanced, &QAbstractButton::clicked, this, &VideoView::showAdvanced);

	FFmpegEncoderInit(&m_encoder);

	updatePresets();

	setPreset({
		.container = "MKV",
		.vcodec = "h.264",
		.acodec = "FLAC",
		.vbr = -1,
		.abr = 0,
		.dims = QSize(),
	});
	showAdvanced(false);
}

void VideoView::updatePresets() {
	m_presets.clear();

	addPreset(m_ui.preset1080, {
		.container = QString(),
		.vcodec = QString(),
		.acodec = QString(),
		.vbr = 0,
		.abr = 0,
		.dims = maintainAspect(QSize(1920, 1080))
	});

	addPreset(m_ui.preset720, {
		.container = QString(),
		.vcodec = QString(),
		.acodec = QString(),
		.vbr = 0,
		.abr = 0,
		.dims = maintainAspect(QSize(1280, 720))
	});

	addPreset(m_ui.preset480, {
		.container = QString(),
		.vcodec = QString(),
		.acodec = QString(),
		.vbr = 0,
		.abr = 0,
		.dims = maintainAspect(QSize(720, 480))
	});

	if (m_nativeWidth && m_nativeHeight) {
		addPreset(m_ui.presetNative, {
			.container = QString(),
			.vcodec = QString(),
			.acodec = QString(),
			.vbr = 0,
			.abr = 0,
			.dims = QSize(m_nativeWidth, m_nativeHeight)
		});
		m_ui.presetNative->setEnabled(true);
	}

	addPreset(m_ui.presetHQ, {
		.container = "MP4",
		.vcodec = "h.264",
		.acodec = "AAC",
		.vbr = 8000,
		.abr = 384,
		.dims = maintainAspect(QSize(1920, 1080))
	});

	addPreset(m_ui.presetYoutube, {
		.container = "MP4",
		.vcodec = "h.264",
		.acodec = "AAC",
		.vbr = 5000,
		.abr = 256,
		.dims = maintainAspect(QSize(1280, 720))
	});

	addPreset(m_ui.presetWebM, {
		.container = "WebM",
		.vcodec = "VP9",
		.acodec = "Opus",
		.vbr = 800,
		.abr = 128
	});

	if (m_nativeWidth && m_nativeHeight) {
		addPreset(m_ui.presetLossless, {
			.container = "MKV",
			.vcodec = "h.264",
			.acodec = "FLAC",
			.vbr = -1,
			.abr = 0,
			.dims = QSize(m_nativeWidth, m_nativeHeight)
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
	connect(controller.get(), &CoreController::stopping, this, &VideoView::stopRecording);
	connect(this, &VideoView::recordingStarted, controller.get(), &CoreController::setAVStream);
	connect(this, &VideoView::recordingStopped, controller.get(), &CoreController::clearAVStream, Qt::DirectConnection);

	setNativeResolution(controller->screenDimensions());
}

void VideoView::startRecording() {
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
	}
}

void VideoView::setFilename(const QString& fname) {
	m_filename = fname;
	validateSettings();
}

void VideoView::setAudioCodec(const QString& codec, bool manual) {
	free(m_audioCodecCstr);
	m_audioCodec = sanitizeCodec(codec, s_acodecMap);
	if (m_audioCodec == "none") {
		m_audioCodecCstr = nullptr;
	} else {
		m_audioCodecCstr = strdup(m_audioCodec.toUtf8().constData());
	}
	if (!FFmpegEncoderSetAudio(&m_encoder, m_audioCodecCstr, m_abr)) {
		free(m_audioCodecCstr);
		m_audioCodecCstr = nullptr;
		m_audioCodec = QString();
	}
	validateSettings();
	if (manual) {
		uncheckIncompatible();
	}
}

void VideoView::setVideoCodec(const QString& codec, bool manual) {
	free(m_videoCodecCstr);
	m_videoCodec = sanitizeCodec(codec, s_vcodecMap);
	if (m_videoCodec == "none") {
		m_videoCodecCstr = nullptr;
	} else {
		m_videoCodecCstr = strdup(m_videoCodec.toUtf8().constData());
	}
	if (!FFmpegEncoderSetVideo(&m_encoder, m_videoCodecCstr, m_vbr, 0)) {
		free(m_videoCodecCstr);
		m_videoCodecCstr = nullptr;
		m_videoCodec = QString();
	}
	validateSettings();
	if (manual) {
		uncheckIncompatible();
	}
}

void VideoView::setContainer(const QString& container, bool manual) {
	free(m_containerCstr);
	m_container = sanitizeCodec(container, s_containerMap);
	m_containerCstr = strdup(m_container.toUtf8().constData());
	if (!FFmpegEncoderSetContainer(&m_encoder, m_containerCstr)) {
		free(m_containerCstr);
		m_containerCstr = nullptr;
		m_container = QString();
	}
	validateSettings();
	if (manual) {
		uncheckIncompatible();
	}
}

void VideoView::setAudioBitrate(int br, bool manual) {
	m_abr = br * 1000;
	FFmpegEncoderSetAudio(&m_encoder, m_audioCodecCstr, m_abr);
	validateSettings();
	if (manual) {
		uncheckIncompatible();
	}
}

void VideoView::setVideoBitrate(int br, bool manual) {
	m_vbr = br >= 0 ? br * 1000 : 0;
	FFmpegEncoderSetVideo(&m_encoder, m_videoCodecCstr, m_vbr, 0);
	validateSettings();
	if (manual) {
		uncheckIncompatible();
	}
}

void VideoView::setWidth(int width, bool manual) {
	m_width = width;
	updateAspectRatio(width, 0, false);
	FFmpegEncoderSetDimensions(&m_encoder, m_width, m_height);
	if (manual) {
		uncheckIncompatible();
	}
}

void VideoView::setHeight(int height, bool manual) {
	m_height = height;
	updateAspectRatio(0, height, false);
	FFmpegEncoderSetDimensions(&m_encoder, m_width, m_height);
	if (manual) {
		uncheckIncompatible();
	}
}

void VideoView::setAspectWidth(int, bool manual) {
	updateAspectRatio(0, m_height, true);
	FFmpegEncoderSetDimensions(&m_encoder, m_width, m_height);
	if (manual) {
		uncheckIncompatible();
	}
}

void VideoView::setAspectHeight(int, bool manual) {
	updateAspectRatio(m_width, 0, true);
	FFmpegEncoderSetDimensions(&m_encoder, m_width, m_height);
	if (manual) {
		uncheckIncompatible();
	}
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
	}

	if (m_videoCodec.isNull()) {
		valid = false;
		m_ui.video->setStyleSheet("QComboBox { color: red; }");
	} else {
		m_ui.video->setStyleSheet("");
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
		// Get greatest common divisor
		while (w != 0) {
			int temp = h % w;
			h = w;
			w = temp;
		}
		int gcd = h;
		w = m_width / gcd;
		h = m_height / gcd;
		safelySet(m_ui.wratio, w);
		safelySet(m_ui.hratio, h);
	}
}

void VideoView::uncheckIncompatible() {
	Preset current = {
		.container = m_container,
		.vcodec = m_videoCodec,
		.acodec = m_audioCodec,
		.vbr = m_vbr / 1000,
		.abr = m_abr / 1000,
		.dims = QSize(m_width, m_height)
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
	bool signalsBlocked = button->blockSignals(true);
	bool autoExclusive = button->autoExclusive();
	button->setAutoExclusive(false);
	button->setChecked(set);
	button->setAutoExclusive(autoExclusive);
	button->blockSignals(signalsBlocked);
}

void VideoView::safelySet(QSpinBox* box, int value) {
	bool signalsBlocked = box->blockSignals(true);
	box->setValue(value);
	box->blockSignals(signalsBlocked);
}

void VideoView::safelySet(QComboBox* box, const QString& value) {
	bool signalsBlocked = box->blockSignals(true);
	box->lineEdit()->setText(value);
	box->blockSignals(signalsBlocked);
}

void VideoView::addPreset(QAbstractButton* button, const Preset& preset) {
	m_presets[button] = preset;
	button->disconnect();
	connect(button, &QAbstractButton::pressed, [this, preset]() {
		setPreset(preset);
	});
}

void VideoView::setPreset(const Preset& preset) {
	if (!preset.container.isNull()) {
		setContainer(preset.container, false);
		safelySet(m_ui.container, preset.container);
	}
	if (!preset.acodec.isNull()) {
		setAudioCodec(preset.acodec, false);
		safelySet(m_ui.audio, preset.acodec);
	}
	if (!preset.vcodec.isNull()) {
		setVideoCodec(preset.vcodec, false);
		safelySet(m_ui.video, preset.vcodec);
	}
	if (preset.abr) {
		setAudioBitrate(preset.abr, false);
		safelySet(m_ui.abr, preset.abr);
	}
	if (preset.vbr) {
		setVideoBitrate(preset.vbr, false);
		safelySet(m_ui.vbr, preset.vbr);
	}
	if (preset.dims.width() > 0) {
		setWidth(preset.dims.width(), false);
		safelySet(m_ui.width, preset.dims.width());
	}
	if (preset.dims.height() > 0) {
		setHeight(preset.dims.height(), false);
		safelySet(m_ui.height, preset.dims.height());
	}

	uncheckIncompatible();
	validateSettings();
}

QSize VideoView::maintainAspect(const QSize& size) {
	QSize ds = size;
	if (ds.width() * m_nativeHeight > ds.height() * m_nativeWidth) {
		ds.setWidth(ds.height() * m_nativeWidth / m_nativeHeight);
	} else if (ds.width() * m_nativeHeight < ds.height() * m_nativeWidth) {
		ds.setHeight(ds.width() * m_nativeHeight / m_nativeWidth);
	}
	return ds;
}

#endif
