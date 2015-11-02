varying vec2 texCoord;
uniform sampler2D tex;

void main() {
	vec4 color = texture2D(tex, texCoord);
	vec3 arrayX[4];
	arrayX[0] = vec3(1.0, 0.2, 0.2);
	arrayX[1] = vec3(0.2, 1.0, 0.2);
	arrayX[2] = vec3(0.2, 0.2, 1.0);
	arrayX[3] = vec3(0.4, 0.4, 0.4);
	vec3 arrayY[4];
	arrayY[0] = vec3(1.0, 1.0, 1.0);
	arrayY[1] = vec3(1.0, 1.0, 1.0);
	arrayY[2] = vec3(1.0, 1.0, 1.0);
	arrayY[3] = vec3(0.8, 0.8, 0.8);
	vec3 bleed = vec3(0.7, 0.7, 0.7);
	vec2 radius = (texCoord.st - vec2(0.5, 0.5)) * vec2(1.5, 1.5);
	bleed += (pow(dot(radius, radius), 3.0) + vec3(0.06, 0.1, 0.2)) * vec3(0.8, 0.88, 1.2);
	color.rgb = pow(color.rgb + vec3(0.1, 0.12, 0.2), vec3(1.6, 1.6, 1.6)) * bleed;
	color.rgb *= arrayX[int(mod(texCoord.s * 960.0, 4.0))];
	color.rgb *= arrayY[int(mod(texCoord.t * 640.0, 4.0))];
	color.rgb += vec3(0.16, 0.18, 0.22);
	color.a = 0.5;
	gl_FragColor = color;
}
