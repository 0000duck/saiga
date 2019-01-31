/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "animatedAsset.h"

#include "saiga/opengl/animation/boneShader.h"

namespace Saiga
{
void AnimatedAsset::render(Camera* cam, const mat4& model, UniformBuffer& boneMatrices)
{
    std::shared_ptr<BoneShader> bs = std::static_pointer_cast<BoneShader>(this->shader);


    bs->bind();
    bs->uploadModel(model);
    //    boneMatrices.bind(0);
    boneMatrices.bind(BONE_MATRICES_BINDING_POINT);
    buffer.bindAndDraw();
    bs->unbind();
}

void AnimatedAsset::renderDepth(Camera* cam, const mat4& model, UniformBuffer& boneMatrices)
{
    std::shared_ptr<BoneShader> bs = std::static_pointer_cast<BoneShader>(this->depthshader);

    bs->bind();
    bs->uploadModel(model);
    boneMatrices.bind(BONE_MATRICES_BINDING_POINT);
    buffer.bindAndDraw();
    bs->unbind();
}

}  // namespace Saiga
