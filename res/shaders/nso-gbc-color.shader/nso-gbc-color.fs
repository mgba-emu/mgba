// Shader that replicates the Nintendo Switch Online's GBC color filter --
varying vec2 texCoord;
varying mat4 profile;
uniform sampler2D tex;
uniform vec2 texSize;

uniform float lighten_screen;

void main() {
	// bring out our stored luminance value
	float lum = profile[3].w;

	// our adjustments need to happen in linear gamma
	vec4 screen = pow(texture2D(tex, texCoord), vec4(1.24, 0.8, 0.7, 1.0)).rgba;

	screen = clamp(screen * lum, 0.0, 1.0);
	screen = profile * screen;
	gl_FragColor = pow(screen, vec4(1.0));
}
