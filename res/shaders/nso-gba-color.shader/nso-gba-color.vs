uniform int color_mode;
attribute vec4 position;
varying vec2 texCoord;
varying mat4 profile;

const mat4 GBA_sRGB = mat4(
	0.865, 0.0575, 0.0575, 0.0,  //red channel
	0.1225, 0.925, 0.1225, 0.0,  //green channel
	0.0125, 0.0125, 0.82, 0.0,  //blue channel
	0.0,  0.0,  0.0,  1.0   //alpha channel
);

const mat4 GBA_DCI = mat4(
	0.72, 0.0875, 0.0725, 0.0,  //red channel
	0.2675, 0.9, 0.185, 0.0,  //green channel
	0.0125, 0.0125, 0.7425, 0.0,  //blue channel
	0.0,  0.0,  0.0,  1.0   //alpha channel
);

const mat4 GBA_Rec2020 = mat4(
	0.57, 0.115, 0.0725, 0.0,  //red channel
	0.3825, 0.8625, 0.195, 0.0,  //green channel
	0.0475, 0.0225, 0.7325, 0.0,  //blue channel
	0.0,  0.0,  0.0,  1.0   //alpha channel
);

void main() {
	if (color_mode == 1) profile = GBA_sRGB;
	else if (color_mode == 2) profile = GBA_DCI;
	else if (color_mode == 3) profile = GBA_Rec2020;

	gl_Position = position;
	texCoord = (position.st + vec2(1.0, 1.0)) * vec2(0.5, 0.5);
}
