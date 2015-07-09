#include "libhello/framework.h"
#include "libhello/rendering/deferred_renderer.h"
#include "libhello/window/window.h"

#include "libhello/opengl/shaderLoader.h"
#include "libhello/opengl/objloader.h"
#include "libhello/opengl/texture/textureLoader.h"


#include "libhello/rendering/lighting/directional_light.h"
#include "libhello/rendering/lighting/point_light.h"
#include "libhello/rendering/lighting/spot_light.h"
#include "libhello/rendering/lighting/box_light.h"

void initFramework(Window *window)
{
    ShaderLoader::instance()->addPath("shader");
    ShaderLoader::instance()->addPath("shader/geometry");
    ShaderLoader::instance()->addPath("shader/lighting");
    ShaderLoader::instance()->addPath("shader/post_processing");

    TextureLoader::instance()->addPath("./objs");
    TextureLoader::instance()->addPath("./textures");
    TextureLoader::instance()->addPath(".");

    MaterialLoader::instance()->addPath(".");
    MaterialLoader::instance()->addPath("./objs");

    ObjLoader::instance()->addPath(".");
    ObjLoader::instance()->addPath("./objs");


    DeferredShader* def = ShaderLoader::instance()->load<DeferredShader>("deferred_mixer.glsl");



    Deferred_Renderer* renderer = new Deferred_Renderer();
    renderer->init(def,window->getWidth(),window->getHeight());
    //    renderer->lighting.setShader(shaderLoader.load<SpotLightShader>("deferred_lighting_spotlight.glsl"));

    Shader::ShaderCodeInjections shadowInjection;
    shadowInjection.emplace_back(GL_FRAGMENT_SHADER,
                                    "#define SHADOWS",1); //after the version number

    renderer->lighting.setShader(
                ShaderLoader::instance()->load<SpotLightShader>("deferred_lighting_spotlight_shadow.glsl"),
                ShaderLoader::instance()->load<SpotLightShader>("deferred_lighting_spotlight_shadow.glsl",shadowInjection)
                );

    renderer->lighting.setShader(
                ShaderLoader::instance()->load<PointLightShader>("deferred_lighting_pointlight_shadow.glsl"),
                ShaderLoader::instance()->load<PointLightShader>("deferred_lighting_pointlight_shadow.glsl",shadowInjection)
                );

    renderer->lighting.setShader(
                ShaderLoader::instance()->load<DirectionalLightShader>("deferred_lighting_directional_shadow.glsl"),
                ShaderLoader::instance()->load<DirectionalLightShader>("deferred_lighting_directional_shadow.glsl",shadowInjection)
                                 );

    renderer->lighting.setShader(
                ShaderLoader::instance()->load<BoxLightShader>("deferred_lighting_boxlight_shadow.glsl"),
                ShaderLoader::instance()->load<BoxLightShader>("deferred_lighting_boxlight_shadow.glsl",shadowInjection)
                                 );

    renderer->lighting.setDebugShader(ShaderLoader::instance()->load<MVPColorShader>("deferred_lighting_debug.glsl"));
    renderer->lighting.setStencilShader(ShaderLoader::instance()->load<MVPShader>("deferred_lighting_stencil.glsl"));

    renderer->lighting.loadShaders();


    //    renderer->postProcessingShader  = shaderLoader.load<PostProcessingShader>("fxaa.glsl");
    //    renderer->postProcessingShader  = shaderLoader.load<PostProcessingShader>("SMAA.glsl");
    //    renderer->postProcessingShader  = shaderLoader.load<PostProcessingShader>("gaussian_blur.glsl");


    renderer->ssaoShader  =  ShaderLoader::instance()->load<SSAOShader>("ssao.glsl");
    renderer->ssao = true;
//    renderer->otherShader  =  ShaderLoader::instance()->load<PostProcessingShader>("post_processing.glsl");

    PostProcessingShader* pps = ShaderLoader::instance()->load<PostProcessingShader>("post_processing.glsl");
    std::vector<PostProcessingShader*> defaultEffects;
    defaultEffects.push_back(pps);

    renderer->postProcessor.setPostProcessingEffects(defaultEffects);
//    renderer->postProcessor.postProcessingEffects.push_back(renderer->postProcessingShader);

//    PostProcessingShader* bla = ShaderLoader::instance()->load<PostProcessingShader>("gaussian_blur.glsl");
//    renderer->postProcessor.postProcessingEffects.push_back(bla);
//    renderer->postProcessor.postProcessingEffects.push_back(bla);
//    renderer->postProcessor.postProcessingEffects.push_back(bla);
//    renderer->postProcessor.postProcessingEffects.push_back(bla);

    renderer->lighting.setRenderDebug( false);

    window->renderer = renderer;

    cout<<"========================== Framework initialization done! =========================="<<endl;
}

