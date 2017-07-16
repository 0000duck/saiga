#include "saiga/assets/coloredAsset.h"

namespace Saiga {

void TexturedAsset::render(Camera *cam, const mat4 &model)
{
    auto tshader = std::static_pointer_cast<MVPTextureShader>(this->shader);
    tshader->bind();
    tshader->uploadModel(model);

    buffer.bind();
    for(TextureGroup& tg : groups){
        tshader->uploadTexture(tg.texture);

        int start = 0 ;
        start += tg.startIndex;
        buffer.draw(tg.indices, start);
    }
     buffer.unbind();



	 tshader->unbind();
}

void TexturedAsset::renderDepth(Camera *cam, const mat4 &model)
{
    auto dshader = std::static_pointer_cast<MVPTextureShader>(this->depthshader);

    dshader->bind();
    dshader->uploadModel(model);

    buffer.bind();
    for(TextureGroup& tg : groups){
        dshader->uploadTexture(tg.texture);

        int start = 0 ;
        start += tg.startIndex;
        buffer.draw(tg.indices, start);
    }
     buffer.unbind();



	 dshader->unbind();
}

}
