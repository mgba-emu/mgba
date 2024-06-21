/* MIT License
*
* Copyright (c) 2015-2023 Lior Halphon
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
/* Based on this (really good) article: http://blog.pkh.me/p/19-butchering-hqx-scaling-filters.html */

/* The colorspace used by the HQnx filters is not really YUV, despite the algorithm description claims it is. It is
   also not normalized. Therefore, we shall call the colorspace used by HQnx "HQ Colorspace" to avoid confusion. */
varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;

vec3 rgb_to_hq_colospace(vec4 rgb)
{
	return vec3( 0.250 * rgb.r + 0.250 * rgb.g + 0.250 * rgb.b,
				 0.250 * rgb.r - 0.000 * rgb.g - 0.250 * rgb.b,
				-0.125 * rgb.r + 0.250 * rgb.g - 0.125 * rgb.b);
}

bool is_different(vec4 a, vec4 b)
{
	vec3 diff = abs(rgb_to_hq_colospace(a) - rgb_to_hq_colospace(b));
	return diff.x > 0.018 || diff.y > 0.002 || diff.z > 0.005;
}

#define P(m, r) ((pattern & (m)) == (r))

vec4 interp_2px(vec4 c1, float w1, vec4 c2, float w2)
{
	return (c1 * w1 + c2 * w2) / (w1 + w2);
}

vec4 interp_3px(vec4 c1, float w1, vec4 c2, float w2, vec4 c3, float w3)
{
	return (c1 * w1 + c2 * w2 + c3 * w3) / (w1 + w2 + w3);
}

vec4 scale(sampler2D image, vec2 position, vec2 input_resolution)
{
	// o = offset, the width of a pixel
	vec2 o = vec2(1, 1) / input_resolution;

	/* We always calculate the top left pixel.  If we need a different pixel, we flip the image */

	// p = the position within a pixel [0...1]
	vec2 p = fract(position * input_resolution);

	if (p.x > 0.5) o.x = -o.x;
	if (p.y > 0.5) o.y = -o.y;

	vec4 w0 = texture2D(image, position + vec2( -o.x, -o.y));
	vec4 w1 = texture2D(image, position + vec2(    0, -o.y));
	vec4 w2 = texture2D(image, position + vec2(  o.x, -o.y));
	vec4 w3 = texture2D(image, position + vec2( -o.x,    0));
	vec4 w4 = texture2D(image, position + vec2(    0,    0));
	vec4 w5 = texture2D(image, position + vec2(  o.x,    0));
	vec4 w6 = texture2D(image, position + vec2( -o.x,  o.y));
	vec4 w7 = texture2D(image, position + vec2(    0,  o.y));
	vec4 w8 = texture2D(image, position + vec2(  o.x,  o.y));

	int pattern = 0;
	if (is_different(w0, w4)) pattern |= 1;
	if (is_different(w1, w4)) pattern |= 2;
	if (is_different(w2, w4)) pattern |= 4;
	if (is_different(w3, w4)) pattern |= 8;
	if (is_different(w5, w4)) pattern |= 16;
	if (is_different(w6, w4)) pattern |= 32;
	if (is_different(w7, w4)) pattern |= 64;
	if (is_different(w8, w4)) pattern |= 128;

	if ((P(0xBF,0x37) || P(0xDB,0x13)) && is_different(w1, w5)) {
		return interp_2px(w4, 3.0, w3, 1.0);
	}
	if ((P(0xDB,0x49) || P(0xEF,0x6D)) && is_different(w7, w3)) {
		return interp_2px(w4, 3.0, w1, 1.0);
	}
	if ((P(0x0B,0x0B) || P(0xFE,0x4A) || P(0xFE,0x1A)) && is_different(w3, w1)) {
		return w4;
	}
	if ((P(0x6F,0x2A) || P(0x5B,0x0A) || P(0xBF,0x3A) || P(0xDF,0x5A) ||
		 P(0x9F,0x8A) || P(0xCF,0x8A) || P(0xEF,0x4E) || P(0x3F,0x0E) ||
		 P(0xFB,0x5A) || P(0xBB,0x8A) || P(0x7F,0x5A) || P(0xAF,0x8A) ||
		 P(0xEB,0x8A)) && is_different(w3, w1)) {
		return interp_2px(w4, 3.0, w0, 1.0);
	}
	if (P(0x0B,0x08)) {
		return interp_3px(w4, 2.0, w0, 1.0, w1, 1.0);
	}
	if (P(0x0B,0x02)) {
		return interp_3px(w4, 2.0, w0, 1.0, w3, 1.0);
	}
	if (P(0x2F,0x2F)) {
		return interp_3px(w4, 4.0, w3, 1.0, w1, 1.0);
	}
	if (P(0xBF,0x37) || P(0xDB,0x13)) {
		return interp_3px(w4, 5.0, w1, 2.0, w3, 1.0);
	}
	if (P(0xDB,0x49) || P(0xEF,0x6D)) {
		return interp_3px(w4, 5.0, w3, 2.0, w1, 1.0);
	}
	if (P(0x1B,0x03) || P(0x4F,0x43) || P(0x8B,0x83) || P(0x6B,0x43)) {
		return interp_2px(w4, 3.0, w3, 1.0);
	}
	if (P(0x4B,0x09) || P(0x8B,0x89) || P(0x1F,0x19) || P(0x3B,0x19)) {
		return interp_2px(w4, 3.0, w1, 1.0);
	}
	if (P(0x7E,0x2A) || P(0xEF,0xAB) || P(0xBF,0x8F) || P(0x7E,0x0E)) {
		return interp_3px(w4, 2.0, w3, 3.0, w1, 3.0);
	}
	if (P(0xFB,0x6A) || P(0x6F,0x6E) || P(0x3F,0x3E) || P(0xFB,0xFA) ||
		P(0xDF,0xDE) || P(0xDF,0x1E)) {
		return interp_2px(w4, 3.0, w0, 1.0);
	}
	if (P(0x0A,0x00) || P(0x4F,0x4B) || P(0x9F,0x1B) || P(0x2F,0x0B) ||
		P(0xBE,0x0A) || P(0xEE,0x0A) || P(0x7E,0x0A) || P(0xEB,0x4B) ||
		P(0x3B,0x1B)) {
		return interp_3px(w4, 2.0, w3, 1.0, w1, 1.0);
	}

	return interp_3px(w4, 6.0, w3, 1.0, w1, 1.0);
}

void main() {
	gl_FragColor = scale(tex, texCoord, texSize);
}
