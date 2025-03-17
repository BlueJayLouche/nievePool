OF_GLSL_SHADER_HEADER

// Input varying
in vec2 texCoordVarying;

// Output color
out vec4 outputColor;

uniform sampler2D tex0;
uniform float sharpenAmount;
uniform float vSharpenAmount;

//-------------------------
vec3 rgb2hsb(in vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

//---------------------------
vec3 hsb2rgb(in vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

//-------------------------
void main() {
    // Calculate sample offsets
    float X = 0.003125; // 2.0 * 0.0015625 simplified
    float Y = 0.004166; // 2.0 * 0.0020833 simplified
    
    // Calculate brightness from surrounding pixels
    float colorSharpenBright = 
        rgb2hsb(texture(tex0, texCoordVarying + vec2(X, Y)).rgb).z +
        rgb2hsb(texture(tex0, texCoordVarying + vec2(-X, Y)).rgb).z +
        rgb2hsb(texture(tex0, texCoordVarying + vec2(-X, -Y)).rgb).z +
        rgb2hsb(texture(tex0, texCoordVarying + vec2(X, -Y)).rgb).z;
    
    // Average the brightness
    colorSharpenBright *= 0.25;
    
    // Sample original pixel color
    vec4 ogColor = texture(tex0, texCoordVarying);
    vec3 ogColorHSB = rgb2hsb(ogColor.rgb);
    
    // Get the brightness for video reactive effects
    float VVV = ogColorHSB.z;
    
    // Calculate sharpening effect
    float sharpEffect = sharpenAmount + (vSharpenAmount * VVV);
    ogColorHSB.z -= sharpEffect * colorSharpenBright;
    
    // Apply brightness and saturation boost if sharpening
    if(sharpenAmount > 0.0) {
        float boostFactor = 1.0 + sharpenAmount * 0.45 + 0.45 * (vSharpenAmount * VVV);
        ogColorHSB.z *= boostFactor;
        ogColorHSB.y *= 1.0 + sharpenAmount * 0.25 + 0.25 * (vSharpenAmount * VVV);
    }
    
    // Convert back to RGB and set output
    vec4 outColor;
    outColor.rgb = hsb2rgb(ogColorHSB);
    outColor.a = 1.0;
    
    outputColor = outColor;
}