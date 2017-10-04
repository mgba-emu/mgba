/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoDumper.h"

#include <QImage>

using namespace QGBA;

VideoDumper::VideoDumper(QObject* parent)
	: QAbstractVideoSurface(parent)
{
}

bool VideoDumper::present(const QVideoFrame& frame) {
	QVideoFrame mappedFrame(frame);
	if (!mappedFrame.map(QAbstractVideoBuffer::ReadOnly)) {
		return false;
	}
	QVideoFrame::PixelFormat vFormat = mappedFrame.pixelFormat();
	QImage::Format format = QVideoFrame::imageFormatFromPixelFormat(vFormat);
	bool swap = false;
	if (format == QImage::Format_Invalid) {
		vFormat = static_cast<QVideoFrame::PixelFormat>(vFormat - QVideoFrame::Format_BGRA32 + QVideoFrame::Format_ARGB32);
		format = QVideoFrame::imageFormatFromPixelFormat(vFormat);
		if (format == QImage::Format_ARGB32) {
			format = QImage::Format_RGBA8888;
		} else if (format == QImage::Format_ARGB32_Premultiplied) {
			format = QImage::Format_RGBA8888_Premultiplied;
		}
		swap = true;
	}
	uchar* bits = mappedFrame.bits();
	QImage image(bits, mappedFrame.width(), mappedFrame.height(), mappedFrame.bytesPerLine(), format);
	if (swap) {
		image = image.rgbSwapped();
	} else {
#ifdef Q_OS_WIN
		// Qt's DirectShow plug-in is pretty dang buggy
		image = image.mirrored();
#else
		image = image.copy(); // Create a deep copy of the bits
#endif
	}
	mappedFrame.unmap();
	emit imageAvailable(image);
	return true;
}

QList<QVideoFrame::PixelFormat> VideoDumper::supportedPixelFormats(QAbstractVideoBuffer::HandleType) const {
	QList<QVideoFrame::PixelFormat> list;
	list.append(QVideoFrame::Format_RGB32);
	list.append(QVideoFrame::Format_ARGB32);
	list.append(QVideoFrame::Format_RGB24);
	list.append(QVideoFrame::Format_ARGB32_Premultiplied);
	list.append(QVideoFrame::Format_RGB565);
	list.append(QVideoFrame::Format_RGB555);
	list.append(QVideoFrame::Format_BGR32);
	list.append(QVideoFrame::Format_BGRA32);
	list.append(QVideoFrame::Format_BGR24);
	list.append(QVideoFrame::Format_BGRA32_Premultiplied);
	list.append(QVideoFrame::Format_BGR565);
	list.append(QVideoFrame::Format_BGR555);
	return list;
}
