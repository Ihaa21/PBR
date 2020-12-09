#pragma once

#include "framework_vulkan\framework_vulkan.h"

struct procedural_model
{
    u32 NumIndices;
    VkBuffer Vertices;
    VkBuffer Indices;
};

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

struct prefiltered_env_inputs
{
    v4 Roughness[5];
};

//
// NOTE: Cube Maps
//

struct global_cubemap_input_entry
{
    m4 WVPTransform;
    i32 LayerId;
    u32 Pad[3];
};

struct global_cubemap_inputs
{
    global_cubemap_input_entry Entries[6];
};

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
    render_target_entry DepthEntry;
    render_target GeometryRenderTarget;
    
    // NOTE: Models
    procedural_model Quad;
    procedural_model Cube;
    procedural_model Sphere;
    
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
    VkRenderPass CubeMapRenderPass;
    VkFramebuffer EnvironmentMapFbo;
    VkFramebuffer IrradianceMapFbo;
    
    // NOTE: Equirectangular Map // TODO: Figure out how to free this stuff dynamically
    vk_image IrradianceRect;
    VkDescriptorSet IrradianceRectDescriptor; 

    // NOTE: Cube Maps
    vk_image EnvironmentMap;
    vk_image IrradianceMap;
    vk_image PreFilteredEnvMap;
    VkImageView PreFilteredEnvViews[5];
    VkFramebuffer PreFilteredEnvFbos[5];

    // NOTE: Equirectangular to Cubemap Data
    VkDescriptorSetLayout EquirectangularToCubeMapDescLayout;
    vk_pipeline* EquirectangularToCubeMapPipeline;

    // NOTE: Irradiance Convolution
    VkDescriptorSetLayout IrradianceConvolutionDescLayout;
    vk_pipeline* IrradianceConvolutionPipeline;
    // TODO: Currently global
    VkDescriptorSet IrradianceConvolutionDescriptor;

    // NOTE: Prefiltered Env Map
    VkDescriptorSetLayout PreFilteredEnvDescLayout;
    vk_pipeline* PreFilteredEnvPipeline;
    // TODO: Currently global
    VkBuffer PreFilteredEnvBuffer;
    VkDescriptorSet PreFilteredEnvDescriptor;

    // NOTE: Integrated BRDF
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
