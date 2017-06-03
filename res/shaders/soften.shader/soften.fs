/*
   Soften Shader

   Copyright (C) 2017 Dominus Iniquitatis - zerosaiko@gmail.com

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

uniform sampler2D tex;
uniform vec2 texSize;
varying vec2 texCoord;

uniform float amount;

vec2 GetTexelSize()
{
	return vec2(1.0 / texSize.x, 1.0 / texSize.y);
}

void main()
{
	vec4 color = texture2D(tex, texCoord);

	vec4 northColor = texture2D(tex, texCoord + vec2(0.0, GetTexelSize().y));
	vec4 southColor = texture2D(tex, texCoord - vec2(0.0, GetTexelSize().y));
	vec4 eastColor = texture2D(tex, texCoord + vec2(GetTexelSize().x, 0.0));
	vec4 westColor = texture2D(tex, texCoord - vec2(GetTexelSize().x, 0.0));

	if (abs(length(color) - length(northColor)) > 0.0)
	{
		color = mix(color, northColor, amount / 4.0);
	}

	if (abs(length(color) - length(southColor)) > 0.0)
	{
		color = mix(color, southColor, amount / 4.0);
	}

	if (abs(length(color) - length(eastColor)) > 0.0)
	{
		color = mix(color, eastColor, amount / 4.0);
	}

	if (abs(length(color) - length(westColor)) > 0.0)
	{
		color = mix(color, westColor, amount / 4.0);
	}

	gl_FragColor = color;
}
