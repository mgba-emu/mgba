/*
   fish shader
   
   algorithm and original implementation by Miloslav "drummyfish" Ciz
   (tastyfish@seznam.cz)

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;

uniform float similarity_threshold;

vec4 texel_fetch(sampler2D t, ivec2 c)   // because GLSL TexelFetch is not supported
  {
    return texture2D(tex,   (2 * vec2(c) + vec2(1,1)) / (2 * texSize) );
  }

float pixel_brightness(vec4 pixel)
  {
    return 0.21 * pixel.x + 0.72 * pixel.y + 0.07 * pixel.z;
  }

bool pixel_is_brighter(vec4 pixel1, vec4 pixel2)
  {
    return pixel_brightness(pixel1) > pixel_brightness(pixel2);
  }

vec3 pixel_to_yuv(vec4 pixel)
  {
    float y = 0.299 * pixel.x + 0.587 * pixel.y + 0.114 * pixel.z;
    return vec3(y, 0.492 * (pixel.z - y), 0.877 * (pixel.x - y));
  }

bool yuvs_are_similar(vec3 yuv1, vec3 yuv2)
  {
    vec3 yuv_difference = abs(yuv1 - yuv2);
    return yuv_difference.x <= similarity_threshold && yuv_difference.y <= similarity_threshold && yuv_difference.z <= similarity_threshold;
  }

bool pixels_are_similar(vec4 pixel1, vec4 pixel2)
  {
    vec3 yuv1 = pixel_to_yuv(pixel1);
    vec3 yuv2 = pixel_to_yuv(pixel2);

    return yuvs_are_similar(yuv1, yuv2);
  }

vec4 interpolate_nondiagonal(vec4 neighbour1, vec4 neighbour2)
  {
    if (pixels_are_similar(neighbour1,neighbour2))
      return mix(neighbour1,neighbour2,0.5);
    else
      return pixel_is_brighter(neighbour1, neighbour2) ? neighbour1 : neighbour2;
  }

vec4 mix3(vec4 value1, vec4 value2, vec4 value3)
  {
    return (value1 + value2 + value3) / 3.0;
  }

vec4 straight_line(vec4 p0, vec4 p1, vec4 p2, vec4 p3)
  {
    return pixel_is_brighter(p2,p0) ? mix(p2,p3,0.5) : mix(p0,p1,0.5);
  }

vec4 corner(vec4 p0, vec4 p1, vec4 p2, vec4 p3)
  {
    return pixel_is_brighter(p1,p0) ? mix3(p1,p2,p3) : mix3(p0,p1,p2);
  }

vec4 interpolate_diagonal(vec4 a, vec4 b, vec4 c, vec4 d)
  {
    // a b
    // c d

    vec3 a_yuv = pixel_to_yuv(a);
    vec3 b_yuv = pixel_to_yuv(b);
    vec3 c_yuv = pixel_to_yuv(c);
    vec3 d_yuv = pixel_to_yuv(d);

    bool ad = yuvs_are_similar(a_yuv,d_yuv);
    bool bc = yuvs_are_similar(b_yuv,c_yuv);
    bool ab = yuvs_are_similar(a_yuv,b_yuv);
    bool cd = yuvs_are_similar(c_yuv,d_yuv);
    bool ac = yuvs_are_similar(a_yuv,c_yuv);
    bool bd = yuvs_are_similar(b_yuv,d_yuv);

    if (ad && cd && ab)                                          // all pixels are equal?
      return( mix(mix(a,b,0.5), mix(c,d,0.5), 0.5) );

    else if (ac && cd && ! ab)                                   // corner 1?
      return corner(b,a,d,c);
    else if (bd && cd && ! ab)                                   // corner 2?
      return corner(a,b,c,d);
    else if (ac && ab && ! bd)                                   // corner 3?
      return corner(d,c,b,a);
    else if (ab && bd && ! ac)                                   // corner 4?
      return corner(c,a,d,b);

    else if (ad && (!bc || pixel_is_brighter(b,a)))              // diagonal line 1?
      return mix(a,d,0.5);
    else if (bc && (!ad || pixel_is_brighter(a,b)))              // diagonal line 2?
      return mix(b,c,0.5);

    else if (ab)                                                 // horizontal line 1?
      return straight_line(a,b,c,d);
    else if (cd)                                                 // horizontal line 2?
      return straight_line(c,d,a,b);

    else if (ac)                                                 // vertical line 1?
      return straight_line(a,c,b,d);
    else if (bd)                                                 // vertical line 2?
      return straight_line(b,d,a,c);

    return( mix(mix(a,b,0.5), mix(c,d,0.5), 0.5) );
  }

void main()
  {
    ivec2 pixel_coords2 = ivec2(texCoord * texSize * 2);
    ivec2 pixel_coords = pixel_coords2 / 2;

    bool x_even = mod(pixel_coords2.x,2) == 0;
    bool y_even = mod(pixel_coords2.y,2) == 0;

    if (x_even)
      {
        if (y_even)
          {

            gl_FragColor = interpolate_diagonal(
              texel_fetch(tex, pixel_coords + ivec2(-1,-1)),
              texel_fetch(tex, pixel_coords + ivec2(0,-1)),
              texel_fetch(tex, pixel_coords + ivec2(-1,0)),
              texel_fetch(tex, pixel_coords + ivec2(0,0))
              );

          }
        else
          {
            gl_FragColor = interpolate_nondiagonal
              (
                texel_fetch(tex, pixel_coords + ivec2(-1,0)),
                texel_fetch(tex, pixel_coords)
              );
          }
      }
    else if (y_even)
      {
        gl_FragColor = interpolate_nondiagonal
          (
            texel_fetch(tex, pixel_coords + ivec2(0,-1)),
            texel_fetch(tex, pixel_coords)
          );
      }
    else
      gl_FragColor = texel_fetch(tex, pixel_coords);
  }
