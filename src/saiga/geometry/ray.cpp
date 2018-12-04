/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "saiga/geometry/ray.h"

#include "internal/noGraphicsAPI.h"
namespace Saiga
{
std::ostream& operator<<(std::ostream& os, const Ray& r)
{
    std::cout << "Ray: " << r.origin << " " << r.direction;
    return os;
}

}  // namespace Saiga
