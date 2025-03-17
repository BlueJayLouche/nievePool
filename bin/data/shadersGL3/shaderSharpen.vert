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
    texCoordVarying = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}