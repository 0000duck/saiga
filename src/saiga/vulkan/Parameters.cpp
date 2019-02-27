﻿/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "Parameters.h"

#include "saiga/core/util/ini/ini.h"

namespace Saiga
{
namespace Vulkan
{
void VulkanParameters::fromConfigFile(const std::string& file)
{
    Saiga::SimpleIni ini;
    ini.LoadFile(file.c_str());

    enableValidationLayer = ini.GetAddBool("Vulkan", "enableValidationLayer", enableValidationLayer);
    enableImgui           = ini.GetAddBool("Vulkan", "enableImgui", enableImgui);
    expand_memory_stats   = ini.GetBoolValue("Vulkan", "expandMemoryStats", false);

    if (ini.changed()) ini.SaveFile(file.c_str());
}

}  // namespace Vulkan
}  // namespace Saiga
