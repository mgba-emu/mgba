/**
 * This shader creates a backlight bleeding effect,
 * and an internal reflection or ghosting effect.
 */
 
varying vec2 texCoord;
uniform sampler2D tex;

/**
 * Determines the color of the backlight bleed.
 * Lower values produce less, dimmer light.
 * Higher values produce brighter or more colorful light.
 * You'll normally want each of these numbers to be close
 * to 1, and not normally lower than 0.
 */
uniform vec3 LightColor;

/**
 * Affects the shape of the backlight bleed glow.
 * Lower values cause the light bleed to fade out quickly
 * from the edges.
 * Higher values cause the light bleed to fade out more
 * softly and gradually toward the center.
 * You'll normally want this to be a number from 0 to 1.
 */
uniform float LightSoftness;

/**
 * Lower values result in a less visible or intense
 * backlight bleed.
 * Higher values make the backlight bleed more pronounced.
 * You'll normally want this to be a number close to 0,
 * and not normally higher than 1.
 */
uniform float LightIntensity;

/**
 * Lower values cause the internal reflection or ghosting
 * effect to be less visible.
 * Higher values cause the effect to be brighter and more
 * visible.
 * You'll normally want this to be a number close to 0,
 * and not normally higher than 1.
 */
uniform float ReflectionBrightness;

/**
 * Lower values have the internal reflection or ghosting
 * effect appear offset by a lesser distance.
 * Higher values have the effect offset by a greater
 * distance.
 * You'll normally want each of these numbers to be close
 * to 0, and not normally higher than 1.
 */
uniform vec2 ReflectionDistance;

#define M_PI 3.1415926535897932384626433832795

/**
 * Helper to compute backlight bleed intensity
 * for a texCoord input.
 */
float getLightIntensity(vec2 coord) {
    vec2 coordCentered = coord - vec2(0.5, 0.5);
    float coordDistCenter = (
        length(coordCentered) / sqrt(0.5)
    );
    vec2 coordQuadrant = vec2(
        1.0 - (1.5 * min(coord.x, 1.0 - coord.x)),
        1.0 - (1.5 * min(coord.y, 1.0 - coord.y))
    );
    float lightIntensityEdges = (
        pow(coordQuadrant.x, 5.0) +
        pow(coordQuadrant.y, 5.0)
    );
    float lightIntensity = (
        (1.0 - LightSoftness) * lightIntensityEdges +
        LightSoftness * coordDistCenter
    );
    return clamp(lightIntensity, 0.0, 1.0);
}

/**
 * Helper to convert an intensity value into a white
 * gray color with that intensity. A radial distortion
 * effect with subtle chromatic abberation is applied that
 * makes it look a little more like a real old or cheap
 * backlight, and also helps to reduce color banding.
 */
vec3 getWhiteVector(float intensity) {
    const float DeformAmount = 0.0025;
    vec2 texCoordCentered = texCoord - vec2(0.5, 0.5);
    float radians = atan(texCoordCentered.y, texCoordCentered.x);
    float rot = pow(2.0, 4.0 + floor(6.0 * length(texCoordCentered)));
    float deformRed = cos(rot * radians + (2.0 / 3.0 * M_PI));
    float deformGreen = cos(rot * radians);
    float deformBlue = cos(rot * radians + (4.0 / 3.0 * M_PI));
    return clamp(vec3(
        intensity + (deformRed * DeformAmount),
        intensity + (deformGreen * DeformAmount),
        intensity + (deformBlue * DeformAmount)
    ), 0.0, 1.0);
}

void main() {
    vec3 colorSource = texture2D(tex, texCoord).rgb;
    vec3 lightWhiteVector = getWhiteVector(getLightIntensity(texCoord));
    vec3 colorLight = LightColor * lightWhiteVector;
    vec3 colorReflection = texture2D(tex, texCoord - ReflectionDistance).rgb;
    vec3 colorResult = (
        colorSource +
        (colorLight * LightIntensity) +
        (colorReflection * ReflectionBrightness)
    );
    gl_FragColor = vec4(
        colorResult,
        1.0
    );
}
