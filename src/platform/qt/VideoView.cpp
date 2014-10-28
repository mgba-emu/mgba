#include "VideoView.h"

#ifdef USE_FFMPEG

#include <QFileDialog>
#include <QMap>

using namespace QGBA;

QMap<QString, QString> VideoView::s_acodecMap;
QMap<QString, QString> VideoView::s_vcodecMap;
QMap<QString, QString> VideoView::s_containerMap;

VideoView::VideoView(QWidget* parent)
	: QWidget(parent)
	, m_audioCodecCstr(nullptr)
	, m_videoCodecCstr(nullptr)
	, m_containerCstr(nullptr)
{
	m_ui.setupUi(this);

	if (s_acodecMap.empty()) {
		s_acodecMap["aac"] = "libfaac";
		s_acodecMap["mp3"] = "libmp3lame";
		s_acodecMap["uncompressed"] = "pcm_s16le";
	}
	if (s_vcodecMap.empty()) {
		s_vcodecMap["h264"] = "libx264rgb";
	}
	if (s_containerMap.empty()) {
		s_containerMap["mkv"] = "matroska";
	}

	connect(m_ui.buttonBox, SIGNAL(rejected()), this, SLOT(close()));
	connect(m_ui.start, SIGNAL(clicked()), this, SLOT(startRecording()));
	connect(m_ui.stop, SIGNAL(clicked()), this, SLOT(stopRecording()));

	connect(m_ui.selectFile, SIGNAL(clicked()), this, SLOT(selectFile()));
	connect(m_ui.filename, SIGNAL(textChanged(const QString&)), this, SLOT(setFilename(const QString&)));

	connect(m_ui.audio, SIGNAL(activated(const QString&)), this, SLOT(setAudioCodec(const QString&)));
	connect(m_ui.video, SIGNAL(activated(const QString&)), this, SLOT(setVideoCodec(const QString&)));
	connect(m_ui.container, SIGNAL(activated(const QString&)), this, SLOT(setContainer(const QString&)));

	connect(m_ui.abr, SIGNAL(valueChanged(int)), this, SLOT(setAudioBitrate(int)));
	connect(m_ui.vbr, SIGNAL(valueChanged(int)), this, SLOT(setVideoBitrate(int)));

	FFmpegEncoderInit(&m_encoder);

	setAudioCodec(m_ui.audio->currentText());
	setVideoCodec(m_ui.video->currentText());
	setAudioBitrate(m_ui.abr->value());
	setVideoBitrate(m_ui.vbr->value());
	setContainer(m_ui.container->currentText());
}

VideoView::~VideoView() {
	stopRecording();
	free(m_audioCodecCstr);
	free(m_videoCodecCstr);
	free(m_containerCstr);
}

void VideoView::startRecording() {
	if (!validateSettings()) {
		return;
	}
	if (!FFmpegEncoderOpen(&m_encoder, m_filename.toLocal8Bit().constData())) {
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

void VideoView::selectFile() {
	QString filename = QFileDialog::getSaveFileName(this, tr("Select output file"));
	if (!filename.isEmpty()) {
		m_ui.filename->setText(filename);
	}
}

void VideoView::setFilename(const QString& fname) {
	m_filename = fname;
	validateSettings();
}

void VideoView::setAudioCodec(const QString& codec) {
	free(m_audioCodecCstr);
	m_audioCodec = sanitizeCodec(codec);
	if (s_acodecMap.contains(m_audioCodec)) {
		m_audioCodec = s_acodecMap[m_audioCodec];
	}
	m_audioCodecCstr = strdup(m_audioCodec.toLocal8Bit().constData());
	if (!FFmpegEncoderSetAudio(&m_encoder, m_audioCodecCstr, m_abr)) {
		free(m_audioCodecCstr);
		m_audioCodecCstr = nullptr;
	}
	validateSettings();
}

void VideoView::setVideoCodec(const QString& codec) {
	free(m_videoCodecCstr);
	m_videoCodec = sanitizeCodec(codec);
	if (s_vcodecMap.contains(m_videoCodec)) {
		m_videoCodec = s_vcodecMap[m_videoCodec];
	}
	m_videoCodecCstr = strdup(m_videoCodec.toLocal8Bit().constData());
	if (!FFmpegEncoderSetVideo(&m_encoder, m_videoCodecCstr, m_vbr)) {
		free(m_videoCodecCstr);
		m_videoCodecCstr = nullptr;
	}
	validateSettings();
}

void VideoView::setContainer(const QString& container) {
	free(m_containerCstr);
	m_container = sanitizeCodec(container);
	if (s_containerMap.contains(m_container)) {
		m_container = s_containerMap[m_container];
	}
	m_containerCstr = strdup(m_container.toLocal8Bit().constData());
	if (!FFmpegEncoderSetContainer(&m_encoder, m_containerCstr)) {
		free(m_containerCstr);
		m_containerCstr = nullptr;
	}
	validateSettings();
}

void VideoView::setAudioBitrate(int br) {
	m_abr = br * 1000;
	FFmpegEncoderSetAudio(&m_encoder, m_audioCodecCstr, m_abr);
	validateSettings();
}

void VideoView::setVideoBitrate(int br) {
	m_vbr = br * 1000;
	FFmpegEncoderSetVideo(&m_encoder, m_videoCodecCstr, m_vbr);
	validateSettings();
}

bool VideoView::validateSettings() {
	bool valid = true;
	if (!m_audioCodecCstr) {
		valid = false;
		m_ui.audio->setStyleSheet("QComboBox { color: red; }");
	} else {
		m_ui.audio->setStyleSheet("");
	}

	if (!m_videoCodecCstr) {
		valid = false;
		m_ui.video->setStyleSheet("QComboBox { color: red; }");
	} else {
		m_ui.video->setStyleSheet("");
	}

	if (!m_containerCstr) {
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

	m_ui.start->setEnabled(valid && !m_filename.isNull());
	return valid;
}

QString VideoView::sanitizeCodec(const QString& codec) {
	QString sanitized = codec.toLower();
	return sanitized.remove(QChar('.'));
}

#endif
