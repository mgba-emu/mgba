#ifndef QGBA_GIF_VIEW
#define QGBA_GIF_VIEW

#ifdef USE_MAGICK

#include <QWidget>

#include "ui_GIFView.h"

extern "C" {
#include "platform/imagemagick/imagemagick-gif-encoder.h"
}

namespace QGBA {

class GIFView : public QWidget {
Q_OBJECT

public:
	GIFView(QWidget* parent = nullptr);
	virtual ~GIFView();

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

private:
	Ui::GIFView m_ui;

	ImageMagickGIFEncoder m_encoder;

	QString m_filename;
};

}

#endif

#endif
