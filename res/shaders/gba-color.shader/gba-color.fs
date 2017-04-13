varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;

uniform float darken_screen;
uniform float target_gamma;
uniform float display_gamma;
uniform float sat;
uniform float lum;
uniform float contrast;
uniform float blr;
uniform float blg;
uniform float blb;
uniform float r;
uniform float g;
uniform float b;
uniform float rg;
uniform float rb;
uniform float gr;
uniform float gb;
uniform float br;
uniform float bg;

void main() {
   vec4 screen = pow(texture2D(tex, texCoord), vec4(target_gamma + darken_screen)).rgba;
   vec4 avglum = vec4(0.5);
   screen = mix(screen, avglum, (1.0 - contrast));
 
mat4 color = mat4(r,  rg,  rb, 0.0,
			     gr,   g,  gb, 0.0,
			     br,  bg,   b, 0.0,
			    blr, blg, blb, 1.0);
			  
mat4 adjust = mat4((1.0 - sat) * 0.3086 + sat, (1.0 - sat) * 0.3086, (1.0 - sat) * 0.3086, 1.0,
(1.0 - sat) * 0.6094, (1.0 - sat) * 0.6094 + sat, (1.0 - sat) * 0.6094, 1.0,
(1.0 - sat) * 0.0820, (1.0 - sat) * 0.0820, (1.0 - sat) * 0.0820 + sat, 1.0,
0.0, 0.0, 0.0, 1.0);
	color *= adjust;
	screen = clamp(screen * lum, 0.0, 1.0);
	screen = color * screen;
	gl_FragColor = pow(screen, vec4(1.0 / display_gamma + (darken_screen / 8.)));
}
