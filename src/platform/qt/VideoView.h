#ifndef QGBA_VIDEO_VIEW
#define QGBA_VIDEO_VIEW

#ifdef USE_FFMPEG

#include <QWidget>

#include "ui_VideoView.h"

extern "C" {
#include "platform/ffmpeg/ffmpeg-encoder.h"
}

namespace QGBA {

class VideoView : public QWidget {
Q_OBJECT

public:
	VideoView(QWidget* parent = nullptr);
	virtual ~VideoView();

	GBAAVStream* getStream() { return &m_encoder.d; }

public slots:
	void startRecording();
	void stopRecording();

signals:
	void recordingStarted(GBAAVStream*);
	void recordingStopped();

private slots:
	void selectFile();
	void setFilename(const QString&);
	void setAudioCodec(const QString&);
	void setVideoCodec(const QString&);
	void setContainer(const QString&);

	void setAudioBitrate(int);
	void setVideoBitrate(int);

private:
	bool validateSettings();
	static QString sanitizeCodec(const QString&);

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

	static QMap<QString, QString> s_acodecMap;
	static QMap<QString, QString> s_vcodecMap;
	static QMap<QString, QString> s_containerMap;
};

}

#endif

#endif
