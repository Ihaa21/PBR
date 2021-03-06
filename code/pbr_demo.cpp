
/*

  TODO: BUGS IN PBR:

    - Reflection vectors for specular ibl seem wrong on one (y) axis. They should be goin in the opposite direction. Maybe cube maps coords
      aren't properly setup or something

    - There is a halo effect around spheres with irradiance. I think this is because the actual renderer uses different pbr functions for
      irradiance vs analytic lights. Double check this path.

    - Specular ibl doesn't take into account surface roughness correctly. There should be less reflections on rougher surfaces but this
      does not seem to happen. Everything is very reflective. Check if we are sampling the mip levels correctly.
  
 */

#include "pbr_demo.h"

inline model_skin DemoSetupPbrSkin(char* Color, char* Normal, char* Metallic, char* Roughness, char* Ao)
{
    model_skin Result = {};
    
    // TODO: Set correct formats
    Result.Color = TextureLoad(Color, VK_FORMAT_R8G8B8A8_UNORM, false, sizeof(u8), 4);
    Result.Normal = TextureLoad(Normal, VK_FORMAT_R8G8B8A8_UNORM, false, sizeof(u8), 4);
    Result.Metallic = TextureLoad(Metallic, VK_FORMAT_R8G8B8A8_UNORM, false, sizeof(u8), 4);
    Result.Roughness = TextureLoad(Roughness, VK_FORMAT_R8G8B8A8_UNORM, false, sizeof(u8), 4);
    Result.Ao = TextureLoad(Ao, VK_FORMAT_R8G8B8A8_UNORM, false, sizeof(u8), 4);
    
    // NOTE: Create descriptor set
    Result.Descriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, DemoState->PbrSkinDescLayout);
    VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptor, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           Result.Color.View, DemoState->AnisoSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptor, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           Result.Normal.View, DemoState->AnisoSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptor, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           Result.Metallic.View, DemoState->AnisoSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptor, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           Result.Roughness.View, DemoState->AnisoSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptor, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           Result.Ao.View, DemoState->AnisoSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // TODO: These are hardcoded as globals right now
    VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptor, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           DemoState->IrradianceMapEntry.View, DemoState->AnisoSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptor, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           DemoState->PreFilteredEnvMap.View, DemoState->AnisoSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptor, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           DemoState->IntegratedBrdfEntry.View, DemoState->AnisoSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return Result;
}

inline void DemoAllocGlobals(linear_arena* Arena)
{
    // IMPORTANT: These are always the top of the program memory
    DemoState = PushStruct(Arena, demo_state);
    RenderState = PushStruct(Arena, render_state);
}

