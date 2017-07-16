#include "saiga/rendering/particles/particle_shader.h"
#include "saiga/opengl/texture/raw_texture.h"

namespace Saiga {

void ParticleShader::checkUniforms()
{
    MVPTextureShader::checkUniforms();

    location_timer = getUniformLocation("timer");

    location_timestep =  getUniformLocation("timestep");
    location_interpolation = getUniformLocation("interpolation");
}

void ParticleShader::uploadTiming(int tick, float interpolation)
{
    Shader::upload(location_timer,tick);
    Shader::upload(location_interpolation,interpolation);
}

void ParticleShader::uploadTimestep(float timestep)
{
    Shader::upload(location_timestep,timestep);
}


//===============================================================

void DeferredParticleShader::checkUniforms()
{
    ParticleShader::checkUniforms();

    location_texture_depth = getUniformLocation("depthTexture");
    location_cameraParameters = getUniformLocation("cameraParameters");
}



void DeferredParticleShader::uploadDepthTexture(std::shared_ptr<raw_Texture> texture){
    texture->bind(1);
    Shader::upload(location_texture_depth,1);
}

void DeferredParticleShader::uploadCameraParameters(vec2 cp)
{
    Shader::upload(location_cameraParameters,cp);
}

}
