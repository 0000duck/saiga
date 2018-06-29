﻿/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "ForwardRenderer.h"

namespace Saiga {
namespace Vulkan {

ForwardRenderer::ForwardRenderer(VulkanBase &base, SwapChain &swapChain)
    : base(base), swapChain(swapChain)
{
}

ForwardRenderer::~ForwardRenderer()
{

}

void ForwardRenderer::create(int width, int height)
{
    this->width = width;
    this->height = height;
    depthBuffer.init(base,width,height);

    // init render path

    {


        // The initial layout for the color and depth attachments will be
        // LAYOUT_UNDEFINED because at the start of the renderpass, we don't
        // care about their contents. At the start of the subpass, the color
        // attachment's layout will be transitioned to LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        // and the depth stencil attachment's layout will be transitioned to
        // LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL.  At the end of the renderpass,
        // the color attachment's layout will be transitioned to
        // LAYOUT_PRESENT_SRC_KHR to be ready to present.  This is all done as part
        // of the renderpass, no barriers are necessary.
        vk::AttachmentDescription attachments[2];
        attachments[0].format = swapChain.colorFormat;
        attachments[0].samples = vk::SampleCountFlagBits::e1;
        attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
        attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
        attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eStore;
        attachments[0].initialLayout = vk::ImageLayout::eUndefined;
        attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;
        //        attachments[0].flags = 0;

        attachments[1].format = depthBuffer.depthFormat;
        attachments[1].samples = vk::SampleCountFlagBits::e1;
        attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
        attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
        attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachments[1].initialLayout = vk::ImageLayout::eUndefined;
        attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        //        attachments[1].samples = NUM_SAMPLES;
        //        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        //        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        //        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        //        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        //        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        //        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        //        attachments[1].flags = 0;


        vk::AttachmentReference color_reference = {};
        color_reference.attachment = 0;
        color_reference.layout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::AttachmentReference depth_reference = {};
        depth_reference.attachment = 1;
        depth_reference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        vk::SubpassDescription subpass = {};
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        //        subpass.flags = 0;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = NULL;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_reference;
        subpass.pResolveAttachments = NULL;
        subpass.pDepthStencilAttachment = &depth_reference;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = NULL;

        vk::RenderPassCreateInfo rp_info = {};
        //        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO vk::st;
        //        rp_info.pNext = NULL;
        rp_info.attachmentCount = 2;
        rp_info.pAttachments = attachments;
        rp_info.subpassCount = 1;
        rp_info.pSubpasses = &subpass;
        rp_info.dependencyCount = 0;
        rp_info.pDependencies = NULL;

        //        res = vkCreateRenderPass(info.device, &rp_info, NULL, &info.render_pass);
        CHECK_VK(base.device.createRenderPass(&rp_info, NULL, &render_pass));




    }

    // init framebuffers

    {
        vk::ImageView attachments[2];
        attachments[1] = depthBuffer.depthview;

        vk::FramebufferCreateInfo fb_info = {};
        //        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        //        fb_info.pNext = NULL;
        fb_info.renderPass = render_pass;
        fb_info.attachmentCount = 2;
        fb_info.pAttachments = attachments;
        fb_info.width = width;
        fb_info.height = height;
        fb_info.layers = 1;

        uint32_t i;
        //        info.framebuffers = (VkFramebuffer *)malloc(info.swapchainImageCount * sizeof(VkFramebuffer));
        framebuffers.resize( swapChain.buffers.size());
        SAIGA_ASSERT(swapChain.buffers.size() >= 1);
        //        assert(info.framebuffers);

        for (i = 0; i < framebuffers.size(); i++)
        {
            //            attachments[0] = swapChainImagesViews[i];
            attachments[0] = swapChain.buffers[i].view;
            CHECK_VK(base.device.createFramebuffer(&fb_info, NULL, &framebuffers[i]));

            //            assert(res == VK_SUCCESS);
        }


    }

}

void ForwardRenderer::begin(vk::CommandBuffer &cmd)
{

    vk::ClearValue clear_values[2];
//    float c = float(i) / count;
    float c = 0.5f;
    clear_values[0].color.float32[0] = c;
    clear_values[0].color.float32[1] = c;
    clear_values[0].color.float32[2] = c;
    clear_values[0].color.float32[3] = c;
    clear_values[1].depthStencil.depth = 1.0f;
    clear_values[1].depthStencil.stencil = 0;

    vk::SemaphoreCreateInfo imageAcquiredSemaphoreCreateInfo;
    //        imageAcquiredSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    //        imageAcquiredSemaphoreCreateInfo.pNext = NULL;
    //        imageAcquiredSemaphoreCreateInfo.flags = 0;

    CHECK_VK(base.device.createSemaphore(&imageAcquiredSemaphoreCreateInfo, NULL, &imageAcquiredSemaphore));



    // Get the index of the next available swapchain image:
    //        current_buffer = device.acquireNextImageKHR(swap_chain, UINT64_MAX, imageAcquiredSemaphore, vk::Fence()).value;
    swapChain.acquireNextImage(imageAcquiredSemaphore,current_buffer);


    vk::RenderPassBeginInfo rp_begin;
    //        rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    //        rp_begin.pNext = NULL;
    rp_begin.renderPass = render_pass;
    rp_begin.framebuffer = framebuffers[current_buffer];
    rp_begin.renderArea.offset.x = 0;
    rp_begin.renderArea.offset.y = 0;
    rp_begin.renderArea.extent.width = width;
    rp_begin.renderArea.extent.height = height;
    rp_begin.clearValueCount = 2;
    rp_begin.pClearValues = clear_values;

    cmd.beginRenderPass(&rp_begin, vk::SubpassContents::eInline);
}

void ForwardRenderer::end(vk::CommandBuffer &cmd)
{
    const vk::CommandBuffer cmd_bufs[] = {cmd};
    vk::FenceCreateInfo fenceInfo;
    vk::Fence drawFence;
    //        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    //        fenceInfo.pNext = NULL;
    //        fenceInfo.flags = 0;
    base.device.createFence(&fenceInfo, NULL, &drawFence);



    vk::PipelineStageFlags pipe_stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submit_info[1] = {};
    //        submit_info[0].pNext = NULL;
    //        submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info[0].waitSemaphoreCount = 1;
    submit_info[0].pWaitSemaphores = &imageAcquiredSemaphore;
    submit_info[0].pWaitDstStageMask = &pipe_stage_flags;
    submit_info[0].commandBufferCount = 1;
    submit_info[0].pCommandBuffers = cmd_bufs;
    submit_info[0].signalSemaphoreCount = 0;
    submit_info[0].pSignalSemaphores = NULL;

    /* Queue the command buffer for execution */

    CHECK_VK(base.queue.submit( 1, submit_info, drawFence));
    //        res = vkQueueSubmit(info.graphics_queue,


    vk::PresentInfoKHR present;
    //        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    //        present.pNext = NULL;
    present.swapchainCount = 1;
    present.pSwapchains = &swapChain.swapChain;
    present.pImageIndices = &current_buffer;
    present.pWaitSemaphores = NULL;
    present.waitSemaphoreCount = 0;
    present.pResults = NULL;


    /* Make sure command buffer is finished before presenting */
    vk::Result res;
    do {
        res = base.device.waitForFences(1, &drawFence, VK_TRUE, 1241515);
    } while (res == vk::Result::eTimeout);


    base.queue.presentKHR(&present);
}


}
}