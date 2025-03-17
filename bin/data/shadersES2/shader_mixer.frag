precision highp float;

// Input varying
varying vec2 texCoordVarying;

// Textures
uniform sampler2D tex0;    // External input
uniform sampler2D fb;      // Feedback framebuffer
uniform sampler2D temporalFilter;  // Previous frame

// Continuous controls
uniform float fbMix;
uniform float lumakey;
uniform float temporalFilterMix;
uniform float fbXDisplace;
uniform float fbYDisplace;
uniform float fbZDisplace;
uniform float fbRotate;
uniform float fbHue;
uniform float fbSaturation;
uniform float fbBright;
uniform float fbHuexMod;
uniform float fbHuexOff;
uniform float fbHuexLfo;
uniform float temporalFilterResonance;

// Switches
uniform int brightInvert;
uniform int saturationInvert;
uniform int hueInvert;
uniform int horizontalMirror;
uniform int verticalMirror;
uniform int toroidSwitch;
uniform int lumakeyInvertSwitch;
uniform int mirrorSwitch;

// Video reactive controls
uniform float vLumakey;
uniform float vMix;
uniform float vHue;
uniform float vSat;
uniform float vBright;
uniform float vtemporalFilterMix;
uniform float vFb1X;
uniform float vX;
uniform float vY;
uniform float vZ;
uniform float vRotate;
uniform float vHuexMod;
uniform float vHuexOff;
uniform float vHuexLfo;

//---------------------------------------------------------------
vec2 mirrorCoord(in vec2 inCoord, in vec2 inDim) {
    // Mirror coordinates that are negative
    inCoord = abs(inCoord);
    
    // Apply modulo to keep within bounds
    inCoord.x = mod(inCoord.x, inDim.x * 2.0);
    inCoord.y = mod(inCoord.y, inDim.y * 2.0);
    
    // Mirror coordinates that exceed dimensions
    if(inCoord.x > inDim.x) {
        inCoord.x = inDim.x - mod(inCoord.x, inDim.x);
    }
    if(inCoord.y > inDim.y) {
        inCoord.y = inDim.y - mod(inCoord.y, inDim.y);
    }
    
    return inCoord;
}

//-----------------------------------------------------------------
vec2 rotate(in vec2 coord, in float theta) {
    // Shift coordinates to center (0,0)
    vec2 center_coord = coord - vec2(0.5);
    
    // Perform rotation using 2D rotation matrix
    float cosTheta = cos(theta);
    float sinTheta = sin(theta);
    vec2 rotate_coord;
    rotate_coord.x = center_coord.x * cosTheta - center_coord.y * sinTheta;
    rotate_coord.y = center_coord.x * sinTheta + center_coord.y * cosTheta;
    
    // Shift back to original domain
    return rotate_coord + vec2(0.5);
}

