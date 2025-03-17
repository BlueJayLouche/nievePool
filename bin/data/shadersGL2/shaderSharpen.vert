OF_GLSL_SHADER_HEADER

// Uniform matrices
uniform mat4 modelViewProjectionMatrix;

// Input attributes
attribute vec4 position;
attribute vec2 texcoord;

// Output varying
varying vec2 texCoordVarying;

void main()
{
    texCoordVarying = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}