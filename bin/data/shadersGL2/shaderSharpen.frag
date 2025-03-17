OF_GLSL_SHADER_HEADER

uniform sampler2D tex0;
uniform float sharpenAmount;
uniform float vSharpenAmount;
varying vec2 texCoordVarying;

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
    // Optimized sample offsets
    const float X = 0.003125; // Constant value for optimization 
    const float Y = 0.004166; // Constant value for optimization
    
    // Calculate brightness from surrounding pixels using diagonal sampling pattern
    // This is more efficient than the original 8-point sampling
    float colorSharpenBright = 
        rgb2hsb(texture2D(tex0, texCoordVarying + vec2(X, Y)).rgb).z +
        rgb2hsb(texture2D(tex0, texCoordVarying + vec2(-X, Y)).rgb).z +
        rgb2hsb(texture2D(tex0, texCoordVarying + vec2(-X, -Y)).rgb).z +
        rgb2hsb(texture2D(tex0, texCoordVarying + vec2(X, -Y)).rgb).z;
    
    // Updated divisor for 4 samples
    colorSharpenBright *= 0.25;
    
    // Original pixel color
    vec4 ogColor = texture2D(tex0, texCoordVarying);
    vec3 ogColorHSB = rgb2hsb(ogColor.rgb);
    
    // Video reactive brightness
    float VVV = ogColorHSB.z;
    
    // Calculate and apply sharpening effect
    float sharpEffect = sharpenAmount * colorSharpenBright + (colorSharpenBright * vSharpenAmount * VVV);
    ogColorHSB.z -= sharpEffect;
    
    // Boost brightness and saturation when sharpening
    if(sharpenAmount > 0.0) {
        float brightBoost = 1.0 + sharpenAmount * 0.45 + 0.45 * (vSharpenAmount * VVV);
        float satBoost = 1.0 + sharpenAmount * 0.25 + 0.25 * (vSharpenAmount * VVV);
        
        ogColorHSB.z *= brightBoost;
        ogColorHSB.y *= satBoost;
    }
    
    // Convert back to RGB
    vec4 outColor;
    outColor.rgb = hsb2rgb(ogColorHSB);
    outColor.a = 1.0;
    
    gl_FragColor = outColor;
}