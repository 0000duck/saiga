#pragma once

#include "saiga/config.h"
#include "saiga/util/glm.h"

#include <vector>
#include <memory>

class TextShader;
class Text;
class Camera;

class SAIGA_GLOBAL TextOverlay3D {

public:

    struct TextContainer{
        std::shared_ptr<Text> text;
        float duration = 0.f;
        bool orientToCamera = true;
        vec3 velocity = vec3(0);

        TextContainer(){}
        TextContainer(std::shared_ptr<Text> text, float duration, bool orientToCamera)
            : text(text), duration(duration), orientToCamera(orientToCamera){}

        bool update(float delta);

    };

    TextShader* textShader;
    //text + duration
    std::vector<TextContainer> texts;

    static const float INFINITE_DURATION;


    TextOverlay3D();
    void render(Camera* cam);
    void renderText(Camera *cam);

    //text stuff
    void addText(std::shared_ptr<Text> text, float duration, bool orientToCamera = true);
    void addText(const TextContainer& tc);


    void update(float secondsPerTick);


    void setTextShader(TextShader* textShader);
};


