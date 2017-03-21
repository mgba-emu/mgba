varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;

void main() {
	float scale[32];
	scale[ 0] =   0.0/255.0; scale[ 1] = 6.0/255.0;
	scale[ 2] =  12.0/255.0; scale[ 3] = 18.0/255.0;
	scale[ 4] =  24.0/255.0; scale[ 5] = 31.0/255.0;
	scale[ 6] =  37.0/255.0; scale[ 7] = 43.0/255.0;
	scale[ 8] =  49.0/255.0; scale[ 9] = 55.0/255.0;
	scale[10] =  61.0/255.0; scale[11] = 67.0/255.0;
	scale[12] =  73.0/255.0; scale[13] = 79.0/255.0;
	scale[14] =  86.0/255.0; scale[15] = 92.0/255.0;
	scale[16] =  98.0/255.0; scale[17] = 104.0/255.0;
	scale[18] = 111.0/255.0; scale[19] = 117.0/255.0;
	scale[20] = 123.0/255.0; scale[21] = 129.0/255.0;
	scale[22] = 135.0/255.0; scale[23] = 141.0/255.0;
	scale[24] = 148.0/255.0; scale[25] = 154.0/255.0;
	scale[26] = 159.0/255.0; scale[27] = 166.0/255.0;
	scale[28] = 172.0/255.0; scale[29] = 178.0/255.0;
	scale[30] = 184.0/255.0; scale[31] = 191.0/255.0;
	
	vec4 color = texture2D(tex, texCoord);
	color.r = scale[int(floor(color.r * 31.0 + 0.5))];
	color.g = scale[int(floor(color.g * 31.0 + 0.5))];
	color.b = scale[int(floor(color.b * 31.0 + 0.5))];
	gl_FragColor = color;
}
