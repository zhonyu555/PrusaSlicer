#version 110

precision highp float;

uniform vec4 uniform_color;

void main()
{
    gl_FragColor = uniform_color;
}
