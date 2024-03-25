/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QObject>
#include <QVideoFrame>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QAbstractVideoSurface>
#include <QVideoSurfaceFormat>
#else
#include <QVideoFrameFormat>
#endif

#ifdef USE_FFMPEG
extern "C" {
#include <libswscale/swscale.h>
}
#endif

namespace QGBA {

class VideoDumper : public
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
QAbstractVideoSurface
#else
QObject
#endif
{
Q_OBJECT

public:
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	using PixelFormat = QVideoFrame::PixelFormat;
	using Direction = QVideoSurfaceFormat::Direction;
#else
	using PixelFormat = QVideoFrameFormat::PixelFormat;
	using Direction = QVideoFrameFormat::Direction;
#endif

	VideoDumper(QObject* parent = nullptr);

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType type) const override;
#endif
	static QList<PixelFormat> supportedPixelFormats();
	static QImage::Format imageFormatFromPixelFormat(PixelFormat);

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	bool present(const QVideoFrame& frame) override;
#else
public slots:
	bool present(const QVideoFrame& frame);
#endif

signals:
	void imageAvailable(const QImage& image);

private:
#ifdef USE_FFMPEG
	AVPixelFormat m_pixfmt = AV_PIX_FMT_NONE;
	SwsContext* m_scaler = nullptr;
	QSize m_scalerSize;
#endif
};

}
