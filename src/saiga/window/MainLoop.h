﻿/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once

#include <thread>

#include "saiga/config.h"
#include "saiga/util/semaphore.h"
#include "saiga/time/gameTime.h"


namespace Saiga {



struct SAIGA_GLOBAL MainLoopParameters
{
    /**
     * @brief startMainLoop
     * @param updatesPerSecond
     *      Number of calls per second to the virtual function "update".
     *      A value of 0 means: update as fast as possible (not recommended)
     * @param framesPerSecond
     *      Number of class per second to the render functions.
     *      A value of 0 is unlimitted frames.
     * @param mainLoopInfoTime
     *      Time between mainloop debug output to the console
     * @param maxFrameSkip
     *      Max number of frames that are skipped if the update cannot keep up.
     * @param _parallelUpdate
     *      Enables parallel updates while rendering. This will call the virtual function parallelUpdate.
     * @param _catchUp
     *      Lets the update loop catch up in case of lags.
     * @param _printInfoMsg
     *      Enable/Disable the debug output
     */
    int   updatesPerSecond      = 60;
    int   framesPerSecond       = 60;
    float mainLoopInfoTime      = 5.0f;
    int   maxFrameSkip          = 0;
    bool  parallelUpdate        = false;
    bool  catchUp               = false;
    bool  printInfoMsg          = true;

    /**
     *  Reads all paramters from the given config file.
     *  Creates the file with the default values if it doesn't exist.
     */
    void fromConfigFile(const std::string& file);
};

struct SAIGA_GLOBAL MainLoopInterface
{
    virtual void render() = 0;
    virtual void swap() = 0;
    virtual float getTotalRenderTime() { return 0; }
    virtual bool shouldClose() { return false; }
    virtual void update(float dt) {}
    virtual void parallelUpdate(float dt) { (void)dt; }
    virtual void interpolate(float dt, float alpha) {}
};



class SAIGA_GLOBAL MainLoop
{
public:
    MainLoopInterface& renderer;
    MainLoopInterface& updating;

    MainLoop(MainLoopInterface& renderer);
    void startMainLoop(MainLoopParameters params = MainLoopParameters());


    void renderImGuiInline();
public:

    //total number of updateticks/frames rendered so far
    int numUpdates = 0;
    int numFrames = 0;

    //game loop running
    bool running = false;

    //basic variables for the parallel update
    Semaphore semStartUpdate, semFinishUpdate;
    std::thread updateThread;
    bool parallelUpdate = false;


    tick_t gameLoopDelay = tick_t(0);

    bool gameloopDropAccumulatedUpdates = false;
    bool printInfoMsg = true;

    //for imgui graph
    bool showImgui = true;
    static const int numGraphValues = 80;
    float ut=0, ft=0;
    float avFt = 0, avUt;
    int imCurrentIndexUpdate = 0;
    int imCurrentIndexRender = 0;
    float imUpdateTimes[numGraphValues];
    float imRenderTimes[numGraphValues];
    bool showImguiDemo = false;
    float maxUpdateTime = 1;
    float maxRenderTime = 1;
    int targetUps = 60;


    ExponentialTimer updateTimer, interpolationTimer, renderCPUTimer, swapBuffersTimer;
    AverageTimer fpsTimer, upsTimer;

    void updateRenderGraph();
    void updateUpdateGraph();


    void update(float dt);
    void render(float dt, float interpolation);
    void startParallelUpdate(float dt);
    void parallelUpdateCaller(float dt);
    void endParallelUpdate();
    void parallelUpdateThread(float dt);


    void sleep(tick_t ticks);

};

}
