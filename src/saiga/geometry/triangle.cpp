/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "saiga/geometry/triangle.h"

namespace Saiga {

void Triangle::stretch(float f){
    vec3 cen = center();

    a = (a-cen) * f + a;
    b = (b-cen) * f + b;
    c = (c-cen) * f + c;
}

vec3 Triangle::center(){
    return (a+b+c) * float(1.0f/3.0f);
}

float Triangle::minimalAngle()
{
    return glm::acos(cosMinimalAngle());
}

float Triangle::cosMinimalAngle()
{
    return glm::max( glm::max(cosAngleAtCorner(0),cosAngleAtCorner(1)),cosAngleAtCorner(2) );
}

float Triangle::angleAtCorner(int i)
{
    return glm::acos(cosAngleAtCorner(i));
}

float Triangle::cosAngleAtCorner(int i)
{
    vec3 center = a;
    vec3 left = b;
    vec3 right = c;


    switch(i)
    {
    case 0:
        center = a;
        left = b;
        right = c;
        break;
    case 1:
        center = b;
        left = c;
        right = a;
        break;
    case 2:
        center = c;
        left = a;
        right = b;
        break;
    }

    return glm::dot( normalize( left - center ), normalize( right - center ) );
}

bool Triangle::isDegenerate()
{
    for(int i = 0; i < 3; ++i)
    {
        float a = cosAngleAtCorner(i);
        if(a <= -1 || a >= 1)
            return true;
    }
    return false;
    //    return !std::isfinite(angleAtCorner(0)) || !std::isfinite(angleAtCorner(1)) || !std::isfinite(angleAtCorner(2));
}

vec3 Triangle::normal()
{
    return normalize(cross(b-a,c-a));
}

std::ostream& operator<<(std::ostream& os, const Triangle& t)
{
    std::cout<<"Triangle: "<<t.a<<t.b<<t.c;
    return os;
}

}