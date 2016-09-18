varying vec2 texCoord;
uniform sampler2D tex;
uniform float reflectionBrightness;
uniform vec2 reflectionDistance;
uniform float lightBrightness;

const float speed = 2.0;
const float decay = 2.0;
const float coeff = 2.5;

void main() {
	float sp = pow(speed, lightBrightness);
	float dc = pow(decay, -lightBrightness);
	float s = (sp - dc) / (sp + dc);
	vec2 radius = (texCoord.st - vec2(0.5, 0.5)) * vec2(coeff * s);
	radius = pow(abs(radius), vec2(4.0));
	vec3 bleed = vec3(0.12, 0.14, 0.19);
	bleed += (dot(radius, radius) + vec3(0.02, 0.03, 0.05)) * vec3(0.14, 0.18, 0.2);

	vec4 color = texture2D(tex, texCoord);
	color.rgb += pow(bleed, pow(vec3(lightBrightness), vec3(-0.5)));

	vec4 reflection = texture2D(tex, texCoord - reflectionDistance);
	color.rgb += reflection.rgb * reflectionBrightness;
	color.a = 1.0;
	gl_FragColor = color;
}
