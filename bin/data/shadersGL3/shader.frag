OF_GLSL_SHADER_HEADER

// Input varying
in vec2 texCoordVarying;

// Output color
out vec4 outputColor;

uniform vec4 mouseColor;

void main()
{
    // Output the mouse color
    outputColor = mouseColor;
}