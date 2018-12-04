/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once

#include "saiga/rendering/particles/particleEffect.h"

namespace Saiga
{
class SAIGA_GLOBAL ParticleEmitter : public ParticleEffect
{
   public:
    float particlesPerTick = 0.5f;

    ParticleEmitter();

    virtual void apply(ParticleSystem& ps);
    virtual void spawnParticles(int count, ParticleSystem& ps) = 0;

    void setParticlesPerTick(float c);
    void setParticlesPerSecond(float c);


   private:
    float time = 0.0f;
};


class SAIGA_GLOBAL SphericalParticleEmitter : public ParticleEmitter
{
   public:
    SphericalParticleEmitter();
    void spawnParticles(int count, ParticleSystem& ps) override;
};


class SAIGA_GLOBAL ConaParticleEmitter : public ParticleEmitter
{
   public:
    vec3 coneDirection = vec3(0, 1, 0);
    float coneAngle    = 45.0f;  // in degrees
    ConaParticleEmitter();
    void spawnParticles(int count, ParticleSystem& ps) override;
};

}  // namespace Saiga
