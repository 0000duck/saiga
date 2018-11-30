//
// Created by Peter Eichinger on 15.10.18.
//

#pragma once
#include <vulkan/vulkan.hpp>
#include "saiga/export.h"
#include "saiga/vulkan/memory/ChunkCreator.h"
#include "saiga/util/imath.h"
#include "saiga/vulkan/memory/MemoryLocation.h"
#include "BaseMemoryAllocator.h"

#include <limits>
#include <saiga/util/easylogging++.h>
#include "FindMemoryType.h"
#include "SafeAllocator.h"
using namespace Saiga::Vulkan::Memory;

namespace Saiga{
namespace Vulkan{
namespace Memory{

struct SAIGA_GLOBAL SimpleMemoryAllocator : public BaseMemoryAllocator {
private:
    std::mutex mutex;
    vk::BufferCreateInfo m_bufferCreateInfo;
    vk::Device m_device;
    vk::PhysicalDevice m_physicalDevice;
    std::vector<MemoryLocation> m_allocations;

public:
    ~SimpleMemoryAllocator() override {
        destroy();
    }
    vk::MemoryPropertyFlags flags;
    vk::BufferUsageFlags  usageFlags;

    SimpleMemoryAllocator(vk::Device _device, vk::PhysicalDevice _physicalDevice, const vk::MemoryPropertyFlags &_flags,
                          const vk::BufferUsageFlags &usage, bool _mapped = false) : BaseMemoryAllocator(_mapped) {
        m_device = _device;
        m_physicalDevice = _physicalDevice;
        flags = _flags;
        mapped = _mapped;
        usageFlags = usage;
        m_bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;
        m_bufferCreateInfo.usage = usage;
        m_bufferCreateInfo.size = 0;
    }

    SimpleMemoryAllocator(SimpleMemoryAllocator&& other) noexcept : BaseMemoryAllocator(std::move(other)),
        m_bufferCreateInfo(std::move(other.m_bufferCreateInfo)),
        m_device(other.m_device),
        m_physicalDevice(other.m_physicalDevice),
        m_allocations(std::move(other.m_allocations)) {

    }


    MemoryLocation allocate(vk::DeviceSize size) override;

    void destroy() override;

    void deallocate(MemoryLocation &location) override;
};


}
}
}
