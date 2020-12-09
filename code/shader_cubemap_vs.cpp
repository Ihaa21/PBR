#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_viewport_layer_array : enable

layout(set = 0, binding = 0) uniform input_buffer
{
    mat4 WVPTransform;
    int LayerId;
    uint Pad[3];
} InputBuffer;

#if VERTEX_SHADER

layout(location = 0) in vec3 InPos;

layout(location = 0) out vec3 OutLocalPos;

void main()
{
    OutLocalPos = InPos;
    gl_Position = InputBuffer.WVPTransform * vec4(InPos, 1);
    gl_Layer = InputBuffer.LayerId;
}

#endif