DEMO_INIT(Init)
{
    // NOTE: Init Memory
    {
        linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
        DemoAllocGlobals(&Arena);
        *DemoState = {};
        *RenderState = {};
        DemoState->Arena = Arena;
        DemoState->TempArena = LinearSubArena(&DemoState->Arena, MegaBytes(10));
    }

    // NOTE: Init Vulkan
    {
        {
            const char* DeviceExtensions[] =
            {
                "VK_EXT_shader_viewport_index_layer",
            };
            
            render_init_params InitParams = {};
            InitParams.ValidationEnabled = true;
            InitParams.WindowWidth = WindowWidth;
            InitParams.WindowHeight = WindowHeight;
            InitParams.StagingBufferSize = MegaBytes(1000);
            InitParams.DeviceExtensionCount = ArrayCount(DeviceExtensions);
            InitParams.DeviceExtensions = DeviceExtensions;
            VkInit(VulkanLib, hInstance, WindowHandle, &DemoState->Arena, &DemoState->TempArena, InitParams);
        }
    }
    
    // NOTE: Create samplers
    DemoState->PointSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f);
    DemoState->LinearSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f);
    DemoState->AnisoSampler = VkSamplerMipMapCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f,
                                                    VK_SAMPLER_MIPMAP_MODE_LINEAR, 0, 0, 5);
    
    DemoState->Camera = CameraFpsCreate(V3(5, 5, -7), V3(0, 0, 1), f32(RenderState->WindowWidth / RenderState->WindowHeight),
                                       0.001f, 1000.0f, 90.0f, 1.0f, 0.04f);
        
    // NOTE: Init render target entries
    DemoState->SwapChainEntry = RenderTargetSwapChainEntryCreate(RenderState->WindowWidth, RenderState->WindowHeight,
                                                                 RenderState->SwapChainFormat);
    RenderTargetEntryCreate(&RenderState->GpuArena, RenderState->WindowWidth, RenderState->WindowHeight, VK_FORMAT_D32_SFLOAT,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, &DemoState->DepthImage,
                            &DemoState->DepthEntry);

    // NOTE: Geometry RT
    {
        render_target_builder Builder = RenderTargetBuilderBegin(&DemoState->Arena, &DemoState->TempArena, RenderState->WindowWidth,
                                                                 RenderState->WindowHeight);
        RenderTargetAddTarget(&Builder, &DemoState->SwapChainEntry, VkClearColorCreate(0, 0, 0, 1));
        RenderTargetAddTarget(&Builder, &DemoState->DepthEntry, VkClearDepthStencilCreate(0, 0));
                            
        vk_render_pass_builder RpBuilder = VkRenderPassBuilderBegin(&DemoState->TempArena);

        u32 ColorId = VkRenderPassAttachmentAdd(&RpBuilder, DemoState->SwapChainEntry.Format, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        u32 DepthId = VkRenderPassAttachmentAdd(&RpBuilder, DemoState->DepthEntry.Format, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        VkRenderPassSubPassBegin(&RpBuilder, VK_PIPELINE_BIND_POINT_GRAPHICS);
        VkRenderPassColorRefAdd(&RpBuilder, ColorId, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderPassDepthRefAdd(&RpBuilder, DepthId, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        VkRenderPassSubPassEnd(&RpBuilder);

        DemoState->GeometryRenderTarget = RenderTargetBuilderEnd(&Builder, VkRenderPassBuilderEnd(&RpBuilder, RenderState->Device));
    }
    
    // NOTE: Environment Map Render Target
    {
        // NOTE: Create render pass
        vk_render_pass_builder RpBuilder = VkRenderPassBuilderBegin(&DemoState->TempArena);

        u32 ColorId = VkRenderPassAttachmentAdd(&RpBuilder, VK_FORMAT_R16G16B16A16_SFLOAT, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkRenderPassSubPassBegin(&RpBuilder, VK_PIPELINE_BIND_POINT_GRAPHICS);
        VkRenderPassColorRefAdd(&RpBuilder, ColorId, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderPassSubPassEnd(&RpBuilder);

        VkRenderPass CubeMapRenderPass = VkRenderPassBuilderEnd(&RpBuilder, RenderState->Device);

        u32 FaceResX = 512;
        u32 FaceResY = 512;

        // NOTE: Environment Map Target
        {
            RenderTargetCubeEntryCreate(&RenderState->GpuArena, FaceResX, FaceResY, VK_FORMAT_R16G16B16A16_SFLOAT,
                                        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                                        &DemoState->EnvironmentMap, &DemoState->EnvironmentMapEntry);

            render_target_builder Builder = RenderTargetBuilderBegin(&DemoState->Arena, &DemoState->TempArena, FaceResX, FaceResY);
            RenderTargetAddTarget(&Builder, &DemoState->EnvironmentMapEntry, VkClearColorCreate(0, 0, 0, 1));
            DemoState->EnvironmentTarget = RenderTargetBuilderEnd(&Builder, CubeMapRenderPass);
        }

        // NOTE: Irradiance Map Target
        {
            RenderTargetCubeEntryCreate(&RenderState->GpuArena, FaceResX, FaceResY, VK_FORMAT_R16G16B16A16_SFLOAT,
                                        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                                        &DemoState->IrradianceMap, &DemoState->IrradianceMapEntry);

            render_target_builder Builder = RenderTargetBuilderBegin(&DemoState->Arena, &DemoState->TempArena, FaceResX, FaceResY);
            RenderTargetAddTarget(&Builder, &DemoState->IrradianceMapEntry, VkClearColorCreate(0, 0, 0, 1));
            DemoState->IrradianceTarget = RenderTargetBuilderEnd(&Builder, CubeMapRenderPass);
        }

        // NOTE: PreFiltered Environment Map Target
        {
            VkFormat Format = VK_FORMAT_R16G16B16A16_SFLOAT;
            DemoState->PreFilteredEnvMap = VkCubeMapCreate(RenderState->Device, &RenderState->GpuArena, FaceResX, FaceResY, Format,
                                                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 5);
        
            for (u32 MipId = 0; MipId < ArrayCount(DemoState->PreFilteredEnvEntries); ++MipId)
            {
                u32 NewResX = FaceResX / u32(Pow(2.0f, f32(MipId)));
                u32 NewResY = FaceResY / u32(Pow(2.0f, f32(MipId)));
            
                VkImageView View = VkImageViewCreate(RenderState->Device, DemoState->PreFilteredEnvMap.Image, VK_IMAGE_VIEW_TYPE_CUBE,
                                                     Format, VK_IMAGE_ASPECT_COLOR_BIT, MipId, 6);
                DemoState->PreFilteredEnvEntries[MipId] = RenderTargetCubeEntryCreate(NewResX, NewResY, Format, View);

                render_target_builder Builder = RenderTargetBuilderBegin(&DemoState->Arena, &DemoState->TempArena, NewResX, NewResY);
                RenderTargetAddTarget(&Builder, DemoState->PreFilteredEnvEntries + MipId, VkClearColorCreate(0, 0, 0, 1));
                DemoState->PreFilteredEnvTargets[MipId] = RenderTargetBuilderEnd(&Builder, CubeMapRenderPass);
            }
        }
    }

    //
    // NOTE: Create Equirectangular To CubeMap Data
    //
    {
        // NOTE: Create descriptor set layout
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&DemoState->EquirectangularToCubeMapDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        // NOTE: Create Pipeline
        {
            vk_pipeline_builder Builder = VkPipelineBuilderBegin(&DemoState->TempArena);

            // NOTE: Shaders
            VkPipelineShaderAdd(&Builder, "shader_cubemap_vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT);
            VkPipelineShaderAdd(&Builder, "shader_equirectangular_to_cubemap_frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
                
            // NOTE: Specify input vertex data format
            VkPipelineVertexBindingBegin(&Builder);
            VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32G32B32_SFLOAT, 2*sizeof(v3) + sizeof(v2));
            VkPipelineVertexBindingEnd(&Builder);

            VkPipelineInputAssemblyAdd(&Builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
            VkPipelineDepthStateAdd(&Builder, VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER);
            
            // NOTE: Set the blending state
            VkPipelineColorAttachmentAdd(&Builder, VK_FALSE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
                                         VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO);

            VkDescriptorSetLayout Layouts[] =
            {
                RenderState->GlobalCubeMapDescLayout,
                DemoState->EquirectangularToCubeMapDescLayout,
            };
            DemoState->EquirectangularToCubeMapPipeline = VkPipelineBuilderEnd(&Builder, RenderState->Device, &RenderState->PipelineManager,
                                                                               DemoState->EnvironmentTarget.RenderPass, 0, Layouts, ArrayCount(Layouts));
        }        
    }
    
    //
    // NOTE: Create Render Cubemap Data
    //
    {
        // NOTE: Create descriptor set layout
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&DemoState->RenderCubeMapDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        // NOTE: Create Pipeline
        {
            vk_pipeline_builder Builder = VkPipelineBuilderBegin(&DemoState->TempArena);

            // NOTE: Shaders
            VkPipelineShaderAdd(&Builder, "shader_render_cubemap_vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT);
            VkPipelineShaderAdd(&Builder, "shader_render_cubemap_frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
                
            // NOTE: Specify input vertex data format
            VkPipelineVertexBindingBegin(&Builder);
            VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32G32B32_SFLOAT, 2*sizeof(v3) + sizeof(v2));
            VkPipelineVertexBindingEnd(&Builder);

            VkPipelineInputAssemblyAdd(&Builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
            VkPipelineDepthStateAdd(&Builder, VK_TRUE, VK_FALSE, VK_COMPARE_OP_GREATER_OR_EQUAL);
            
            // NOTE: Set the blending state
            VkPipelineColorAttachmentAdd(&Builder, VK_FALSE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
                                         VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO);
            
            DemoState->RenderCubeMapPipeline = VkPipelineBuilderEnd(&Builder, RenderState->Device, &RenderState->PipelineManager,
                                                                    DemoState->GeometryRenderTarget.RenderPass, 0,
                                                                    &DemoState->RenderCubeMapDescLayout, 1);
        }

        // TODO: This is currently global so we create everything here
        DemoState->RenderCubeMapInputs = VkBufferCreate(RenderState->Device, &RenderState->GpuArena,
                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                        sizeof(render_cubemap_inputs));
        DemoState->RenderCubeMapDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, DemoState->RenderCubeMapDescLayout);
        VkDescriptorBufferWrite(&RenderState->DescriptorManager, DemoState->RenderCubeMapDescriptor, 0,
                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DemoState->RenderCubeMapInputs);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, DemoState->RenderCubeMapDescriptor, 1,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DemoState->EnvironmentMapEntry.View, DemoState->AnisoSampler,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    //
    // NOTE: Create Pbr data
    //
    {
        // NOTE: Create descriptor set layout
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&DemoState->PbrSkinDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        // NOTE: Create global descriptor set layout
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&DemoState->PbrGlobalDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        // NOTE: Create Pipeline
        {
            vk_pipeline_builder Builder = VkPipelineBuilderBegin(&DemoState->TempArena);

            // NOTE: Shaders
            VkPipelineShaderAdd(&Builder, "pbr_vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT);
            VkPipelineShaderAdd(&Builder, "pbr_frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
                
            // NOTE: Specify input vertex data format
            VkPipelineVertexBindingBegin(&Builder);
            VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32G32B32_SFLOAT, sizeof(v3));
            VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32G32B32_SFLOAT, sizeof(v3));
            VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32G32_SFLOAT, sizeof(v2));
            VkPipelineVertexBindingEnd(&Builder);

            VkPipelineInputAssemblyAdd(&Builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
            VkPipelineDepthStateAdd(&Builder, VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER);
                
            // NOTE: Set the blending state
            VkPipelineColorAttachmentAdd(&Builder, VK_FALSE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
                                         VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO);

            VkDescriptorSetLayout DescriptorLayouts[] =
            {
                DemoState->PbrSkinDescLayout,
                DemoState->PbrGlobalDescLayout,
            };
            
            DemoState->PbrPipeline = VkPipelineBuilderEnd(&Builder, RenderState->Device, &RenderState->PipelineManager,
                                                          DemoState->GeometryRenderTarget.RenderPass, 0,
                                                          DescriptorLayouts, ArrayCount(DescriptorLayouts));
        }
            
        // NOTE: Create Instance Buffer
        {
            u32 NumX = 10;
            u32 NumY = 10;
            DemoState->NumInstances = NumX * NumY;
            DemoState->InstancePositions = PushArray(&DemoState->Arena, v3, DemoState->NumInstances);
            for (u32 Y = 0; Y < NumY; ++Y)
            {
                for (u32 X = 0; X < NumX; ++X)
                {
                    DemoState->InstancePositions[Y * NumX + X] = V3(f32(X), f32(Y), 0.0f);
                }
            }

            // TODO: Implement as a actual instance buffer
            DemoState->InstanceBuffer = VkBufferCreate(RenderState->Device, &RenderState->GpuArena,
                                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                       sizeof(model_input)*DemoState->NumInstances);
        }
    }
    
    //
    // NOTE: Create PBR Irradiance data
    //
    {
        // NOTE: Create descriptor set layout
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&DemoState->IrradianceConvolutionDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        // NOTE: Create Pipeline
        {
            vk_pipeline_builder Builder = VkPipelineBuilderBegin(&DemoState->TempArena);

            // NOTE: Shaders
            VkPipelineShaderAdd(&Builder, "shader_cubemap_vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT);
            VkPipelineShaderAdd(&Builder, "shader_irradiance_convolution_frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
                
            // NOTE: Specify input vertex data format
            VkPipelineVertexBindingBegin(&Builder);
            VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32G32B32_SFLOAT, 2*sizeof(v3) + sizeof(v2));
            VkPipelineVertexBindingEnd(&Builder);

            VkPipelineInputAssemblyAdd(&Builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
            VkPipelineDepthStateAdd(&Builder, VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER);
            
            // NOTE: Set the blending state
            VkPipelineColorAttachmentAdd(&Builder, VK_FALSE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
                                         VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO);

            VkDescriptorSetLayout Layouts[] =
            {
                RenderState->GlobalCubeMapDescLayout,
                DemoState->IrradianceConvolutionDescLayout,
            };
            DemoState->IrradianceConvolutionPipeline = VkPipelineBuilderEnd(&Builder, RenderState->Device, &RenderState->PipelineManager,
                                                                            DemoState->IrradianceTarget.RenderPass, 0, Layouts, ArrayCount(Layouts));
        }

        // TODO: Currently this is global so we setup the descriptor data here
        DemoState->IrradianceConvolutionDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, DemoState->IrradianceConvolutionDescLayout);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, DemoState->IrradianceConvolutionDescriptor, 0,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DemoState->EnvironmentMapEntry.View, DemoState->AnisoSampler,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    //
    // NOTE: Create PBR PreFiltered Env Map data
    //
    {
        // NOTE: Create descriptor set layout
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&DemoState->PreFilteredEnvDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        // NOTE: Create Pipeline
        {
            vk_pipeline_builder Builder = VkPipelineBuilderBegin(&DemoState->TempArena);

            // NOTE: Shaders
            VkPipelineShaderAdd(&Builder, "shader_cubemap_vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT);
            VkPipelineShaderAdd(&Builder, "shader_prefiltered_env_map_frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
                
            // NOTE: Specify input vertex data format
            VkPipelineVertexBindingBegin(&Builder);
            VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32G32B32_SFLOAT, 2*sizeof(v3) + sizeof(v2));
            VkPipelineVertexBindingEnd(&Builder);

            VkPipelineInputAssemblyAdd(&Builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
            VkPipelineDepthStateAdd(&Builder, VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER);
            
            // NOTE: Set the blending state
            VkPipelineColorAttachmentAdd(&Builder, VK_FALSE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
                                         VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO);

            VkDescriptorSetLayout Layouts[] =
                {
                    RenderState->GlobalCubeMapDescLayout,
                    DemoState->PreFilteredEnvDescLayout,
                };
            DemoState->PreFilteredEnvPipeline = VkPipelineBuilderEnd(&Builder, RenderState->Device, &RenderState->PipelineManager,
                                                                     DemoState->PreFilteredEnvTargets[0].RenderPass, 0, Layouts,
                                                                     ArrayCount(Layouts));
        }
    }

    //
    // NOTE: Create Integrated BRDF Data
    //
    {
        RenderTargetEntryCreate(&RenderState->GpuArena, 512, 512, VK_FORMAT_R32G32_SFLOAT,
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                                &DemoState->IntegratedBrdf, &DemoState->IntegratedBrdfEntry);

        // NOTE: Integrated Brdf RT
        {
            render_target_builder Builder = RenderTargetBuilderBegin(&DemoState->Arena, &DemoState->TempArena, 512, 512);
            RenderTargetAddTarget(&Builder, &DemoState->IntegratedBrdfEntry, VkClearColorCreate(0, 0, 0, 0));
                            
            vk_render_pass_builder RpBuilder = VkRenderPassBuilderBegin(&DemoState->TempArena);

            u32 ColorId = VkRenderPassAttachmentAdd(&RpBuilder, DemoState->IntegratedBrdfEntry.Format, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                    VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            VkRenderPassSubPassBegin(&RpBuilder, VK_PIPELINE_BIND_POINT_GRAPHICS);
            VkRenderPassColorRefAdd(&RpBuilder, ColorId, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            VkRenderPassSubPassEnd(&RpBuilder);

            DemoState->IntegratedBrdfTarget = RenderTargetBuilderEnd(&Builder, VkRenderPassBuilderEnd(&RpBuilder, RenderState->Device));
        }

        DemoState->IntegratedBrdfPass = FullScreenPassCreate("shader_brdf_convolution_frag.spv", "main", &DemoState->IntegratedBrdfTarget,
                                                             0, 0, 0, 0);
    }
    
    // NOTE: Upload assets
    vk_commands Commands = RenderState->Commands;
    VkCommandsBegin(RenderState->Device, Commands);
    {
        // NOTE: Push geometry
        DemoState->Quad = AssetsPushQuad();
        DemoState->Cube = AssetsPushCube();
        DemoState->Sphere = AssetsPushSphere(64, 64);
        
        // NOTE: Push pbr data
        {
            DemoState->GoldSkin = DemoSetupPbrSkin("textures\\gold\\albedo.png", "textures\\gold\\normal.png", "textures\\gold\\metallic.png", "textures\\gold\\roughness.png", "textures\\gold\\ao.png");
            //DemoState->GrassSkin = DemoSetupPbrSkin("textures\\grass\\albedo.png", "textures\\grass\\normal.png", "textures\\grass\\metallic.png", "textures\\grass\\roughness.png", "textures\\grass\\ao.png");
            //DemoState->PlasticSkin = DemoSetupPbrSkin("textures\\plastic\\albedo.png", "textures\\plastic\\normal.png", "textures\\plastic\\metallic.png", "textures\\plastic\\roughness.png", "textures\\plastic\\ao.png");
            DemoState->RustedIronSkin = DemoSetupPbrSkin("textures\\rusted_iron\\albedo.png", "textures\\rusted_iron\\normal.png", "textures\\rusted_iron\\metallic.png", "textures\\rusted_iron\\roughness.png", "textures\\rusted_iron\\ao.png");
            //DemoState->WallSkin = DemoSetupPbrSkin("textures\\wall\\albedo.png", "textures\\wall\\normal.png", "textures\\wall\\metallic.png", "textures\\wall\\roughness.png", "textures\\wall\\ao.png");
            
            // NOTE: Create descriptor set
            {
                DemoState->PbrGlobalDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, DemoState->PbrGlobalDescLayout);
                VkDescriptorBufferWrite(&RenderState->DescriptorManager, DemoState->PbrGlobalDescriptor, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        DemoState->Camera.GpuBuffer);
                VkDescriptorBufferWrite(&RenderState->DescriptorManager, DemoState->PbrGlobalDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        DemoState->InstanceBuffer);
            }
        }

        // NOTE: Push PBR Cubemap/Irradiance Data
        {
            // VK_FORMAT_R16G16B16A16_SFLOAT
            DemoState->IrradianceRect = TextureLoad("textures\\skybox_hdr\\newport_loft.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, true, sizeof(f32), 4);
            
            // NOTE: Irradiance Convolution Descriptor
            {
                DemoState->IrradianceRectDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, DemoState->EquirectangularToCubeMapDescLayout);
                VkDescriptorImageWrite(&RenderState->DescriptorManager, DemoState->IrradianceRectDescriptor, 0,
                                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DemoState->IrradianceRect.View, DemoState->AnisoSampler,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }

        // NOTE: Push PBR PreFiltered Env Map Data
        {
            u32 EntrySize = u32(Max(RenderState->DeviceLimits.minUniformBufferOffsetAlignment, sizeof(prefiltered_env_input))); 
            DemoState->PreFilteredEnvBuffer = VkBufferCreate(RenderState->Device, &RenderState->GpuArena,
                                                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                             EntrySize*5);
            u8* Data = VkTransferPushWrite(&RenderState->TransferManager, DemoState->PreFilteredEnvBuffer, EntrySize*5,
                                           BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                           BarrierMask(VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT));

            for (u32 MipId = 0; MipId < 5; ++MipId)
            {
                prefiltered_env_input* Input = (prefiltered_env_input*)Data;
                Input->Roughness.x = f32(MipId) / (5.0f - 1.0f);

                Data += EntrySize;
            }
            
            // TODO: Currently this is global so we setup teh descriptor data here
            DemoState->PreFilteredEnvDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, DemoState->PreFilteredEnvDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, DemoState->PreFilteredEnvDescriptor, 0,
                                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DemoState->EnvironmentMapEntry.View, DemoState->AnisoSampler,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, DemoState->PreFilteredEnvDescriptor, 1,
                                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, DemoState->PreFilteredEnvBuffer);
        }
        
        VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
        VkTransferManagerFlush(&RenderState->TransferManager, RenderState->Device, Commands.Buffer, &RenderState->BarrierManager);
    }
    
    // NOTE: Generate environment map
    RenderTargetPassBegin(&DemoState->EnvironmentTarget, Commands, RenderTargetRenderPass_SetViewPort | RenderTargetRenderPass_SetScissor);
    {
        vkCmdBindPipeline(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->EquirectangularToCubeMapPipeline->Handle);

        // NOTE: Render to each face
        VkDeviceSize Offset = 0;
        vkCmdBindVertexBuffers(Commands.Buffer, 0, 1, &DemoState->Cube.Vertices, &Offset);
        vkCmdBindIndexBuffer(Commands.Buffer, DemoState->Cube.Indices, 0, VK_INDEX_TYPE_UINT32);
        for (u32 FaceId = 0; FaceId < 6; ++FaceId)
        {
            u32 BufferOffsets = u32(Max(RenderState->DeviceLimits.minUniformBufferOffsetAlignment, sizeof(global_cubemap_input_entry))*FaceId);
            VkDescriptorSet DescriptorSets[] =
                {
                    RenderState->GlobalCubeMapDescriptor,
                    DemoState->IrradianceRectDescriptor,
                };
            vkCmdBindDescriptorSets(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->EquirectangularToCubeMapPipeline->Layout,
                                    0, ArrayCount(DescriptorSets), DescriptorSets, 1, &BufferOffsets);
            vkCmdDrawIndexed(Commands.Buffer, DemoState->Cube.NumIndices, 1, 0, 0, 0);
        }
    }
    RenderTargetPassEnd(Commands);
    
    // NOTE: Generate irradiance map
    RenderTargetPassBegin(&DemoState->IrradianceTarget, Commands, RenderTargetRenderPass_SetViewPort | RenderTargetRenderPass_SetScissor);
    {
        // NOTE: Render to each face
        VkDeviceSize Offset = 0;
        vkCmdBindVertexBuffers(Commands.Buffer, 0, 1, &DemoState->Cube.Vertices, &Offset);
        vkCmdBindIndexBuffer(Commands.Buffer, DemoState->Cube.Indices, 0, VK_INDEX_TYPE_UINT32);
        for (u32 FaceId = 0; FaceId < 6; ++FaceId)
        {
            vkCmdBindPipeline(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->IrradianceConvolutionPipeline->Handle);

            u32 BufferOffsets = u32(Max(RenderState->DeviceLimits.minUniformBufferOffsetAlignment, sizeof(global_cubemap_input_entry))*FaceId);
            VkDescriptorSet DescriptorSets[] =
            {
                RenderState->GlobalCubeMapDescriptor,
                DemoState->IrradianceConvolutionDescriptor,
            };
            vkCmdBindDescriptorSets(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->IrradianceConvolutionPipeline->Layout,
                                    0, ArrayCount(DescriptorSets), DescriptorSets, 1, &BufferOffsets);

            vkCmdDrawIndexed(Commands.Buffer, DemoState->Cube.NumIndices, 1, 0, 0, 0);
        }
    }
    RenderTargetPassEnd(Commands);
        
    // NOTE: Generate prefiltered env map
    for (u32 MipId = 0; MipId < 5; ++MipId)
    {
        RenderTargetPassBegin(&DemoState->PreFilteredEnvTargets[MipId], Commands, RenderTargetRenderPass_SetViewPort | RenderTargetRenderPass_SetScissor);

        // NOTE: Render to each face
        VkDeviceSize Offset = 0;
        vkCmdBindVertexBuffers(Commands.Buffer, 0, 1, &DemoState->Cube.Vertices, &Offset);
        vkCmdBindIndexBuffer(Commands.Buffer, DemoState->Cube.Indices, 0, VK_INDEX_TYPE_UINT32);
        for (u32 FaceId = 0; FaceId < 6; ++FaceId)
        {
            vkCmdBindPipeline(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->PreFilteredEnvPipeline->Handle);

            u32 BufferOffsets[2] =
                {
                    u32(Max(RenderState->DeviceLimits.minUniformBufferOffsetAlignment, sizeof(global_cubemap_input_entry))*FaceId),
                    u32(Max(RenderState->DeviceLimits.minUniformBufferOffsetAlignment, sizeof(prefiltered_env_input))*MipId),
                };
            VkDescriptorSet DescriptorSets[] =
                {
                    RenderState->GlobalCubeMapDescriptor,
                    DemoState->PreFilteredEnvDescriptor,
                };
            vkCmdBindDescriptorSets(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->PreFilteredEnvPipeline->Layout,
                                    0, ArrayCount(DescriptorSets), DescriptorSets, ArrayCount(BufferOffsets), BufferOffsets);

            vkCmdDrawIndexed(Commands.Buffer, DemoState->Cube.NumIndices, 1, 0, 0, 0);
        }
        
        RenderTargetPassEnd(Commands);
    }

    // NOTE: Compute Integrated BRDF
    FullScreenPassRender(Commands, &DemoState->IntegratedBrdfPass);
    
    VkCommandsSubmit(RenderState->GraphicsQueue, Commands);
}

DEMO_DESTROY(Destroy)
{
}

DEMO_SWAPCHAIN_CHANGE(SwapChainChange)
{
}

DEMO_CODE_RELOAD(CodeReload)
{
    linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
    // IMPORTANT: We are relying on the memory being the same here since we have the same base ptr with the VirtualAlloc so we just need
    // to patch our global pointers here
    DemoAllocGlobals(&Arena);

    VkGetGlobalFunctionPointers(VulkanLib);
    VkGetInstanceFunctionPointers();
    VkGetDeviceFunctionPointers();
}

DEMO_MAIN_LOOP(MainLoop)
{
    u32 ImageIndex;
    VkCheckResult(vkAcquireNextImageKHR(RenderState->Device, RenderState->SwapChain,
                                        UINT64_MAX, RenderState->ImageAvailableSemaphore,
                                        VK_NULL_HANDLE, &ImageIndex));
    DemoState->SwapChainEntry.View = RenderState->SwapChainViews[ImageIndex];

    CameraUpdate(&DemoState->Camera, CurrInput, PrevInput);

    vk_commands Commands = RenderState->Commands;
    VkCommandsBegin(RenderState->Device, Commands);

    // NOTE: Update pipelines
    VkPipelineUpdateShaders(RenderState->Device, &RenderState->CpuArena, &RenderState->PipelineManager);

    RenderTargetUpdateEntries(&DemoState->Arena, &DemoState->GeometryRenderTarget);

    // NOTE: Upload uniforms
    {
        // NOTE: Upload camera
        {
            camera_input* Data = VkTransferPushWriteStruct(&RenderState->TransferManager, DemoState->Camera.GpuBuffer, camera_input,
                                                           BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                           BarrierMask(VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT));
            *Data = {};
            Data->CameraPos = DemoState->Camera.Pos;
        }
            
        // NOTE: Push instance buffer
        {
            model_input* Data = VkTransferPushWriteArray(&RenderState->TransferManager, DemoState->InstanceBuffer, model_input,
                                                         DemoState->NumInstances,
                                                         BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                         BarrierMask(VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT));

            model_input* CurrInstance = Data;
            for (u32 InstanceId = 0; InstanceId < DemoState->NumInstances; ++InstanceId, ++Data)
            {
                *Data = {};
                Data->WTransform = M4Pos(DemoState->InstancePositions[InstanceId])*M4Scale(V3(0.35f, 0.35f, 0.35f));
                Data->WVPTransform = CameraGetVP(&DemoState->Camera) * Data->WTransform;

                u32 X = InstanceId % 10;
                u32 Y = InstanceId / 10;
                Data->Roughness = f32(X) / 10.0f;
                Data->Metallic = f32(Y) / 10.0f;
            }
        }
        
        // NOTE: Push Render CubeMap Data
        {
            render_cubemap_inputs* Data = VkTransferPushWriteStruct(&RenderState->TransferManager, DemoState->RenderCubeMapInputs,
                                                                    render_cubemap_inputs,
                                                                    BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                                    BarrierMask(VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT));

            Data->WVPTransform = CameraGetVP(&DemoState->Camera)*M4Pos(DemoState->Camera.Pos);
        }

        VkTransferManagerFlush(&RenderState->TransferManager, RenderState->Device, RenderState->Commands.Buffer, &RenderState->BarrierManager);
    }

    RenderTargetPassBegin(&DemoState->GeometryRenderTarget, Commands, RenderTargetRenderPass_SetViewPort | RenderTargetRenderPass_SetScissor);
    
    // NOTE: Draw PBR Textures
    {
        vkCmdBindPipeline(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->PbrPipeline->Handle);
        VkDescriptorSet DescriptorSets[] =
        {
            DemoState->RustedIronSkin.Descriptor,
            DemoState->PbrGlobalDescriptor,
        };
        vkCmdBindDescriptorSets(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->PbrPipeline->Layout, 0,
                                ArrayCount(DescriptorSets), DescriptorSets, 0, 0);

        VkDeviceSize Offset = 0;
        vkCmdBindVertexBuffers(Commands.Buffer, 0, 1, &DemoState->Sphere.Vertices, &Offset);
        vkCmdBindIndexBuffer(Commands.Buffer, DemoState->Sphere.Indices, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(Commands.Buffer, DemoState->Sphere.NumIndices, DemoState->NumInstances, 0, 0, 0);
    }

    // NOTE: Draw Cube Map
    {
        vkCmdBindPipeline(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->RenderCubeMapPipeline->Handle);
        vkCmdBindDescriptorSets(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->RenderCubeMapPipeline->Layout, 0,
                                1, &DemoState->RenderCubeMapDescriptor, 0, 0);

        VkDeviceSize Offset = 0;
        vkCmdBindVertexBuffers(Commands.Buffer, 0, 1, &DemoState->Cube.Vertices, &Offset);
        vkCmdBindIndexBuffer(Commands.Buffer, DemoState->Cube.Indices, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(Commands.Buffer, DemoState->Cube.NumIndices, DemoState->NumInstances, 0, 0, 0);
    }
    
    RenderTargetPassEnd(Commands);        
    VkCheckResult(vkEndCommandBuffer(Commands.Buffer));
                    
    // NOTE: Render to our window surface
    // NOTE: Tell queue where we render to surface to wait
    VkPipelineStageFlags WaitDstMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.pWaitSemaphores = &RenderState->ImageAvailableSemaphore;
    SubmitInfo.pWaitDstStageMask = &WaitDstMask;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &Commands.Buffer;
    SubmitInfo.signalSemaphoreCount = 1;
    SubmitInfo.pSignalSemaphores = &RenderState->FinishedRenderingSemaphore;
    VkCheckResult(vkQueueSubmit(RenderState->GraphicsQueue, 1, &SubmitInfo, Commands.Fence));
            
    VkPresentInfoKHR PresentInfo = {};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = 1;
    PresentInfo.pWaitSemaphores = &RenderState->FinishedRenderingSemaphore;
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = &RenderState->SwapChain;
    PresentInfo.pImageIndices = &ImageIndex;
    VkResult Result = vkQueuePresentKHR(RenderState->PresentQueue, &PresentInfo);

    switch (Result)
    {
        case VK_SUCCESS:
        {
        } break;

        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
        {
            // NOTE: Window size changed
            InvalidCodePath;
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }
}
