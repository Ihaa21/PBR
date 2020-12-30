#pragma once

#define VALIDATION

#include "framework_vulkan\framework_vulkan.h"

//
// NOTE: PBR 
//

struct model_input
{
    m4 WTransform;
    m4 WVPTransform;
    f32 Roughness;
    f32 Metallic;
    u32 Pad[2];
};

struct model_skin
{
    vk_image Color;
    vk_image Normal;
    vk_image Metallic;
    vk_image Roughness;
    vk_image Ao;
    VkDescriptorSet Descriptor;
};

// TODO: Store num mips we do somewhere, hardcoded rn
struct prefiltered_env_input
{
    v4 Roughness;
};

//
// NOTE: Cube Maps
//

struct render_cubemap_inputs
{
    m4 WVPTransform;
};

struct demo_state
{
    linear_arena Arena;
    linear_arena TempArena;

    // NOTE: Samplers
    VkSampler PointSampler;
    VkSampler LinearSampler;
    VkSampler AnisoSampler;
    
    camera Camera;

    // NOTE: Render Target Entries
    render_target_entry SwapChainEntry;
    VkImage DepthImage;
    render_target_entry DepthEntry;
    render_target GeometryRenderTarget;

    // NOTE: Models
    procedural_mesh Quad;
    procedural_mesh Cube;
    procedural_mesh Sphere;
    
    // NOTE: Pbr Assets
    model_skin GoldSkin;
    model_skin GrassSkin;
    model_skin PlasticSkin;
    model_skin RustedIronSkin;
    model_skin WallSkin;

    // TODO: Make Cube map Render Targets
    // NOTE: Global Cube Map Data
    VkBuffer GlobalCubeMapData;
    VkDescriptorSet GlobalCubeMapDescriptor;
    VkDescriptorSetLayout GlobalCubeMapDescLayout;
    
    // NOTE: Equirectangular Map // TODO: Figure out how to free this stuff dynamically
    vk_image IrradianceRect;
    VkDescriptorSet IrradianceRectDescriptor; 
    VkImage EnvironmentMap;
    render_target_entry EnvironmentMapEntry;
    render_target EnvironmentTarget;

    // NOTE: Equirectangular to Cubemap Data
    VkDescriptorSetLayout EquirectangularToCubeMapDescLayout;
    vk_pipeline* EquirectangularToCubeMapPipeline;

    // NOTE: Irradiance Convolution
    VkDescriptorSetLayout IrradianceConvolutionDescLayout;
    vk_pipeline* IrradianceConvolutionPipeline;
    // TODO: Currently global
    VkDescriptorSet IrradianceConvolutionDescriptor;
    VkImage IrradianceMap;
    render_target_entry IrradianceMapEntry;
    render_target IrradianceTarget;

    // NOTE: Prefiltered Env Map
    VkDescriptorSetLayout PreFilteredEnvDescLayout;
    vk_pipeline* PreFilteredEnvPipeline;
    // TODO: Currently global
    VkBuffer PreFilteredEnvBuffer;
    VkDescriptorSet PreFilteredEnvDescriptor;
    vk_image PreFilteredEnvMap;
    render_target_entry PreFilteredEnvEntries[5];
    render_target PreFilteredEnvTargets[5];

    // NOTE: Integrated BRDF
    VkImage IntegratedBrdf;
    render_target_entry IntegratedBrdfEntry;
    render_target IntegratedBrdfTarget;
    render_fullscreen_pass IntegratedBrdfPass;
    
    // NOTE: Pbr Data
    VkDescriptorSet PbrGlobalDescriptor;
    VkDescriptorSetLayout PbrSkinDescLayout;
    VkDescriptorSetLayout PbrGlobalDescLayout;
    vk_pipeline* PbrPipeline;
    
    // NOTE: Render Cubemap
    VkBuffer RenderCubeMapInputs;
    VkDescriptorSetLayout RenderCubeMapDescLayout;
    vk_pipeline* RenderCubeMapPipeline;
    // TODO: Currently global
    VkDescriptorSet RenderCubeMapDescriptor;
    
    u32 NumInstances;
    v3* InstancePositions;
    VkBuffer InstanceBuffer;
};

global demo_state* DemoState;
