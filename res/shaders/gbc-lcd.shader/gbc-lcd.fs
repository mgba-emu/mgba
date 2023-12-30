/**
 * This shader imitates the GameBoy Color subpixel
 * arrangement.
 */

varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;

/**
 * Adds a base color to everything.
 * Lower values make black colors darker.
 * Higher values make black colors lighter.
 * You'll normally want each of these numbers to be close
 * to 0, and not normally higher than 1.
 */
uniform vec3 BaseColor;

/**
 * Modifies the contrast or saturation of the image.
 * Lower values make the image more gray and higher values
 * make it more colorful.
 * A value of 1 represents a normal, baseline level of
 * contrast.
 * You'll normally want this to be somewhere around 1.
 */
uniform float SourceContrast;

/**
 * Modifies the luminosity of the image.
 * Lower values make the image darker and higher values make
 * it lighter.
 * A value of 1 represents normal, baseline luminosity.
 * You'll normally want this to be somewhere around 1.
 */
uniform float SourceLuminosity;

/**
 * Lower values look more like a sharp, unshaded image.
 * Higher values look more like an LCD display with subpixels.
 * You'll normally want this to be a number from 0 to 1.
 */
uniform float SubpixelBlendAmount;

/**
 * Lower values make subpixels darker.
 * Higher values make them lighter and over-bright.
 * A value of 1 represents a normal, baseline gamma value.
 * You'll normally want this to be somewhere around 1.
 */
uniform float SubpixelGamma;

/**
 * Higher values allow subpixels to be more blended-together
 * and brighter.
 * Lower values keep subpixel colors more separated.
 * You'll normally want this to be a number from 0 to 1.
 */
uniform float SubpixelColorBleed;

// Subpixel colors are loosely based on this resource:
// R: #FF7145, G: #C1D650, B: #3BCEFF
// https://gbcc.dev/technology/
const vec3 SubpixelColorRed = vec3(1.00, 0.38, 0.22);
const vec3 SubpixelColorGreen = vec3(0.60, 0.88, 0.30);
const vec3 SubpixelColorBlue = vec3(0.23, 0.65, 1.00);

// These values determine the size and fade-off of subpixels.
const float SubpixelGlowWidthMin = 0.08;
const float SubpixelGlowWidthMax = 0.36;
const float SubpixelGlowHeightMin = 0.30;
const float SubpixelGlowHeightMax = 0.65;
const float SubpixelGlowWidthDelta = (
    SubpixelGlowWidthMax - SubpixelGlowWidthMin
);
const float SubpixelGlowHeightDelta = (
    SubpixelGlowHeightMax - SubpixelGlowHeightMin
);

// Helper to get luminosity of an RGB color.
float GetColorLumosity(in vec3 rgb) {
    return (
        (rgb.r * (5.0 / 16.0)) +
        (rgb.g * (9.0 / 16.0)) +
        (rgb.b * (2.0 / 16.0))
    );
}

// Helper to convert RGB color to HCL. (Hue, Chroma, Luma)
vec3 ConvertRgbToHcl(in vec3 rgb) {
    float xMin = min(rgb.r, min(rgb.g, rgb.b));
    float xMax = max(rgb.r, max(rgb.g, rgb.b));
    float c = xMax - xMin;
    float l = GetColorLumosity(rgb);
    float h = mod((
        c == 0 ? 0.0 :
        xMax == rgb.r ? ((rgb.g - rgb.b) / c) :
        xMax == rgb.g ? ((rgb.b - rgb.r) / c) + 2.0 :
        xMax == rgb.b ? ((rgb.r - rgb.g) / c) + 4.0 :
        0.0
    ), 6.0);
    return vec3(h, c, l);
}

// Helper to convert HCL color to RGB. (Hue, Chroma, Luma)
vec3 ConvertHclToRgb(in vec3 hcl) {
    vec3 rgb;
    float h = mod(hcl.x, 6.0);
    float c = hcl.y;
    float l = hcl.z;
    float x = c * (1.0 - abs(mod(h, 2.0) - 1.0));
    if(h <= 1.0) {
        rgb = vec3(c, x, 0.0);
    }
    else if(h <= 2.0) {
        rgb = vec3(x, c, 0.0);
    }
    else if(h <= 3.0) {
        rgb = vec3(0.0, c, x);
    }
    else if(h <= 4.0) {
        rgb = vec3(0.0, x, c);
    }
    else if(h <= 5.0) {
        rgb = vec3(x, 0.0, c);
    }
    else {
        rgb = vec3(c, 0.0, x);
    }
    float lRgb = GetColorLumosity(rgb);
    float m = l - lRgb;
    return clamp(vec3(m, m, m) + rgb, 0.0, 1.0);
}

void main() {
    // Get base color of the pixel, adjust based on contrast
    // and luminosity settings
    vec3 colorSource = texture2D(tex, texCoord).rgb;
    vec3 colorSourceHcl = ConvertRgbToHcl(colorSource);
    vec3 colorSourceAdjusted = ConvertHclToRgb(vec3(
        colorSourceHcl.x,
        colorSourceHcl.y * SourceContrast,
        colorSourceHcl.z * SourceLuminosity
    ));
    // Determine how much each subpixel color should affect
    // this fragment
    vec2 subpixelPos = mod(texCoord * texSize, 1.0);
    float distRed = abs(subpixelPos.x - (1.35 / 8.0));
    float distGreen = abs(subpixelPos.x - (4.0 / 8.0));
    float distBlue = abs(subpixelPos.x - (6.65 / 8.0));
    float mixRed = (
        max(0.0, (
            SubpixelGlowWidthDelta -
            max(0.0, distRed - SubpixelGlowWidthMin)
        )) /
        SubpixelGlowWidthDelta
    );
    float mixGreen = (
        max(0.0, (
            SubpixelGlowWidthDelta -
            max(0.0, distGreen - SubpixelGlowWidthMin)
        )) /
        SubpixelGlowWidthDelta
    );
    float mixBlue = (
        max(0.0, (
            SubpixelGlowWidthDelta -
            max(0.0, distBlue - SubpixelGlowWidthMin)
        )) /
        SubpixelGlowWidthDelta
    );
    float distVertical = abs(subpixelPos.y - 0.55);
    float mixVertical = (
        max(0.0, (
            SubpixelGlowHeightDelta -
            max(0.0, distVertical - SubpixelGlowHeightMin)
        )) /
        SubpixelGlowHeightDelta
    );
    mixRed *= (
        SubpixelColorBleed +
        ((1.0 - SubpixelColorBleed) * colorSourceAdjusted.r)
    );
    mixGreen *= (
        SubpixelColorBleed +
        ((1.0 - SubpixelColorBleed) * colorSourceAdjusted.g)
    );
    mixBlue *= (
        SubpixelColorBleed +
        ((1.0 - SubpixelColorBleed) * colorSourceAdjusted.b)
    );
    vec3 subpixelLightColor = SubpixelGamma * mixVertical * (
        (mixRed * SubpixelColorRed) +
        (mixGreen * SubpixelColorGreen) +
        (mixBlue * SubpixelColorBlue)
    );
    // Compute final color
    vec3 colorResult = clamp(
        subpixelLightColor * colorSourceAdjusted, 0.0, 1.0
    );
    vec3 colorResultBlended = (
        ((1.0 - SubpixelBlendAmount) * colorSourceAdjusted) +
        (SubpixelBlendAmount * colorResult)
    );
    colorResultBlended = BaseColor + (
        (colorResultBlended * (vec3(1.0, 1.0, 1.0) - BaseColor))
    );
    gl_FragColor = vec4(
        colorResultBlended,
        1.0
    );
}
