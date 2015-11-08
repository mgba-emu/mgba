varying vec2 texCoord;
uniform sampler2D tex;

void main() {
	vec4 color = texture2D(tex, texCoord);
	vec4 reflection = texture2D(tex, texCoord - vec2(0, 0.025));
	vec3 bleed = vec3(0.12, 0.14, 0.19);
	vec2 radius = (texCoord.st - vec2(0.5, 0.5)) * vec2(1.6, 1.6);
	bleed += (dot(pow(radius, vec2(4.0)), pow(radius, vec2(4.0))) + vec3(0.02, 0.03, 0.05)) * vec3(0.14, 0.18, 0.2);
	color.rgb += bleed;
	color.rgb += reflection.rgb * 0.07;
	color.a = 1.0;
	gl_FragColor = color;
}
