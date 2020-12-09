#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "shader_pbr_lighting.cpp"
#include "shader_random.cpp"

#if FRAGMENT_SHADER

layout(location = 0) in vec2 InUv;

layout(location = 0) out vec2 OutIntegratedBrdf;

float GeometrySchlickGGX2(float NDotV, float Roughness)
{
    float R = Roughness;
    float K = Square(R) / 8.0;
    float Result = NDotV / (NDotV * (1.0 - K) + K);
    return Result;
}

float GeometrySmithGGX2(vec3 Normal, vec3 View, vec3 LightDir, float Roughness)
{
    // NOTE: Smiths Schlick GGX
    float NDotV = max(dot(Normal, View), 0);
    float NDotL = max(dot(Normal, LightDir), 0);
    float Result = GeometrySchlickGGX2(NDotV, Roughness) * GeometrySchlickGGX2(NDotL, Roughness);
    return Result;
}

void main()
{
    float NDotV = InUv.x;
    float Roughness = InUv.y;

    vec3 View = vec3(sqrt(1 - Square(NDotV)), 0, NDotV);
    vec2 IntegratedBrdf = vec2(0);

    vec3 Normal = vec3(0, 0, 1);
    const uint NumSamples = 1024u;
    for (uint Index = 0; Index < NumSamples; ++Index)
    {
        vec2 Xi = HammersleySequence(Index, NumSamples);
        vec3 H = ImportanceSampleGGX(Xi, Normal, Roughness);
        vec3 LightDir = normalize(2.0 * dot(View, H) * H - View);

        float NDotL = max(LightDir.z, 0);
        float NDotH = max(H.z, 0);
        float VDotH = max(dot(View, H), 0);

        if (NDotL > 0)
        {
            float G = GeometrySmithGGX2(Normal, View, LightDir, Roughness);
            float G_Vis = (G * VDotH) / (NDotH * NDotV);
            float Fc = pow(1.0 - VDotH, 5.0);

            IntegratedBrdf += vec2((1.0 - Fc), Fc) * G_Vis;
        }
    }

    IntegratedBrdf /= float(NumSamples);
    OutIntegratedBrdf = IntegratedBrdf;
}

#endif
