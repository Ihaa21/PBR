#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "shader_blinn_phong_lighting.cpp"
#include "shader_pbr_lighting.cpp"
#include "shader_light_types.cpp"

//
// NOTE: Model Helper Functions
//

vec3 NormalMapping(sampler2D NormalTexture, vec2 Uv, vec3 GeometryNormal, vec3 GeometryTangent)
{
    // IMPORTANT: We assume GoemetryNormal and GeometryTanget are normalized

    /*
        NOTE: http://ogldev.atspace.co.uk/www/tutorial26/tutorial26.html

        Normal vectors in the normal map are specified relative to the x,y vectors in texture space which correspond to uv. We have to
        transform the normal to world space and add it to our Geometry Normal. Because we already have a GeometryTangent provided for us,
        we can construct a 3x3 mat that transforms our normal (note 3x3 because normals have no position). 
     */

    // NOTE: Since our vectors are blended and we want them to be perpendicular, we use Granmm-Scmidt process to make tangent perp
    GeometryTangent = normalize(GeometryTangent - dot(GeometryTangent, GeometryNormal)*GeometryNormal);
    vec3 GeometryBiTangent = cross(GeometryTangent, GeometryNormal);
    mat3 Transform = mat3(GeometryTangent, GeometryBiTangent, GeometryNormal);

    // NOTE: Surface normal is stored in [0,1] range in texture, so we expand it to its real range here
    vec3 SurfaceNormal = 2.0*texture(NormalTexture, Uv).xyz - 1.0;

    // NOTE: We transform our surface normal into world space. Since our matrix takes into account the geometry normal, we are essentially
    // adding in the surface normal and geometry normal here while also getting the normal in the right space
    vec3 Result = normalize(Transform * SurfaceNormal);

    return SurfaceNormal;
}

layout(set = 0, binding = 0) uniform sampler2D ColorTexture;
layout(set = 0, binding = 1) uniform sampler2D NormalTexture;
layout(set = 0, binding = 2) uniform sampler2D MetallicTexture;
layout(set = 0, binding = 3) uniform sampler2D RoughnessTexture;
layout(set = 0, binding = 4) uniform sampler2D AoTexture;

// NOTE: Image based lighting
layout(set = 0, binding = 5) uniform samplerCube IrradianceMap;
layout(set = 0, binding = 6) uniform samplerCube PreFilteredEnvMap;
layout(set = 0, binding = 7) uniform sampler2D BrdfLut;

layout(set = 1, binding = 0) uniform camera_buffer
{
    vec3 CameraPos;
} CameraBuffer;

struct instance_entry
{
    mat4 WTransform;
    mat4 WVPTransform;
    float Roughness;
    float Metallic;
};

layout(set = 1, binding = 1) buffer instance_buffer
{
    instance_entry Entries[];
} InstanceBuffer;

#if VERTEX_SHADER

layout(location = 0) in vec3 InPos;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InUv;

layout(location = 0) out vec3 OutWorldPos;
layout(location = 1) out vec3 OutWorldNormal;
layout(location = 2) out vec2 OutUv;
layout(location = 3) out flat uint OutInstanceId;

void main()
{
    instance_entry Entry = InstanceBuffer.Entries[gl_InstanceIndex];
    
    gl_Position = Entry.WVPTransform * vec4(InPos, 1);
    OutWorldPos = (Entry.WTransform * vec4(InPos, 1)).xyz;
    OutWorldNormal = (Entry.WTransform * vec4(InNormal, 0)).xyz;
    OutUv = InUv;
    OutInstanceId = gl_InstanceIndex;
}

#endif

#if FRAGMENT_SHADER

layout(location = 0) in vec3 InWorldPos;
layout(location = 1) in vec3 InWorldNormal;
layout(location = 2) in vec2 InUv;
layout(location = 3) in flat uint InInstanceId;

layout(location = 0) out vec4 OutColor;

