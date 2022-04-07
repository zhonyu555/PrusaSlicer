#version 100
#extension GL_NV_fragdepth : enable

precision highp float;

uniform vec4 uniform_color;

void main()
{
    gl_FragColor = uniform_color;
}
