uniform int color_mode;
attribute vec4 position;
varying vec2 texCoord;
varying mat4 profile;

const mat4 GBC_sRGB = mat4(
	0.84, 0.105, 0.15, 0.0,  //red channel
	0.265, 0.67, 0.30, 0.0,  //green channel
	0.0, 0.24, 0.525, 0.0,  //blue channel
	0.175,  0.18,  0.18,  0.85   //alpha channel
); 

const mat4 GBC_DCI = mat4(
	0.84, 0.105, 0.15, 0.0,  //red channel
	0.265, 0.67, 0.30, 0.0,  //green channel
	0.0, 0.24, 0.525, 0.0,  //blue channel
	0.175,  0.18,  0.18,  1.0   //alpha channel
); 

const mat4 GBC_Rec2020 = mat4(
	0.84, 0.105, 0.15, 0.0,  //red channel
	0.265, 0.67, 0.30, 0.0,  //green channel
	0.0, 0.24, 0.525, 0.0,  //blue channel
	0.175,  0.18,  0.18,  1.0   //alpha channel
); 

void main() {
	if (color_mode == 1) profile = GBC_sRGB;
	else if (color_mode == 2) profile = GBC_DCI;
	else if (color_mode == 3) profile = GBC_Rec2020;

	gl_Position = position;
	texCoord = (position.st + vec2(1.0, 1.0)) * vec2(0.5, 0.5);
}
