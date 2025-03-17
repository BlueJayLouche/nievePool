precision highp float;

varying vec2 texCoordVarying;

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
    // Calculate optimized sample offsets
    float X = 0.003125; // Optimized value for sampling
    float Y = 0.004166; // Optimized value for sampling
    
    // Calculate brightness from surrounding pixels (optimized sampling pattern)
    float colorSharpenBright = 
        rgb2hsb(texture2D(tex0, texCoordVarying + vec2(X, Y)).rgb).z +
        rgb2hsb(texture2D(tex0, texCoordVarying + vec2(-X, Y)).rgb).z +
        rgb2hsb(texture2D(tex0, texCoordVarying + vec2(-X, -Y)).rgb).z +
        rgb2hsb(texture2D(tex0, texCoordVarying + vec2(X, -Y)).rgb).z;
    
    // Use 0.25 instead of 0.125 since we're sampling 4 pixels not 8
    colorSharpenBright *= 0.25;
    
    // Sample original color
    vec4 ogColor = texture2D(tex0, texCoordVarying);
    vec3 ogColorHSB = rgb2hsb(ogColor.rgb);
    
    // Get brightness for video reactive effects
    float VVV = ogColorHSB.z;
    
    // Apply sharpening effect
    float sharpEffect = sharpenAmount * colorSharpenBright + (colorSharpenBright * vSharpenAmount * VVV);
    ogColorHSB.z -= sharpEffect;
    
    // Boost saturation and brightness if sharpening is applied
    if(sharpenAmount > 0.0) {
        // Pre-calculate boost factors for better performance
        float brightBoost = 1.0 + sharpenAmount * 0.45 + 0.45 * (vSharpenAmount * VVV);
        float satBoost = 1.0 + sharpenAmount * 0.25 + 0.25 * (vSharpenAmount * VVV);
        
        ogColorHSB.z *= brightBoost;
        ogColorHSB.y *= satBoost;
    }
    
    // Convert back to RGB and set output
    vec4 outColor;
    outColor.rgb = hsb2rgb(ogColorHSB);
    outColor.a = 1.0;
    
    gl_FragColor = outColor;
}