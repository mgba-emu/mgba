/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/convolve.h>

void ConvolutionKernelCreate(struct ConvolutionKernel* kernel, size_t rank, size_t* dims) {
	kernel->rank = rank;
	kernel->dims = malloc(sizeof(kernel->dims[0]) * rank);
	size_t ksize = 1;
	size_t i;
	for (i = 0; i < rank; ++i) {
		kernel->dims[i] = dims[i];
		ksize *= dims[i];
	}
	kernel->kernel = calloc(ksize, sizeof(float));
}

void ConvolutionKernelDestroy(struct ConvolutionKernel* kernel) {
	free(kernel->kernel);
	free(kernel->dims);
	kernel->kernel = NULL;
	kernel->dims = NULL;
	kernel->rank = 0;
}

void ConvolutionKernelFillRadial(struct ConvolutionKernel* kernel, bool normalize) {
	if (kernel->rank != 2) {
		return;
	}
	float support;
	if (normalize) {
		support = 12.f / (M_PI * (kernel->dims[0] - 1) * (kernel->dims[1] - 1));
	} else {
		support = 1.f;
	}
	float wr = (kernel->dims[0] - 1) / 2.f;
	float hr = (kernel->dims[1] - 1) / 2.f;
	float* elem = kernel->kernel;
	size_t y;
	for (y = 0; y < kernel->dims[1]; ++y) {
		size_t x;
		for (x = 0; x < kernel->dims[0]; ++x) {
			float r = (1.f - hypotf((x - wr) / wr, (y - hr) / hr)) * support;
			*elem = fmaxf(0, r);
			++elem;
		}
	}
}

void ConvolutionKernelFillCircle(struct ConvolutionKernel* kernel, bool normalize) {
	if (kernel->rank != 2) {
		return;
	}
	float support;
	if (normalize) {
		support = 4.f / (M_PI * (kernel->dims[0] - 1) * (kernel->dims[1] - 1));
	} else {
		support = 1.f;
	}
	float wr = (kernel->dims[0] - 1) / 2.f;
	float hr = (kernel->dims[1] - 1) / 2.f;
	float* elem = kernel->kernel;
	size_t y;
	for (y = 0; y < kernel->dims[1]; ++y) {
		size_t x;
		for (x = 0; x < kernel->dims[0]; ++x) {
			float r = hypotf((x - wr) / wr, (y - hr) / hr);
			*elem = r <= 1.f ? support : 0.f;
			++elem;
		}
	}
}

void Convolve1DPad0PackedS32(const int32_t* restrict src, int32_t* restrict dst, size_t length, const struct ConvolutionKernel* restrict kernel) {
	if (kernel->rank != 1) {
		return;
	}
	size_t kx2 = kernel->dims[0] / 2;
	size_t x;
	for (x = 0; x < length; ++x) {
		float sum = 0.f;
		size_t kx;
		for (kx = 0; kx < kernel->dims[0]; ++kx) {
			if (x + kx <= kx2) {
				continue;
			}
			size_t cx = x + kx - kx2;
			if (cx >= length) {
				continue;
			}
			sum += src[cx] * kernel->kernel[kx];
		}
		*dst = sum;
		++dst;
	}
}

void Convolve2DClampPacked8(const uint8_t* restrict src, uint8_t* restrict dst, size_t width, size_t height, size_t stride, const struct ConvolutionKernel* restrict kernel) {
	if (kernel->rank != 2) {
		return;
	}
	size_t kx2 = kernel->dims[0] / 2;
	size_t ky2 = kernel->dims[1] / 2;
	size_t y;
	for (y = 0; y < height; ++y) {
		uint8_t* orow = &dst[y * stride];
		size_t x;
		for (x = 0; x < width; ++x) {
			float sum = 0.f;
			size_t ky;
			for (ky = 0; ky < kernel->dims[1]; ++ky) {
				size_t cy = 0;
				if (y + ky > ky2) {
					cy = y + ky - ky2;
				}
				if (cy >= height) {
					cy = height - 1;
				}
				const uint8_t* irow = &src[cy * stride];
				size_t kx;
				for (kx = 0; kx < kernel->dims[0]; ++kx) {
					size_t cx = 0;
					if (x + kx > kx2) {
						cx = x + kx - kx2;
					}
					if (cx >= width) {
						cx = width - 1;
					}
					sum += irow[cx] * kernel->kernel[ky * kernel->dims[0] + kx];
				}
			}
			*orow = sum;
			++orow;
		}
	}
}

void Convolve2DClampChannels8(const uint8_t* restrict src, uint8_t* restrict dst, size_t width, size_t height, size_t stride, size_t channels, const struct ConvolutionKernel* restrict kernel) {
	if (kernel->rank != 2) {
		return;
	}
	size_t kx2 = kernel->dims[0] / 2;
	size_t ky2 = kernel->dims[1] / 2;
	size_t y;
	for (y = 0; y < height; ++y) {
		uint8_t* orow = &dst[y * stride];
		size_t x;
		for (x = 0; x < width; ++x) {
			size_t c;
			for (c = 0; c < channels; ++c) {
				float sum = 0.f;
				size_t ky;
				for (ky = 0; ky < kernel->dims[1]; ++ky) {
					size_t cy = 0;
					if (y + ky > ky2) {
						cy = y + ky - ky2;
					}
					if (cy >= height) {
						cy = height - 1;
					}
					const uint8_t* irow = &src[cy * stride];
					size_t kx;
					for (kx = 0; kx < kernel->dims[0]; ++kx) {
						size_t cx = 0;
						if (x + kx > kx2) {
							cx = x + kx - kx2;
						}
						if (cx >= width) {
							cx = width - 1;
						}
						cx *= channels;
						sum += irow[cx + c] * kernel->kernel[ky * kernel->dims[0] + kx];
					}
				}
				*orow = sum;
				++orow;
			}
		}
	}
}
