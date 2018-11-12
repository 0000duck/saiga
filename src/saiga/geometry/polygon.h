/**
 * Copyright (c) 2017 Darius Rückert 
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once


#include "saiga/config.h"
#include "saiga/util/math.h"
#include <saiga/geometry/aabb.h>
#include <saiga/geometry/triangle.h>

#include <vector>

namespace Saiga {

using PolygonType = std::vector<vec3>;

namespace Polygon {

inline vec3 center(const PolygonType& pol){
    vec3 c(0);
    for(auto po : pol){
        c += po;
    }
    return c / (float)pol.size();
}

inline AABB boundingBox(const PolygonType& pol){
    AABB res;
    res.makeNegative();
    for(auto p : pol){
        res.growBox(p);
    }
    return res;
}

inline PolygonType toPolygon(const Triangle& tri){
    PolygonType triP = {tri.a, tri.b, tri.c};
    return triP;
}



}

}
