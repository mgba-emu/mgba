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
	QImage::Format format = QVideoFrame::imageFormatFromPixelFormat(mappedFrame.pixelFormat());
	uchar* bits = mappedFrame.bits();
	QImage image(bits, mappedFrame.width(), mappedFrame.height(), mappedFrame.bytesPerLine(), format);
	image = image.copy(); // Create a deep copy of the bits
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
	return list;
}
