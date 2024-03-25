/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoDumper.h"

#include <QImage>

using namespace QGBA;

VideoDumper::VideoDumper(QObject* parent)
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	: QAbstractVideoSurface(parent)
#else
	: QObject(parent)
#endif
{
}

bool VideoDumper::present(const QVideoFrame& frame) {
	QVideoFrame mappedFrame(frame);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	if (!mappedFrame.map(QAbstractVideoBuffer::ReadOnly)) {
		return false;
	}
#else
	if (!mappedFrame.map(QVideoFrame::ReadOnly)) {
		return false;
	}
#endif
	PixelFormat vFormat = mappedFrame.pixelFormat();
	QImage::Format format = imageFormatFromPixelFormat(vFormat);
	bool swap = false;
#ifdef USE_FFMPEG
	bool useScaler = false;
#endif
	if (format == QImage::Format_Invalid) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
		if (vFormat < QVideoFrame::Format_BGRA5658_Premultiplied) {
			vFormat = static_cast<QVideoFrame::PixelFormat>(vFormat - QVideoFrame::Format_BGRA32 + QVideoFrame::Format_ARGB32);
#else
		if (vFormat < PixelFormat::Format_AYUV) {
#endif
			format = imageFormatFromPixelFormat(vFormat);
			swap = true;
		} else {
#ifdef USE_FFMPEG
			enum AVPixelFormat pixelFormat;
			switch (vFormat) {
			case VideoDumper::PixelFormat::Format_YUV420P:
				pixelFormat = AV_PIX_FMT_YUV420P;
				break;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
			case VideoDumper::PixelFormat::Format_YUV422P:
				pixelFormat = AV_PIX_FMT_YUV422P;
				break;
#endif
			case VideoDumper::PixelFormat::Format_YUYV:
				pixelFormat = AV_PIX_FMT_YUYV422;
				break;
			case VideoDumper::PixelFormat::Format_UYVY:
				pixelFormat = AV_PIX_FMT_UYVY422;
				break;
			case VideoDumper::PixelFormat::Format_NV12:
				pixelFormat = AV_PIX_FMT_NV12;
				break;
			case VideoDumper::PixelFormat::Format_NV21:
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
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	Direction direction = surfaceFormat().scanLineDirection();
#else
	Direction direction = mappedFrame.surfaceFormat().scanLineDirection();
#endif
#ifdef USE_FFMPEG
	QImage image;
	if (!useScaler) {
		image = QImage(mappedFrame.bits(0), mappedFrame.width(), mappedFrame.height(), mappedFrame.bytesPerLine(0), format);
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
	QImage image(mappedFrame.bits(0), mappedFrame.width(), mappedFrame.height(), mappedFrame.bytesPerLine(0), format);
#endif
	if (swap) {
		image = image.rgbSwapped();
	} else if (direction != Direction::BottomToTop) {
		image = image.copy(); // Create a deep copy of the bits
	}
	if (direction == Direction::BottomToTop) {
		image = image.mirrored();
	}
	mappedFrame.unmap();
	emit imageAvailable(image);
	return true;
}

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
QList<QVideoFrame::PixelFormat> VideoDumper::supportedPixelFormats(QAbstractVideoBuffer::HandleType) const {
	return VideoDumper::supportedPixelFormats();
}
#endif

QList<VideoDumper::PixelFormat> VideoDumper::supportedPixelFormats() {
	QList<VideoDumper::PixelFormat> list{{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
		VideoDumper::PixelFormat::Format_RGB32,
		VideoDumper::PixelFormat::Format_ARGB32,
		VideoDumper::PixelFormat::Format_RGB24,
		VideoDumper::PixelFormat::Format_ARGB32_Premultiplied,
		VideoDumper::PixelFormat::Format_RGB565,
		VideoDumper::PixelFormat::Format_RGB555,
		VideoDumper::PixelFormat::Format_BGR32,
		VideoDumper::PixelFormat::Format_BGRA32,
		VideoDumper::PixelFormat::Format_BGR24,
		VideoDumper::PixelFormat::Format_BGRA32_Premultiplied,
		VideoDumper::PixelFormat::Format_BGR565,
		VideoDumper::PixelFormat::Format_BGR555,
#else
		VideoDumper::PixelFormat::Format_XRGB8888,
		VideoDumper::PixelFormat::Format_ARGB8888,
		VideoDumper::PixelFormat::Format_ARGB8888_Premultiplied,
		VideoDumper::PixelFormat::Format_RGBX8888,
		VideoDumper::PixelFormat::Format_RGBA8888,
		VideoDumper::PixelFormat::Format_XBGR8888,
		VideoDumper::PixelFormat::Format_ABGR8888,
		VideoDumper::PixelFormat::Format_BGRX8888,
		VideoDumper::PixelFormat::Format_BGRA8888,
		VideoDumper::PixelFormat::Format_BGRA8888_Premultiplied,
#endif
#ifdef USE_FFMPEG
		VideoDumper::PixelFormat::Format_YUYV,
		VideoDumper::PixelFormat::Format_UYVY,
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
		VideoDumper::PixelFormat::Format_YUV422P,
#endif
		VideoDumper::PixelFormat::Format_YUV420P,
		VideoDumper::PixelFormat::Format_NV12,
		VideoDumper::PixelFormat::Format_NV21,
#endif
	}};
	return list;
}

QImage::Format VideoDumper::imageFormatFromPixelFormat(VideoDumper::PixelFormat vFormat) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	QImage::Format format = QVideoFrame::imageFormatFromPixelFormat(vFormat);
#else
	QImage::Format format = QVideoFrameFormat::imageFormatFromPixelFormat(vFormat);
#endif
	if (format == QImage::Format_ARGB32) {
		format = QImage::Format_RGBA8888;
	} else if (format == QImage::Format_ARGB32_Premultiplied) {
		format = QImage::Format_RGBA8888_Premultiplied;
	}
	return format;
}
