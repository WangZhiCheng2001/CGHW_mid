#version 460

layout(location = 0) in vec4 pos;

layout(push_constant) uniform PushConstants 
{
    mat4 matrixVP; 
    vec3 lightDirection;
    uint mipLevelCount;
};

void main()
{
    gl_Position = matrixVP * pos;
}