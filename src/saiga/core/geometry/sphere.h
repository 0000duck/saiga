/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once

#include "saiga/config.h"
#include "saiga/core/math/math.h"

#include "aabb.h"

namespace Saiga
{
class SAIGA_CORE_API Sphere
{
   public:
    vec3 pos;
    float r;


    Sphere(void) {}

    Sphere(const vec3& p, float r) : pos(p), r(r) {}
    ~Sphere(void) {}



    int intersectAabb(const AABB& other) const;
    bool intersectAabb2(const AABB& other) const;

    void getMinimumAabb(AABB& box) const;

    bool contains(vec3 p) const;
    bool intersect(const Sphere& other) const;

    //    TriangleMesh* createMesh(int rings, int sectors);
    //    void addToBuffer(std::vector<VertexNT> &vertices, std::vector<GLuint> &indices, int rings, int sectors);

    SAIGA_CORE_API friend std::ostream& operator<<(std::ostream& os, const Sphere& dt);
};

}  // namespace Saiga
