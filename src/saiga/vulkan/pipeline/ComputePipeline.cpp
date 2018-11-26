﻿/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "ComputePipeline.h"
#include "saiga/vulkan/Vertex.h"
#include "saiga/vulkan/VulkanInitializers.hpp"

namespace Saiga
{
namespace Vulkan
{
void ComputePipelineInfo::setShader(Saiga::Vulkan::ShaderModule& shader)
{
    shaderStage = shader.createPipelineInfo();
}

vk::ComputePipelineCreateInfo ComputePipelineInfo::createCreateInfo(vk::PipelineLayout pipelineLayout)
{
    vk::ComputePipelineCreateInfo pipelineCreateInfo(vk::PipelineCreateFlags(), shaderStage, pipelineLayout,
                                                     vk::Pipeline(), 0);
    return pipelineCreateInfo;
}



ComputePipeline::ComputePipeline() : PipelineBase(vk::PipelineBindPoint::eCompute) {}


void ComputePipeline::create(ComputePipelineInfo pipelineInfo)
{
    createPipelineLayout();
    pipelineInfo.setShader(shader);
    auto pipelineCreateInfo = pipelineInfo.createCreateInfo(pipelineLayout);
    pipeline                = device.createComputePipeline(base->pipelineCache, pipelineCreateInfo);
    SAIGA_ASSERT(pipeline);
    shader.destroy();
}



}  // namespace Vulkan
}  // namespace Saiga
