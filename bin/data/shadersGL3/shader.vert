OF_GLSL_SHADER_HEADER

// Input attributes
in vec4 position;
in vec2 texcoord;

// Output varying
out vec2 texCoordVarying;

// Uniform matrices and mouse parameters
uniform mat4 modelViewProjectionMatrix;
uniform float mouseRange;
uniform vec2 mousePos;
uniform vec4 mouseColor;

void main()
{
    // Pass texture coordinates to fragment shader
    texCoordVarying = texcoord;
    
    // Copy position so we can work with it
    vec4 pos = position;
    
    // Direction vector from mouse position to vertex position
    vec2 dir = pos.xy - mousePos;
    
    // Distance between the mouse position and vertex position (optimized)
    float dist = length(dir);
    
    // Check vertex is within mouse range
    if(dist > 0.0 && dist < mouseRange) {
        // Normalize distance between 0 and 1
        float distNorm = 1.0 - dist / mouseRange;
        
        // Make the direction vector magnitude fade out with distance
        dir *= distNorm;
        
        // Add the direction vector to the vertex position
        pos.xy += dir;
    }
    
    // Set the final position
    gl_Position = modelViewProjectionMatrix * pos;
}