//----------------------------------------------------------------------
vec3 rgb2hsb(in vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

//--------------------------------------------------------------------
vec3 hsb2rgb(in vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

//---------------------------------------------------------------------
void main() {
    // Initialize output color
    vec4 color = vec4(0.0);
    
    // Sample input color and convert to HSB
    vec4 input1Color = texture2D(tex0, texCoordVarying);
    vec3 input1ColorHsb = rgb2hsb(input1Color.rgb);
    
    // Video reactive attenuator
    float VVV = input1ColorHsb.z;
    
    // Sample temporal filter
    vec4 temporalFilterColor = texture2D(temporalFilter, texCoordVarying);
    
    // Center coordinates
    vec2 fbCoord = texCoordVarying - vec2(0.5);
    
    // Apply zoom (optimized calculation)
    float zoomFactor = fbZDisplace * (1.0 + vZ * VVV);
    fbCoord *= zoomFactor;
    
    // Apply mirrors if switched on
    if(horizontalMirror == 1 && fbCoord.x > 0.0) {
        fbCoord.x = -fbCoord.x;
    }
    if(verticalMirror == 1 && fbCoord.y > 0.0) {
        fbCoord.y = -fbCoord.y;
    }
    
    // Apply x and y displacement
    fbCoord += vec2(fbXDisplace + (vX * VVV), fbYDisplace + (vY * VVV)) + vec2(0.5);
    
    // Apply rotation
    fbCoord = rotate(fbCoord, fbRotate + (vRotate * VVV));
    
    // Apply coordinate wrapping (toroid)
    if(toroidSwitch == 1) {
        if(abs(fbCoord.x) > 1.0) {
            fbCoord.x = abs(1.0 - fbCoord.x);
        }
        if(abs(fbCoord.y) > 1.0) {
            fbCoord.y = abs(1.0 - fbCoord.y);
        }
        fbCoord = fract(fbCoord);
    }
    
    // Apply mirror coordinates
    if(mirrorSwitch == 1) {
        fbCoord = mirrorCoord(fbCoord, vec2(1.0));
    }
    
    // Sample feedback buffer
    vec4 fbColor = texture2D(fb, fbCoord);
    
    // Clamp coordinates outside of bounds
    if(toroidSwitch == 0 && mirrorSwitch == 0) {
        if(fbCoord.x > 1.0 || fbCoord.y > 1.0 || fbCoord.x < 0.0 || fbCoord.y < 0.0) {
            fbColor = vec4(0.0);
        }
    }
    
    // Convert feedback color to HSB
    vec3 fbColorHsb = rgb2hsb(fbColor.rgb);
    
    // Apply hue transformations (optimized calculation)
    float hueMod = fbHuexMod + vHuexMod * VVV;
    float hueOffset = fbHuexOff + vHuexOff * VVV;
    float hueEffect = fbHue * (1.0 + vHue * VVV);
    float lfoPart = (fbHuexLfo + vHuexLfo * VVV) * sin(fbColorHsb.x / 3.14);
    
    fbColorHsb.x = abs(fbColorHsb.x * hueEffect + lfoPart);
    fbColorHsb.x = fract(mod(fbColorHsb.x, hueMod) + hueOffset);
    
    // Apply saturation and brightness
    fbColorHsb.y = clamp(fbColorHsb.y * fbSaturation * (1.0 + vSat * VVV), 0.0, 1.0);
    fbColorHsb.z = clamp(fbColorHsb.z * fbBright * (1.0 + vBright * VVV), 0.0, 1.0);
    
    // Apply inversion if enabled
    if(brightInvert == 1) {
        fbColorHsb.z = 1.0 - fbColorHsb.z;
    }
    if(saturationInvert == 1) {
        fbColorHsb.y = 1.0 - fbColorHsb.y;
    }
    if(hueInvert == 1) {
        fbColorHsb.x = fract(1.0 - fbColorHsb.x);
    }
    
    // Convert back to RGBA
    fbColor = vec4(hsb2rgb(fbColorHsb), 1.0);
    
    // Apply temporal filter resonance
    vec3 temporalFilterColorHsb = rgb2hsb(temporalFilterColor.rgb);
    float resonanceEffect = temporalFilterResonance * (1.0 + vFb1X * VVV);
    
    temporalFilterColorHsb.z = clamp(temporalFilterColorHsb.z * (1.0 + 0.5 * resonanceEffect), 0.0, 1.0);
    temporalFilterColorHsb.y = clamp(temporalFilterColorHsb.y * (1.0 + 0.25 * resonanceEffect), 0.0, 1.0);
    temporalFilterColor = vec4(hsb2rgb(temporalFilterColorHsb), 1.0);
    
    // Mix colors
    color = mix(input1Color, fbColor, fbMix + (vMix * VVV));
    
    // Apply luma-based keying
    float lumakeyValue = lumakey + (vLumakey * VVV);
    if(lumakeyInvertSwitch == 0) {
        if(VVV < lumakeyValue) {
            color = fbColor;
        }
    } else {
        if(VVV > lumakeyValue) {
            color = fbColor;
        }
    }
    
    // Add temporal filter into the mix
    color = mix(color, temporalFilterColor, temporalFilterMix + (vtemporalFilterMix * VVV));
    
    // Output final color
    gl_FragColor = color;
}