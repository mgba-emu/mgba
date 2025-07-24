// Shader that replicates the LCD Colorspace from a Nintendo DS Lite --
varying vec2 texCoord;
varying mat4 profile;
uniform sampler2D tex;
uniform vec2 texSize;

const float target_gamma = 2.2;
const float display_gamma = 2.2;

void main() {
	// bring out our stored luminance value
	float lum = profile[3].w;

	// our adjustments need to happen in linear gamma
	vec4 screen = pow(texture2D(tex, texCoord), vec4(target_gamma)).rgba;

	screen = clamp(screen * lum, 0.0, 1.0);
	screen = profile * screen;
	gl_FragColor = pow(screen, vec4(1.0 / display_gamma));
}
