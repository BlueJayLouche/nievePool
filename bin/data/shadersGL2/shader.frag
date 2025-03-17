OF_GLSL_SHADER_HEADER

uniform vec4 mouseColor;
varying vec2 texCoordVarying;

void main()
{
    gl_FragColor = mouseColor;
}