/**
 * Copyright (c) 2017 Darius Rückert 
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "saiga/rendering/renderer.h"


namespace Saiga {

Renderer::Renderer()
{

}

Renderer::~Renderer()
{

}

void Renderer::setRenderObject(Rendering &r)
{
    rendering = &r;
}


}
