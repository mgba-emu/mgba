varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;

const float scale[32] = float[](
	  0.0/255.0,   6.0/255.0,  12.0/255.0,  18.0/255.0,  24.0/255.0,  31.0/255.0,  37.0/255.0,  43.0/255.0,
	 49.0/255.0,  55.0/255.0,  61.0/255.0,  67.0/255.0,  73.0/255.0,  79.0/255.0,  86.0/255.0,  92.0/255.0,
	 98.0/255.0, 104.0/255.0, 111.0/255.0, 117.0/255.0, 123.0/255.0, 129.0/255.0, 135.0/255.0, 141.0/255.0,
	148.0/255.0, 154.0/255.0, 159.0/255.0, 166.0/255.0, 172.0/255.0, 178.0/255.0, 184.0/255.0, 191.0/255.0
);

void main() {
	vec4 color = texture2D(tex, texCoord);
	color.rgb = round(color.rgb * 31.0);
	color = vec4(
		scale[int(color.r)],
		scale[int(color.g)],
		scale[int(color.b)],
		1.0
	);
	gl_FragColor = color;
}