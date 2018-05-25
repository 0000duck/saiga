/**
 * Copyright (c) 2017 Darius Rückert 
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once

#include <saiga/opengl/opengl.h>
#include <saiga/camera/camera.h>
#include <saiga/util/mouse.h>

#include <saiga/util/glm.h>


#include <array>

namespace Saiga {

template<typename camera_t>
class Controllable_Camera : public camera_t{
public:
    float movementSpeed = 5;
    float movementSpeedFast = 10;

    float rotationSpeed = 0.2f;

    enum Key{
        Forward = 0,
        Backward = 1,
        Left = 2,
        Right = 3,
        Fast = 4,
        Up = 5,
    };


    std::vector<int> keyboardmap;
    std::vector<int> mousemap;
    glm::ivec2 lastMousePos;
    int dragState = 0; // 0 = nothing, 1 = first button drag, 2 = second button drag
    vec3 rotationPoint = vec3(std::numeric_limits<float>::infinity(),0,0);

    std::array<bool,6> keyPressed {};

    bool input = true;
    bool mouseTurnLocal = false;


    Controllable_Camera(){}
    virtual ~Controllable_Camera(){}

    void update(float delta);
    void interpolate(float dt, float interpolation);

    void mouseRotate(float dx, float dy);
    void mouseRotateAroundPoint(float dx, float dy);
    void mouseRotateAroundPoint(float dx, float dy, vec3 point);
    void mouseRotateAroundPoint(float dx, float dy, vec3 point, vec3 up);

    void enableInput() { input = true; }
    void disableInput() { input = false; }

};

template<class camera_t>
void Controllable_Camera<camera_t>::mouseRotate(float dx, float dy)
{
    if(mouseTurnLocal)
        this->turnLocal(dx*rotationSpeed,dy*rotationSpeed);
    else
        this->turn(dx*rotationSpeed,dy*rotationSpeed);
    this->calculateModel();
    this->updateFromModel();
}


template<class camera_t>
void Controllable_Camera<camera_t>::mouseRotateAroundPoint(float dx, float dy)
{
    vec3 point;
    if(rotationPoint.x == std::numeric_limits<float>::infinity())
    {
        vec3 dir = vec3(this->getDirection());
        point = this->getPosition() - 10.0f * dir;

    }else
    {
        point = rotationPoint;
    }

    mouseRotateAroundPoint(dx,dy,point);
//    mouseRotateAroundPoint(dx,dy,point,vec3(0,1,0));
}


template<class camera_t>
void Controllable_Camera<camera_t>::mouseRotateAroundPoint(float dx, float dy, vec3 point)
{
#if 0
    vec2 relMovement(dx,dy);
    float angle = glm::length(relMovement);
    if(angle == 0)
        return;

    vec4 right = this->getRightVector();
    vec4 up = this->getUpVector();

    vec3 axis = -glm::normalize(vec3(right * relMovement.y + up * relMovement.x));
//        cout << angle << camera.position << endl;

    quat qrot = glm::angleAxis(glm::radians(angle),axis);
    this->rot = qrot * this->rot;
    this->position = vec4(qrot * (this->getPosition()-point),1);


    this->position = vec4(point + this->getPosition(),1);

    this->calculateModel();
    this->updateFromModel();
#else
       vec2 relMovement(dx,dy);
    float angle = glm::length(relMovement);
    if(angle == 0)
        return;

    vec4 right = this->getRightVector();
    vec4 up = this->getUpVector();

    vec3 axis = -glm::normalize(vec3(right * relMovement.y + up * relMovement.x));
//        cout << angle << camera.position << endl;

    quat qrot = glm::angleAxis(glm::radians(angle * 0.3f),axis);
    this->rot = qrot * this->rot;
    vec3 p = qrot * (vec3(this->position)-point);

    p += point;
    this->position = vec4(p,1);
//        camera.rotateAroundPoint(vec3(0),vec3(1,0,0),relMovement.y);
    this->calculateModel();
    this->updateFromModel();
#endif

}



template<class camera_t>
void Controllable_Camera<camera_t>::mouseRotateAroundPoint(float dx, float dy, vec3 point, vec3 up)
{
    vec2 relMovement(dx,dy);
    float angle = glm::length(relMovement);
    if(angle == 0)
        return;

    vec3 dir = normalize(point - this->getPosition());

    vec3 right = normalize(cross(dir,up));
//    up = normalize(cross(right,dir));

//    vec4 right = this->getRightVector();
//    vec4 up = this->getUpVector();

    vec3 axis = -glm::normalize(vec3(right * relMovement.y + up * relMovement.x));
//        cout << angle << camera.position << endl;

    quat qrot = glm::angleAxis(glm::radians(angle),axis);
    this->rot = qrot * this->rot;
    this->position = vec4(qrot * (this->getPosition()-point),1);


    this->position = vec4(point + this->getPosition(),1);

    this->calculateModel();
    this->updateFromModel();
}


template<class camera_t>
void Controllable_Camera<camera_t>::update(float delta)
{
    if(!input)
        return;
    int FORWARD = keyboard.getMappedKeyState(Forward,keyboardmap) - keyboard.getMappedKeyState(Backward,keyboardmap);
    int RIGHT = keyboard.getMappedKeyState(Right,keyboardmap) - keyboard.getMappedKeyState(Left,keyboardmap);

    float speed;
    if (keyboard.getMappedKeyState(Fast,keyboardmap)){
        speed = movementSpeedFast;
    } else {
        speed = movementSpeed;
    }

    vec3 trans = delta*speed*FORWARD*vec3(0,0,-1) + delta*speed*RIGHT*vec3(1,0,0);
    vec3 transg =  vec3(0,1,0) * (delta*speed*keyboard.getMappedKeyState(Up,keyboardmap));
    this->translateLocal(trans);
    this->translateGlobal(transg);
    this->calculateModel();
    this->updateFromModel();
}


template<class camera_t>
void Controllable_Camera<camera_t>::interpolate(float dt, float interpolation)
{
	//the camera isn't actually "interpolated" 
	//we just use the latest mouse position
	(void)dt; (void)interpolation;

	if(!input)
        return;

    int newDragState = mouse.getMappedKeyState(0,mousemap) ? 1 : mouse.getMappedKeyState(1,mousemap) ? 2 : 0;


	

    //only do mouse handling here
    glm::ivec2 mousedelta = lastMousePos - mouse.getPosition();
    lastMousePos = mouse.getPosition();

    if(dragState == 1)
    {
        this->mouseRotate(mousedelta.x,mousedelta.y);
    }else if(dragState == 2)
    {
        this->mouseRotateAroundPoint(mousedelta.x,mousedelta.y);
    }


    dragState = newDragState;

}

}