uniform int color_mode;
attribute vec4 position;
varying vec2 texCoord;
varying mat4 profile;

const mat4 GBC_sRGB = mat4(
	0.905, 0.10, 0.1575, 0.0,  //red channel
	0.195, 0.65, 0.1425, 0.0,  //green channel
	-0.10, 0.25, 0.70, 0.0,  //blue channel
	0.0,  0.0,  0.0,  0.91   //alpha channel
); 

const mat4 GBC_DCI = mat4(
	0.76, 0.125, 0.16, 0.0,  //red channel
	0.27, 0.6375, 0.18, 0.0,  //green channel
	-0.03, 0.2375, 0.66, 0.0,  //blue channel
	0.0,  0.0,  0.0,  0.97  //alpha channel
); 

const mat4 GBC_Rec2020 = mat4(
	0.61, 0.155, 0.16, 0.0,  //red channel
	0.345, 0.615, 0.1875, 0.0,  //green channel
	0.045, 0.23, 0.6525, 0.0,  //blue channel
	0.0,  0.0,  0.0,   1.0  //alpha channel
);

void main() {
	if (color_mode == 1) profile = GBC_sRGB;
	else if (color_mode == 2) profile = GBC_DCI;
	else if (color_mode == 3) profile = GBC_Rec2020;

	gl_Position = position;
	texCoord = (position.st + vec2(1.0, 1.0)) * vec2(0.5, 0.5);
}
