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

/**
 * Determines the distance between subpixels.
 * Lower values put the red, green, and blue subpixels
 * within a single pixel closer together.
 * Higher values put them farther apart.
 * You'll normally want this to be a number from 0 to 1.
 */
uniform float SubpixelSpread;

/**
 * Determines the vertical offset of subpixels within
 * a pixel.
 * Lower values put the red, green, and blue subpixels
 * within a single pixel higher up.
 * Higher values put them further down.
 * You'll normally want this to be a number from 0 to 1.
 */
uniform float SubpixelVerticalOffset;

/**
 * Lower values make the subpixels horizontally thinner,
 * and higher values make them thicker.
 * You'll normally want this to be a number from 0 to 1.
 */
uniform float SubpixelLightWidth;

/**
 * Lower values make the subpixels vertically taller,
 * and higher values make them shorter.
 * You'll normally want this to be a number from 0 to 1.
 */
uniform float SubpixelLightHeight;

/**
 * Lower values make the subpixels sharper and more
 * individually distinct.
 * Higher values add an increasingly intense glowing
 * effect around each subpixel.
 * You'll normally want this to be a number from 0 to 1.
 */
uniform float SubpixelLightGlow;

/**
 * Scale the size of pixels up or down.
 * Useful for looking at larger than 8x8 subpixel sizes.
 * You'll normally want this number to be exactly 1,
 * meaning that every group of 3 subpixels corresponds
 * to one pixel in the display.
 */
uniform float SubpixelScale;

/**
 * GBC subpixels are roughly rectangular shaped, but
 * with a rectangular gap in the lower-right corner.
 * Lower values make the lower-right gap in each GBC
 * subpixel less distinct. A value of 0 results in no
 * gap being shown at all.
 * Higher values make the gap more distinct.
 * You'll normally want this to be a number from 0 to 1.
 */
uniform float SubpixelTabHeight;

/**
 * The following three uniforms decide the base colors
 * of each of the subpixels.
 * 
 * Default subpixel colors are based on this resource:
 * https://gbcc.dev/technology/
 * R: #FF7145 (1.00, 0.44, 0.27)
 * G: #C1D650 (0.75, 0.84, 0.31)
 * B: #3BCEFF (0.23, 0.81, 1.00)
 */ 
uniform vec3 SubpixelColorRed; // vec3(1.00, 0.38, 0.22);
uniform vec3 SubpixelColorGreen; // vec3(0.60, 0.88, 0.30);
uniform vec3 SubpixelColorBlue; // vec3(0.23, 0.65, 1.00);

/**
 * Helper to get luminosity of an RGB color.
 * Used with HCL color space related code.
 */
float getColorLumosity(in vec3 rgb) {
    return (
        (rgb.r * (5.0 / 16.0)) +
        (rgb.g * (9.0 / 16.0)) +
        (rgb.b * (2.0 / 16.0))
    );
}

/**
 * Helper to convert RGB color to HCL. (Hue, Chroma, Luma)
 */
vec3 convertRgbToHcl(in vec3 rgb) {
    float xMin = min(rgb.r, min(rgb.g, rgb.b));
    float xMax = max(rgb.r, max(rgb.g, rgb.b));
    float c = xMax - xMin;
    float l = getColorLumosity(rgb);
    float h = mod((
        c == 0 ? 0.0 :
        xMax == rgb.r ? ((rgb.g - rgb.b) / c) :
        xMax == rgb.g ? ((rgb.b - rgb.r) / c) + 2.0 :
        xMax == rgb.b ? ((rgb.r - rgb.g) / c) + 4.0 :
        0.0
    ), 6.0);
    return vec3(h, c, l);
}

/**
 * Helper to convert HCL color to RGB. (Hue, Chroma, Luma)
 */
vec3 convertHclToRgb(in vec3 hcl) {
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
    float lRgb = getColorLumosity(rgb);
    float m = l - lRgb;
    return clamp(vec3(m, m, m) + rgb, 0.0, 1.0);
}

/**
 * Helper to check if a point is contained within
 * a rectangular area.
 */
bool getPointInRect(
    vec2 point,
    vec2 rectTopLeft,
    vec2 rectBottomRight
) {
    return (
        point.x >= rectTopLeft.x &&
        point.y >= rectTopLeft.y &&
        point.x <= rectBottomRight.x &&
        point.y <= rectBottomRight.y
    );
}

/**
 * Helper to get the nearest offset vector from a
 * point to a line segment.
 * (The length of this offset vector is the nearest
 * distance from the point to the line segment.)
 * Thank you to https://stackoverflow.com/a/1501725
 */
vec2 getPointLineDistance(
    vec2 point,
    vec2 line0,
    vec2 line1
) {
    vec2 lineDelta = line0 - line1;
    float lineLengthSq = dot(lineDelta, lineDelta);
    if(lineLengthSq <= 0) {
        return line0 - point;
    }
    float t = (
        dot(point - line0, line1 - line0) / lineLengthSq
    );
    vec2 projection = (
        line0 + clamp(t, 0.0, 1.0) * (line1 - line0)
    );
    return projection - point;
}

/**
 * Helper to get the nearest offset vector from a
 * point to a rectangle.
 * Returns (0, 0) for points within the rectangle.
 */
