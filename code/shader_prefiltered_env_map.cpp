#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_viewport_layer_array : enable

layout(set = 1, binding = 0) uniform samplerCube EnvironmentMap;

layout(set = 1, binding = 1) uniform input_buffer
{
    float Roughness;
} InputBuffer;

#include "shader_pbr_lighting.cpp"
#include "shader_random.cpp"

#if FRAGMENT_SHADER

layout(location = 0) in vec3 InLocalPos;

layout(location = 0) out vec4 OutColor;

void main()
{
    vec3 Normal = normalize(InLocalPos);
    vec3 View = Normal;
    
    const uint NumSamples = 1024u;
    float TotalWeight = 0.0f;
    vec3 PreFilteredColor = vec3(0);
    for (uint Index = 0; Index < NumSamples; ++Index)
    {
        vec2 Xi = HammersleySequence(Index, NumSamples);
        vec3 SampleVec = ImportanceSampleGGX(Xi, Normal, InputBuffer.Roughness);
        vec3 LightDir = normalize(2.0 * dot(View, SampleVec) * SampleVec - View);

        float NDotL = max(dot(Normal, LightDir), 0);
        if (NDotL > 0)
        {
            PreFilteredColor += texture(EnvironmentMap, LightDir).rgb * NDotL;
            TotalWeight += NDotL;
        }
    }

    PreFilteredColor /= TotalWeight;
    OutColor = vec4(PreFilteredColor, 1);
}

#endif
