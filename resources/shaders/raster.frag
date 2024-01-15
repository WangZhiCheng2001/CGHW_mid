#version 460

layout(push_constant) uniform PushConstants 
{
    mat4 matrixVP; 
    vec3 lightDirection;
    uint mipLevelCount;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(1);
}