varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;

uniform float darken_screen;
const float target_gamma = 2.2;
const float display_gamma = 2.5;
const float sat = 1.0;
const float lum = 0.99;
const float contrast = 1.0;
const vec3 bl = vec3(0.0, 0.0, 0.0);
const vec3 r = vec3(0.84, 0.09, 0.15);
const vec3 g = vec3(0.18, 0.67, 0.10);
const vec3 b = vec3(0.0, 0.26, 0.73);

void main() {
	vec4 screen = pow(texture2D(tex, texCoord), vec4(target_gamma + darken_screen)).rgba;
	vec4 avglum = vec4(0.5);
	screen = mix(screen, avglum, (1.0 - contrast));
 
	mat4 color = mat4(	r.r,	r.g,	r.b,	0.0,
				g.r,	g.g,	g.b,	0.0,
				b.r,	b.g,	b.b,	0.0,
				bl.r,	bl.g,	bl.b,	1.0);
			  
	mat4 adjust = mat4(	(1.0 - sat) * 0.3086 + sat,	(1.0 - sat) * 0.3086,		(1.0 - sat) * 0.3086,		1.0,
				(1.0 - sat) * 0.6094,		(1.0 - sat) * 0.6094 + sat,	(1.0 - sat) * 0.6094,		1.0,
				(1.0 - sat) * 0.0820,		(1.0 - sat) * 0.0820,		(1.0 - sat) * 0.0820 + sat,	1.0,
				0.0,				0.0,				0.0,				1.0);
	color *= adjust;
	screen = clamp(screen * lum, 0.0, 1.0);
	screen = color * screen;
	gl_FragColor = pow(screen, vec4(1.0 / display_gamma + (darken_screen * 0.125)));
}
