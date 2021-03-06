/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once

#include "saiga/config.h"
#include "saiga/core/math/math.h"

namespace Saiga
{
class SAIGA_CORE_API Ray
{
   public:
    vec3 direction;
    vec3 origin;

    Ray(const vec3& dir = make_vec3(0), const vec3& ori = make_vec3(0)) : direction(dir), origin(ori) {}


    vec3 positionOnRay(float t) const { return origin + t * direction; }

    SAIGA_CORE_API friend std::ostream& operator<<(std::ostream& os, const Ray& dt);
};

}  // namespace Saiga
