varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;
uniform float subPixelDimming;

void main() {
	vec4 color = texture2D(tex, texCoord);
	vec3 subPixels[3];
	float subPixelDimming = 1.0 - subPixelDimming;
	subPixels[0] = vec3(1.0, subPixelDimming, subPixelDimming);
	subPixels[1] = vec3(subPixelDimming, 1.0, subPixelDimming);
	subPixels[2] = vec3(subPixelDimming, subPixelDimming, 1.0);
	color.rgb *= subPixels[int(mod(texCoord.s * texSize.x * 3.0, 3.0))];
	gl_FragColor = color;
}	