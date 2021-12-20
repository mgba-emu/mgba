varying vec2 texCoord;
uniform sampler2D tex;
uniform float brightness;

void main() {
	vec4 color = texture2D(tex, texCoord);
	color.rgb *= brightness;
	gl_FragColor = color;
}
