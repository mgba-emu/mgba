/* Shader implementation of Scale2x is adapted from https://gist.github.com/singron/3161079 */
varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;

void main() {
    // o = offset, the width of a pixel
    vec2 o = 1.0 / texSize;

    // texel arrangement
    // A B C
    // D E F
    // G H I
    vec4 B = texture2D(tex, texCoord + vec2(  0.0,  o.y));
    vec4 D = texture2D(tex, texCoord + vec2( -o.x,  0.0));
    vec4 E = texture2D(tex, texCoord + vec2(  0.0,  0.0));
    vec4 F = texture2D(tex, texCoord + vec2(  o.x,  0.0));
    vec4 H = texture2D(tex, texCoord + vec2(  0.0, -o.y));
    vec2 p = texCoord * texSize;
    // p = the texCoord within a pixel [0...1]
    p = fract(p);
	if (p.x > .5) {
		if (p.y > .5) {
			// Top Right
			gl_FragColor = B == F && B != D && F != H ? F : E;
		} else {
			// Bottom Right
			gl_FragColor = H == F && D != H && B != F ? F : E;
		}
	} else {
		if (p.y > .5) {
			// Top Left
			gl_FragColor = D == B && B != F && D != H ? D : E;
		} else {
			// Bottom Left
			gl_FragColor = D == H && D != B && H != F ? D : E;
		}
	}
}
