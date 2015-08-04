#pragma once

#include <saiga/config.h>
#include "saiga/util/glm.h"

class SAIGA_GLOBAL Cone
{
public:
    glm::vec3 position;
    glm::vec3 direction;
    float alpha;
    float height;


    Cone(void){}

    Cone(const vec3 &position, const vec3 &direction, float alpha, float height)
        :position(position),direction(direction),alpha(alpha),height(height){}
    ~Cone(void){}



};
