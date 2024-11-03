uniform int color_mode;
attribute vec4 position;
varying vec2 texCoord;
varying mat4 profile;

const mat4 GBA_sRGB = mat4(
	0.80, 0.135, 0.195, 0.0,  //red channel
	0.275, 0.64, 0.155, 0.0,  //green channel
	-0.075, 0.225, 0.65, 0.0,  //blue channel
	0.0,  0.0,  0.0,  0.93   //alpha channel
); 

const mat4 GBA_DCI = mat4(
	0.685, 0.16, 0.20, 0.0,  //red channel
	0.34, 0.629, 0.19, 0.0,  //green channel
	-0.025, 0.211, 0.61, 0.0,  //blue channel
	0.0,  0.0,  0.0,  0.975  //alpha channel
); 

const mat4 GBA_Rec2020 = mat4(
	0.555, 0.1825, 0.20, 0.0,  //red channel
	0.395, 0.61, 0.195, 0.0,  //green channel
	0.05, 0.2075, 0.605, 0.0,  //blue channel
	0.0,   0.0,  0.0,   1.0  //alpha channel
); 

void main() {
	if (color_mode == 1) profile = GBA_sRGB;
	else if (color_mode == 2) profile = GBA_DCI;
	else if (color_mode == 3) profile = GBA_Rec2020;

	gl_Position = position;
	texCoord = (position.st + vec2(1.0, 1.0)) * vec2(0.5, 0.5);
}
