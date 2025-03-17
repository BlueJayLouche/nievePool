// Uniform matrices
uniform mat4 modelViewProjectionMatrix;

// Input attributes
attribute vec4 position;
attribute vec2 texcoord;

// Output varying
varying vec2 texCoordVarying;

void main()
{
    // Pass texture coordinates to fragment shader
    texCoordVarying = texcoord;
    
    // Calculate position
    gl_Position = modelViewProjectionMatrix * position;
}