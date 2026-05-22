varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;
uniform float boundBrightness;

void main() {
	vec4 color = texture2D(tex, texCoord);
	if (int(mod(texCoord.s * texSize.x * 6.0, 6.0)) == 5 ||
		int(mod(texCoord.t * texSize.y * 6.0, 6.0)) == 5)
	{
		color.rgb *= vec3(1.0, 1.0, 1.0) * boundBrightness;
	}
	gl_FragColor = color;
}