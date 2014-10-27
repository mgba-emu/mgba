#ifndef QGBA_VIDEO_VIEW
#define QGBA_VIDEO_VIEW

#ifdef USE_FFMPEG

#include <QWidget>

#include "ui_VideoView.h"

struct FFmpegEncoder;

namespace QGBA {

class VideoView : public QWidget {
Q_OBJECT

public:
	VideoView(QWidget* parent = nullptr);

private:
	Ui::VideoView m_ui;

	FFmpegEncoder* m_encoder;
};

}

#endif

#endif
