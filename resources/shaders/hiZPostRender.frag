#version 460

#extension GL_ARB_fragment_shader_interlock : enable
layout(early_fragment_tests) in;
layout(pixel_interlock_ordered) in;

layout(set = 1, binding = 0, r32f) uniform coherent image2D ZBuffer[11];
layout(set = 2, binding = 0) restrict readonly buffer VertexAttributes { vec4 pos[]; };
layout(set = 2, binding = 2, r32f) uniform coherent image2D tempZBuffer;

layout(push_constant) uniform PushConstants 
{
    mat4 matrixVP; 
    vec3 lightDirection;
    uint mipLevelCount;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    ivec2 positionScreen = ivec2(gl_FragCoord.xy);

    // linearize depth
    float z = gl_FragCoord.z * 2 - 1;
    float linearDepth = (2 * .01f) / (1000.f + .01f - z * (1000.f - .01f)); 

    beginInvocationInterlockARB();

    float depth = imageLoad(tempZBuffer, positionScreen).x;
    if(linearDepth < depth)
    {
        imageStore(tempZBuffer, positionScreen, vec4(linearDepth, .0f, .0f, .0f));
        // use memory barrier to guarantee RW sync
        memoryBarrier();
    }
    // i do not want to use discard() to throw fragment(since this may cause low efficiency)
    // but due to test, this is the only way we can get correct result...
    else discard;

    endInvocationInterlockARB();

    const vec3 v0 = pos[3 * gl_PrimitiveID].xyz;
    const vec3 v1 = pos[3 * gl_PrimitiveID + 1].xyz;
    const vec3 v2 = pos[3 * gl_PrimitiveID + 2].xyz;
    const vec3 N = normalize(cross(v1 - v0, v2 - v1));
    fragColor = vec4(dot(N, lightDirection));
}