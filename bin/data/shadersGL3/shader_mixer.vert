OF_GLSL_SHADER_HEADER

// Input attributes
in vec4 position;
in vec2 texcoord;

// Output varying
out vec2 texCoordVarying;

// Matrices
uniform mat4 modelViewProjectionMatrix;

void main()
{
    // Pass texture coordinates to fragment shader
    texCoordVarying = texcoord;
    
    // Calculate position
    gl_Position = modelViewProjectionMatrix * position;
}