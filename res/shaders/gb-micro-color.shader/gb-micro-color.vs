uniform int color_mode;
attribute vec4 position;
varying vec2 texCoord;
varying mat4 profile;

const mat4 GBM_sRGB = mat4(
	0.8025, 0.10, 0.1225, 0.0,  //red channel
	0.31, 0.6875, 0.1125, 0.0,  //green channel
	-0.1125, 0.2125, 0.765, 0.0,  //blue channel
	0.0,  0.0,  0.0,  0.9   //alpha channel
); 

const mat4 GBM_DCI = mat4(
	0.6675, 0.125, 0.13, 0.0,  //red channel
	0.3825, 0.675, 0.1475, 0.0,  //green channel
	-0.05, 0.20, 0.7225, 0.0,  //blue channel
	0.0,  0.0,  0.0,  0.96   //alpha channel
); 

const mat4 GBM_Rec2020 = mat4(
	0.525, 0.15, 0.13, 0.0,  //red channel
	0.43, 0.65, 0.155, 0.0,  //green channel
	0.045, 0.20, 0.715, 0.0,  //blue channel
	0.0,  0.0,  0.0,  1.0   //alpha channel
); 

void main() {
	if (color_mode == 1) profile = GBM_sRGB;
	else if (color_mode == 2) profile = GBM_DCI;
	else if (color_mode == 3) profile = GBM_Rec2020;

	gl_Position = position;
	texCoord = (position.st + vec2(1.0, 1.0)) * vec2(0.5, 0.5);
}
