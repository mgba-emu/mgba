uniform int color_mode;
attribute vec4 position;
varying vec2 texCoord;
varying mat4 profile;

const mat4 SP1_sRGB = mat4(
	0.96, 0.0325, 0.001, 0.0,  //red channel
	0.11, 0.89, -0.03, 0.0,  //green channel
	-0.07, 0.0775, 1.029, 0.0,  //blue channel
	0.0,  0.0,  0.0,  0.935   //alpha channel
); 

const mat4 SP1_DCI = mat4(
	0.805, 0.0675, 0.017, 0.0,  //red channel
	0.24, 0.86, 0.02, 0.0,  //green channel
	-0.045, 0.0725, 0.963, 0.0,  //blue channel
	0.0,   0.0,   0.0,   0.955  //alpha channel
); 

const mat4 SP1_Rec2020 = mat4(
	0.625, 0.10, 0.015, 0.0,  //red channel
	0.35, 0.82, 0.0325, 0.0,  //green channel
	0.025, 0.08, 0.9525, 0.0,  //blue channel
	0.0,   0.0,   0.0,   1.0  //alpha channel
); 

void main() {
	if (color_mode == 1) profile = SP1_sRGB;
	else if (color_mode == 2) profile = SP1_DCI;
	else if (color_mode == 3) profile = SP1_Rec2020;

	gl_Position = position;
	texCoord = (position.st + vec2(1.0, 1.0)) * vec2(0.5, 0.5);
}
