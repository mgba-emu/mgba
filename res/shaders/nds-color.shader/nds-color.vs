uniform int color_mode;
attribute vec4 position;
varying vec2 texCoord;
varying mat4 profile;

const mat4 NDS_sRGB = mat4(
	0.835, 0.10, 0.105, 0.0,  //red channel
	0.27, 0.6375, 0.175, 0.0,  //green channel
	-0.105, 0.2625, 0.72, 0.0,  //blue channel
	0.0,   0.0,   0.0,   0.905   //alpha channel
); 

const mat4 NDS_DCI = mat4(
	0.70, 0.125, 0.12, 0.0,  //red channel
	0.34, 0.625, 0.20, 0.0,  //green channel
	-0.04, 0.25, 0.68, 0.0,  //blue channel
	0.0,   0.0,   0.0,   0.96  //alpha channel
); 

const mat4 NDS_Rec2020 = mat4(
	0.555, 0.1475, 0.1175, 0.0,  //red channel
	0.39, 0.6075, 0.2075, 0.0,  //green channel
	0.055, 0.245, 0.675, 0.0,  //blue channel
	0.0,   0.0,   0.0,   1.0  //alpha channel
); 

void main() {
	if (color_mode == 1) profile = NDS_sRGB;
	else if (color_mode == 2) profile = NDS_DCI;
	else if (color_mode == 3) profile = NDS_Rec2020;

	gl_Position = position;
	texCoord = (position.st + vec2(1.0, 1.0)) * vec2(0.5, 0.5);
}
