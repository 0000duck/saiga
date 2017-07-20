/**
 * Copyright (c) 2017 Darius Rückert 
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once

#include "saiga/rendering/object3d.h"

namespace Saiga {

class ParticleSystem;


class SAIGA_GLOBAL ParticleEffect : public Object3D{
public:
    float velocity = 1.0f;
    float radius = 0.1f;
    float lifetime = 1.0f;
public:

    virtual void apply(ParticleSystem& ps) = 0;
};

}
