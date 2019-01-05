varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;

void main() {
	vec4 color = texture2D(tex, texCoord);
	vec3 arrayX[3];
	arrayX[0] = vec3(0.2, 0.2, 1.0);
	arrayX[1] = vec3(0.2, 1.0, 0.2);
	arrayX[2] = vec3(1.0, 0.2, 0.2);
	vec3 arrayY[4];
	arrayY[0] = vec3(1.0, 1.0, 1.0);
	arrayY[1] = vec3(1.0, 1.0, 1.0);
	arrayY[2] = vec3(1.0, 1.0, 1.0);
	arrayY[3] = vec3(0.8, 0.8, 0.8);
	color.rgb = pow(color.rgb * vec3(0.8, 0.8, 0.8), vec3(1.8, 1.8, 1.8)) + vec3(0.16, 0.16, 0.16);
	color.rgb *= arrayX[int(mod(texCoord.s * texSize.x * 3.0, 3.0))];
	color.rgb *= arrayY[int(mod(texCoord.t * texSize.y * 4.0, 4.0))];
	color.a = 0.5;
	gl_FragColor = color;
}
