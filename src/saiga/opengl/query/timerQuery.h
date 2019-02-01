/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once

#include "saiga/opengl/opengl.h"

namespace Saiga
{
/**
 * Asynchronous OpenGL GPU timer.
 *
 * Meassuers the time of the OpenGL calls between startTimer and stopTimer.
 * These calls do not empty the GPU command queue and return immediately.
 *
 * startTimer and stopTimer can only be called once per frame.
 * startTimer and stopTimer from multiple GPUTimers CAN NOT be interleaved. Use GPUTimer instead.
 *
 * Core since version 	3.3
 *
 */
class SAIGA_OPENGL_API TimerQuery
{
   private:
    GLuint queryBackBuffer = 0, queryFrontBuffer = 0;

    GLuint64 time;

    void swapQueries();

   public:
    TimerQuery();
    ~TimerQuery();

    /**
     * Creates the underlying OpenGL objects.
     */
    void create();

    void startTimer();
    void stopTimer();

    float getTimeMS();
    GLuint64 getTimeNS();
};

}  // namespace Saiga