vec2 getPointRectDistance(
    vec2 point,
    vec2 rectTopLeft,
    vec2 rectBottomRight
) {
    if(getPointInRect(point, rectTopLeft, rectBottomRight)) {
        return vec2(0.0, 0.0);
    }
    vec2 rectTopRight = vec2(rectBottomRight.x, rectTopLeft.y);
    vec2 rectBottomLeft = vec2(rectTopLeft.x, rectBottomRight.y);
    vec2 v0 = getPointLineDistance(point, rectTopLeft, rectTopRight);
    vec2 v1 = getPointLineDistance(point, rectBottomLeft, rectBottomRight);
    vec2 v2 = getPointLineDistance(point, rectTopLeft, rectBottomLeft);
    vec2 v3 = getPointLineDistance(point, rectTopRight, rectBottomRight);
    float v0LengthSq = dot(v0, v0);
    float v1LengthSq = dot(v1, v1);
    float v2LengthSq = dot(v2, v2);
    float v3LengthSq = dot(v3, v3);
    float minLengthSq = min(
        min(v0LengthSq, v1LengthSq),
        min(v2LengthSq, v3LengthSq)
    );
    if(minLengthSq == v0LengthSq) {
        return v0;
    }
    else if(minLengthSq == v1LengthSq) {
        return v1;
    }
    else if(minLengthSq == v2LengthSq) {
        return v2;
    }
    else {
        return v3;
    }
}

/**
 * Helper to get the nearest offset vector from a
 * point to a subpixel.
 * GBC subpixels are roughly rectangular in shape,
 * but have a rectangular gap in their bottom-left
 * corner.
 * Returns (0, 0) for points within the subpixel.
 */
vec2 getPointSubpixelDistance(
    vec2 point,
    vec2 subpixelCenter,
    vec2 subpixelSizeHalf
) {
    float rectLeft = subpixelCenter.x - subpixelSizeHalf.x;
    float rectRight = subpixelCenter.x + subpixelSizeHalf.x;
    float rectTop = subpixelCenter.y - subpixelSizeHalf.y;
    float rectBottom = subpixelCenter.y + subpixelSizeHalf.y;
    vec2 offsetLeft = getPointRectDistance(
        point,
        vec2(rectLeft, rectTop + SubpixelTabHeight),
        vec2(subpixelCenter.x, rectBottom)
    );
    vec2 offsetRight = getPointRectDistance(
        point,
        vec2(subpixelCenter.x, rectTop),
        vec2(rectRight, rectBottom)
    );
    float offsetLeftLengthSq = dot(offsetLeft, offsetLeft);
    float offsetRightLengthSq = dot(offsetRight, offsetRight);
    if(offsetLeftLengthSq <= offsetRightLengthSq) {
        return offsetLeft;
    }
    else {
        return offsetRight;
    }
}

/**
 * Helper to get the intensity of light from a
 * subpixel.
 * The pixelPosition argument represents a
 * fragment's position within a pixel.
 * Spread represents the subpixel's horizontal
 * position within the pixel.
 */
float getSubpixelIntensity(
    vec2 pixelPosition,
    float spread
) {
    vec2 subpixelCenter = vec2(
        0.5 + (spread * SubpixelSpread),
        1.0 - SubpixelVerticalOffset
    );
    vec2 subpixelSizeHalf = 0.5 * vec2(
        SubpixelLightWidth,
        SubpixelLightHeight
    );
    vec2 offset = getPointSubpixelDistance(
        pixelPosition,
        subpixelCenter,
        subpixelSizeHalf
    );
    if(SubpixelLightGlow <= 0) {
        return dot(offset, offset) <= 0.0 ? 1.0 : 0.0;
    }
    else {
        float dist = length(offset);
        float glow = max(0.0,
            1.0 - (dist / SubpixelLightGlow)
        );
        return glow;
    }
}

/**
 * Helper to apply SubpixelColorBleed to the intensity
 * value computed for a fragment and subpixel.
 * Subpixel color bleed allows subpixel colors to be
 * more strongly coerced to more accurately represent
 * the underlying pixel color.
 */
float applySubpixelBleed(
    float subpixelIntensity,
    float colorSourceChannel
) {
    return subpixelIntensity * (
        SubpixelColorBleed +
        ((1.0 - SubpixelColorBleed) * colorSourceChannel)
    );
}

void main() {
    // Get base color of the pixel, adjust based on
    // contrast and luminosity settings.
    vec3 colorSource = texture2D(tex, texCoord).rgb;
    vec3 colorSourceHcl = convertRgbToHcl(colorSource);
    vec3 colorSourceAdjusted = convertHclToRgb(vec3(
        colorSourceHcl.x,
        colorSourceHcl.y * SourceContrast,
        colorSourceHcl.z * SourceLuminosity
    ));
    // Determine how much each subpixel's light should
    // affect this fragment.
    vec2 pixelPosition = (
        mod(texCoord * texSize * SubpixelScale, 1.0)
    );
    float subpixelIntensityRed = applySubpixelBleed(
        getSubpixelIntensity(pixelPosition, -1.0),
        colorSourceAdjusted.r
    );
    float subpixelIntensityGreen = applySubpixelBleed(
        getSubpixelIntensity(pixelPosition, +0.0),
        colorSourceAdjusted.g
    );
    float subpixelIntensityBlue = applySubpixelBleed(
        getSubpixelIntensity(pixelPosition, +1.0),
        colorSourceAdjusted.b
    );
    vec3 subpixelLightColor = SubpixelGamma * (
        (subpixelIntensityRed * SubpixelColorRed) +
        (subpixelIntensityGreen * SubpixelColorGreen) +
        (subpixelIntensityBlue * SubpixelColorBlue)
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
