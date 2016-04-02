
#include "saiga/opengl/texture/texture.h"



void basic_Texture_2D::setDefaultParameters(){
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(GL_LINEAR));
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(GL_LINEAR));
    glTexParameteri(target, GL_TEXTURE_WRAP_S, static_cast<GLint>(GL_CLAMP_TO_EDGE));
    glTexParameteri(target, GL_TEXTURE_WRAP_T,static_cast<GLint>( GL_CLAMP_TO_EDGE));
}

bool basic_Texture_2D::fromImage(Image &img){


    setFormat(img);

    createGlTexture();
    uploadData(img.data);
    return true;
}

//====================================================================================


void multisampled_Texture_2D::setDefaultParameters(){
//    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(GL_LINEAR));
//    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(GL_LINEAR));
//    glTexParameteri(target, GL_TEXTURE_WRAP_S, static_cast<GLint>(GL_CLAMP_TO_EDGE));
//    glTexParameteri(target, GL_TEXTURE_WRAP_T,static_cast<GLint>( GL_CLAMP_TO_EDGE));
}


void multisampled_Texture_2D::uploadData(GLubyte* data ){
    bind();
    glTexImage2DMultisample(target,
                            samples,
                            internal_format,
                            width,
                            height,
                            GL_TRUE //fixedsamplelocations
                            );
    unbind();
}
