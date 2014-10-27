#include "VideoView.h"

#ifdef USE_FFMPEG

using namespace QGBA;

VideoView::VideoView(QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);

	connect(m_ui.buttonBox, SIGNAL(rejected()), this, SLOT(close()));
}

#endif
