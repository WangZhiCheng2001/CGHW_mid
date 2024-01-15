#version 460

#extension GL_ARB_fragment_shader_interlock : enable
layout(early_fragment_tests) in;
layout(pixel_interlock_ordered) in;

layout(set = 0, binding = 0, r32f) uniform coherent image2D ZBuffer[11];

void main()
{
    ivec2 positionScreen = ivec2(gl_FragCoord.xy);

    // linearize depth
    float z = gl_FragCoord.z * 2 - 1;
    float linearDepth = (2 * .01f) / (1000.f + .01f - z * (1000.f - .01f)); 

    beginInvocationInterlockARB();

    float depth = imageLoad(ZBuffer[0], positionScreen).x;
    if(linearDepth < depth)
    {
        imageStore(ZBuffer[0], positionScreen, vec4(linearDepth, .0f, .0f, .0f));
        // use memory barrier to guarantee RW sync
        memoryBarrier();
    }
    else discard;

    endInvocationInterlockARB();
}