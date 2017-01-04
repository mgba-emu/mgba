/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_VIDEO_VIEW
#define QGBA_VIDEO_VIEW

#ifdef USE_FFMPEG

#include <QWidget>

#include "ui_VideoView.h"

#include "feature/ffmpeg/ffmpeg-encoder.h"

namespace QGBA {

class VideoView : public QWidget {
Q_OBJECT

public:
	VideoView(QWidget* parent = nullptr);
	virtual ~VideoView();

	mAVStream* getStream() { return &m_encoder.d; }

public slots:
	void startRecording();
	void stopRecording();
	void setNativeResolution(const QSize&);

signals:
	void recordingStarted(mAVStream*);
	void recordingStopped();

private slots:
	void selectFile();
	void setFilename(const QString&);
	void setAudioCodec(const QString&, bool manual = true);
	void setVideoCodec(const QString&, bool manual = true);
	void setContainer(const QString&, bool manual = true);

	void setAudioBitrate(int, bool manual = true);
	void setVideoBitrate(int, bool manual = true);

	void setWidth(int, bool manual = true);
	void setHeight(int, bool manual = true);
	void setAspectWidth(int, bool manual = true);
	void setAspectHeight(int, bool manual = true);

	void showAdvanced(bool);

	void uncheckIncompatible();
	void updatePresets();

private:
	struct Preset {
		QString container;
		QString vcodec;
		QString acodec;
		int vbr;
		int abr;
		QSize dims;

		bool compatible(const Preset&) const;
	};

	bool validateSettings();
	void updateAspectRatio(int width, int height, bool force = false);
	static QString sanitizeCodec(const QString&, const QMap<QString, QString>& mapping);
	static void safelyCheck(QAbstractButton*, bool set = true);
	static void safelySet(QSpinBox*, int value);
	static void safelySet(QComboBox*, const QString& value);

	void addPreset(QAbstractButton*, const Preset&);
	void setPreset(const Preset&);

	QSize maintainAspect(const QSize&);

	Ui::VideoView m_ui;

	FFmpegEncoder m_encoder;

	QString m_filename;
	QString m_audioCodec;
	QString m_videoCodec;
	QString m_container;
	char* m_audioCodecCstr;
	char* m_videoCodecCstr;
	char* m_containerCstr;

	int m_abr;
	int m_vbr;

	int m_width;
	int m_height;

	int m_nativeWidth;
	int m_nativeHeight;

	QMap<QAbstractButton*, Preset> m_presets;

	static QMap<QString, QString> s_acodecMap;
	static QMap<QString, QString> s_vcodecMap;
	static QMap<QString, QString> s_containerMap;
};

}

#endif

#endif
