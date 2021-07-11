/* Shader implementation of Scale2x is adapted from https://gist.github.com/singron/3161079 */
varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;

vec4 scale2x(vec4 pixels[5], vec2 p) {
    // texel arrangement
    // x 0 x
    // 1 2 3
    // x 4 x
    // p = the texCoord within a pixel [0...1]
    p = fract(p);
	if (p.x > .5) {
		if (p.y > .5) {
			// Top Right
			return pixels[0] == pixels[3] && pixels[0] != pixels[1] && pixels[3] != pixels[4] ? pixels[3] : pixels[2];
		} else {
			// Bottom Right
			return pixels[4] == pixels[3] && pixels[1] != pixels[4] && pixels[0] != pixels[3] ? pixels[3] : pixels[2];
		}
	} else {
		if (p.y > .5) {
			// Top Left
			return pixels[1] == pixels[0] && pixels[0] != pixels[3] && pixels[1] != pixels[4] ? pixels[1] : pixels[2];
		} else {
			// Bottom Left
			return pixels[1] == pixels[4] && pixels[1] != pixels[0] && pixels[4] != pixels[3] ? pixels[1] : pixels[2];
		}
	}
}

vec4 scaleNeighborhood(vec2 p, vec2 x, vec2 o) {
	vec4 neighborhood[5];
    neighborhood[0] = texture2D(tex, texCoord + x + vec2( 0.0,  o.y));
    neighborhood[1] = texture2D(tex, texCoord + x + vec2(-o.x,  0.0));
    neighborhood[2] = texture2D(tex, texCoord + x + vec2( 0.0,  0.0));
    neighborhood[3] = texture2D(tex, texCoord + x + vec2( o.x,  0.0));
    neighborhood[4] = texture2D(tex, texCoord + x + vec2( 0.0, -o.y));
	return scale2x(neighborhood, p + x * texSize);
}

void main() {
    // o = offset, the width of a pixel
    vec2 o = 1.0 / texSize;

    vec2 p = texCoord * texSize;
	vec4 pixels[5];
	pixels[0] = scaleNeighborhood(p, vec2(       0.0,  o.y / 2.0), o);
	pixels[1] = scaleNeighborhood(p, vec2(-o.x / 2.0,        0.0), o);
	pixels[2] = scaleNeighborhood(p, vec2(       0.0,        0.0), o);
	pixels[3] = scaleNeighborhood(p, vec2( o.x / 2.0,        0.0), o);
	pixels[4] = scaleNeighborhood(p, vec2(       0.0, -o.y / 2.0), o);
	gl_FragColor = scale2x(pixels, p * 2.0);
}
