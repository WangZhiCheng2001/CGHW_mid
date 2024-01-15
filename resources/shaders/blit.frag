#version 460

layout(location = 0) in vec2 texCoords;
layout(binding = 3, rgba8) uniform coherent image2D colorBuffer; 

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = imageLoad(colorBuffer, ivec2(texCoords * imageSize(colorBuffer)));
}