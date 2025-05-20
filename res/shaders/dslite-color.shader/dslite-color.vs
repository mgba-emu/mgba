uniform int color_mode;
attribute vec4 position;
varying vec2 texCoord;
varying mat4 profile;

const mat4 DSL_sRGB = mat4(
	0.93, 0.025, 0.008, 0.0,  //red channel
	0.14, 0.90, -0.03, 0.0,  //green channel
	-0.07, 0.075, 1.022, 0.0,  //blue channel
	0.0,  0.0,  0.0,  0.935   //alpha channel
); 

const mat4 DSL_DCI = mat4(
	0.76, 0.055, 0.0225, 0.0,  //red channel
	0.27, 0.875, 0.0225, 0.0,  //green channel
	-0.03, 0.07, 0.955, 0.0,  //blue channel
	0.0,   0.0,   0.0,   0.97  //alpha channel
); 

const mat4 DSL_Rec2020 = mat4(
	0.585, 0.09, 0.0225, 0.0,  //red channel
	0.3725, 0.825, 0.035, 0.0,  //green channel
	0.0425, 0.085, 0.9425, 0.0,  //blue channel
	0.0,   0.0,   0.0,   1.0  //alpha channel
); 

void main() {
	if (color_mode == 1) profile = DSL_sRGB;
	else if (color_mode == 2) profile = DSL_DCI;
	else if (color_mode == 3) profile = DSL_Rec2020;

	gl_Position = position;
	texCoord = (position.st + vec2(1.0, 1.0)) * vec2(0.5, 0.5);
}
