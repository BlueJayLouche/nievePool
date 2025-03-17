OF_GLSL_SHADER_HEADER

uniform float mouseRange;
uniform vec2 mousePos;
uniform vec4 mouseColor;

// Add model matrix uniform for flexibility
uniform mat4 modelViewProjectionMatrix;
attribute vec4 position;
attribute vec2 texcoord;

varying vec2 texCoordVarying;

void main()
{
    // Pass texture coordinates to fragment shader
    texCoordVarying = texcoord;
    
    // Copy position so we can work with it
    vec4 pos = position;
    
    // Direction vector from mouse position to vertex position
    vec2 dir = pos.xy - mousePos;
    
    // Distance between the mouse position and vertex position
    float dist = length(dir);
    
    // Check vertex is within mouse range
    if(dist > 0.0 && dist < mouseRange) {
        // Normalize distance between 0 and 1
        float distNorm = dist / mouseRange;
        
        // Flip it so the closer we are the greater the repulsion
        distNorm = 1.0 - distNorm;
        
        // Make the direction vector magnitude fade out the further it gets from mouse position
        dir *= distNorm;
        
        // Add the direction vector to the vertex position
        pos.xy += dir;
    }
    
    // Use the model view projection matrix for proper rendering
    gl_Position = modelViewProjectionMatrix * pos;
}