void main()
{
#if 0
    // TODO: Can we get tangent/bitangent via derivatives of Uv and WorldPos?
    vec3 SurfaceNormal = vec3(0);
    {
        // NOTE: Surface normal is stored in [0,1] range in texture, so we expand it to its real range here
        vec3 Normal = 2.0*texture(NormalTexture, InUv).xyz - 1.0;

        vec3 Q1 = dFdx(InWorldPos);
        vec3 Q2 = dFdy(InWorldPos);
        vec2 st1 = dFdx(InUv);
        vec2 st2 = dFdy(InUv);

        vec3 N = normalize(InWorldNormal);
        vec3 T = normalize(Q1*st2.t - Q2*st1.t);
        vec3 B = -normalize(cross(N, T));
        mat3 TBN = mat3(T, B, N);

        SurfaceNormal = normalize(TBN * Normal);
    }
    
    vec3 SurfaceColor = texture(ColorTexture, InUv).rgb;
    float SurfaceMetallic = texture(MetallicTexture, InUv).r;
    float SurfaceRoughness = texture(RoughnessTexture, InUv).r;
    float SurfaceAo = texture(AoTexture, InUv).r;
#else
    vec3 SurfaceNormal = normalize(InWorldNormal);
    vec3 SurfaceColor = vec3(1, 0, 0);
    float SurfaceMetallic = InstanceBuffer.Entries[InInstanceId].Metallic;
    float SurfaceRoughness = InstanceBuffer.Entries[InInstanceId].Roughness;
    float SurfaceAo = 1.0;
#endif
    
    vec3 SurfacePos = InWorldPos;
    vec3 View = normalize(CameraBuffer.CameraPos - SurfacePos);
    vec3 ReflectionDir = reflect(-View, SurfaceNormal);

    // NOTE: This part isn't well explained but if dia-electric use F0 of 0.04, otherwise the more metalic use albedo
    vec3 SurfaceReflection = mix(vec3(0.04), SurfaceColor, SurfaceMetallic);
    vec3 Color = vec3(0);

#if 0
    // NOTE: Point Light
    {
        vec3 LightPos = vec3(5.5, 5, -1);
        vec3 LightColor = 45*vec3(1, 1, 1);
        vec3 LightDir = normalize(LightPos - SurfacePos);

        //Color += BlinnPhongLighting(View, SurfaceColor, SurfaceNormal, 32, LightDir, PointLight(SurfacePos, LightPos, LightColor));
        Color += PbrPointLight(View, SurfacePos, SurfaceColor, SurfaceNormal, SurfaceReflection, SurfaceRoughness, SurfaceMetallic, LightPos, LightColor);
    }
#endif

#if 0
    // NOTE: Directional Light
    {
        vec3 LightColor = 3*vec3(1, 1, 1);
        vec3 LightDir = normalize(vec3(0, 0, -1));

        Color += PbrDirectionalLight(View, SurfacePos, SurfaceColor, SurfaceNormal, SurfaceReflection, SurfaceRoughness, SurfaceMetallic, LightDir, LightColor);
    }
#endif
    
    // NOTE: Ambient Light (Constant)
#if 0
    {
        vec3 AmbientLight = 0.1*vec3(1, 1, 1);
        Color += AmbientLight * SurfaceColor * SurfaceAo;
    }
#endif

    // NOTE: Ambient Light (Irradiance + PreFiltered Env Map)
    {
        vec3 Ks = FresnelSchlickRoughness(max(dot(SurfaceNormal, View), 0), SurfaceReflection, SurfaceRoughness);
        vec3 Kd = (1.0 - Ks) * (1.0 - SurfaceMetallic);
        
        vec3 Irradiance = texture(IrradianceMap, SurfaceNormal).rgb;
        vec3 Diffuse = Irradiance * SurfaceColor;

        const float MaxReflectionLod = 4.0;
        vec3 PreFilteredColor = textureLod(PreFilteredEnvMap, ReflectionDir, SurfaceRoughness * MaxReflectionLod).rgb;
        vec2 EnvBrdf = texture(BrdfLut, vec2(max(dot(SurfaceNormal, View), 0), SurfaceRoughness)).rg;
        vec3 Specular = vec3(0.0f); //PreFilteredColor * (Ks * EnvBrdf.x + EnvBrdf.y);
        
        vec3 Ambient = (Kd * Diffuse + Specular) * SurfaceAo;
        Color += Ambient;
    }
    
    // TODO: Gamma correction
    OutColor = vec4(clamp(pow(Color, vec3(1.0 / 2.2)), 0, 1), 1);
    //OutColor = vec4(vec3(SurfaceRoughness), 1);
    //OutColor = vec4(SurfaceNormal, 1);
}

#endif
