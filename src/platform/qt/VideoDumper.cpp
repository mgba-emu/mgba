/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoDumper.h"

#include <QImage>
#include <QVideoSurfaceFormat>

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
#ifdef USE_FFMPEG
	bool useScaler = false;
#endif
	if (format == QImage::Format_Invalid) {
		if (vFormat < QVideoFrame::Format_BGRA5658_Premultiplied) {
			vFormat = static_cast<QVideoFrame::PixelFormat>(vFormat - QVideoFrame::Format_BGRA32 + QVideoFrame::Format_ARGB32);
			format = QVideoFrame::imageFormatFromPixelFormat(vFormat);
			if (format == QImage::Format_ARGB32) {
				format = QImage::Format_RGBA8888;
			} else if (format == QImage::Format_ARGB32_Premultiplied) {
				format = QImage::Format_RGBA8888_Premultiplied;
			}
			swap = true;
		} else {
#ifdef USE_FFMPEG
			enum AVPixelFormat pixelFormat;
			switch (vFormat) {
			case QVideoFrame::Format_YUV420P:
				pixelFormat = AV_PIX_FMT_YUV420P;
				break;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
			case QVideoFrame::Format_YUV422P:
				pixelFormat = AV_PIX_FMT_YUV422P;
				break;
#endif
			case QVideoFrame::Format_YUYV:
				pixelFormat = AV_PIX_FMT_YUYV422;
				break;
			case QVideoFrame::Format_UYVY:
				pixelFormat = AV_PIX_FMT_UYVY422;
				break;
			case QVideoFrame::Format_NV12:
				pixelFormat = AV_PIX_FMT_NV12;
				break;
			case QVideoFrame::Format_NV21:
				pixelFormat = AV_PIX_FMT_NV12;
				break;
			default:
				return false;
			}
			format = QImage::Format_RGB888;
			useScaler = true;
			if (pixelFormat != m_pixfmt || m_scalerSize != mappedFrame.size()) {
				if (m_scaler) {
					sws_freeContext(m_scaler);
				}
				m_scaler = sws_getContext(mappedFrame.width(), mappedFrame.height(), pixelFormat,
				                          mappedFrame.width(), mappedFrame.height(), AV_PIX_FMT_RGB24,
				                          SWS_POINT, nullptr, nullptr, nullptr);
				m_scalerSize = mappedFrame.size();
				m_pixfmt = pixelFormat;
			}
#else
			return false;
#endif
		}
	}
	uchar* bits = mappedFrame.bits();
#ifdef USE_FFMPEG
	QImage image;
	if (!useScaler) {
		image = QImage(bits, mappedFrame.width(), mappedFrame.height(), mappedFrame.bytesPerLine(), format);
	}
	if (useScaler) {
		image = QImage(mappedFrame.width(), mappedFrame.height(), format);
		const uint8_t* planes[8] = {0};
		int strides[8] = {0};
		for (int plane = 0; plane < mappedFrame.planeCount(); ++plane) {
			planes[plane] = mappedFrame.bits(plane);
			strides[plane] = mappedFrame.bytesPerLine(plane);
		}
		uint8_t* outBits = image.bits();
		int outStride = image.bytesPerLine();
		sws_scale(m_scaler, planes, strides, 0, mappedFrame.height(), &outBits, &outStride);
	} else
#else
	QImage image(bits, mappedFrame.width(), mappedFrame.height(), mappedFrame.bytesPerLine(), format);
#endif
	if (swap) {
		image = image.rgbSwapped();
	} else if (surfaceFormat().scanLineDirection() != QVideoSurfaceFormat::BottomToTop) {
		image = image.copy(); // Create a deep copy of the bits
	}
	if (surfaceFormat().scanLineDirection() == QVideoSurfaceFormat::BottomToTop) {
		image = image.mirrored();
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
#ifdef USE_FFMPEG
	list.append(QVideoFrame::Format_YUYV);
	list.append(QVideoFrame::Format_UYVY);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
	list.append(QVideoFrame::Format_YUV422P);
#endif
	list.append(QVideoFrame::Format_YUV420P);
	list.append(QVideoFrame::Format_NV12);
	list.append(QVideoFrame::Format_NV21);
#endif
	return list;
}
