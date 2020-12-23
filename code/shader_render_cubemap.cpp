#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(set = 0, binding = 0) uniform input_buffer
{
    mat4 WVPTransform;
} InputBuffer;

layout(set = 0, binding = 1) uniform samplerCube CubeMap;

#if VERTEX_SHADER

layout(location = 0) in vec3 InPos;

layout(location = 0) out vec3 OutUvw;

void main()
{
    vec4 TransformedPos = InputBuffer.WVPTransform * vec4(InPos, 1);
    // NOTE: This sets the z to far z and fails depth test for everything else
    gl_Position = vec4(TransformedPos.xy, 0, TransformedPos.w);
    //gl_Position = TransformedPos.xyww;
    OutUvw = InPos;
}

#endif

#if FRAGMENT_SHADER

layout(location = 0) in vec3 InUvw;

layout(location = 0) out vec4 OutColor;

void main()
{
    OutColor = texture(CubeMap, InUvw);
}

#endif
