#include "saiga/camera/camera.h"

#define ANG2RAD 3.14159265358979323846/180.0

Camera::Camera()
{
}


void Camera::setView(const mat4 &v){
    view=v;
    recalculateMatrices();
    model = glm::inverse(view);

    this->position = vec3(model[3]);
    this->rot = glm::quat_cast(model);
}

void Camera::setView(const vec3 &eye,const vec3 &center,const vec3 &up){
    setView(glm::lookAt(eye,center,up));
}

void Camera::updateFromModel(){
    view = glm::inverse(model);
    recalculateMatrices();
}



//-------------------------------

Camera::IntersectionResult Camera::pointInFrustum(const vec3 &p) {

    for(int i=0; i < 6; i++) {
        if (planes[i].distance(p) < 0)
            return OUTSIDE;
    }
    return INSIDE;

}

Camera::IntersectionResult Camera::sphereInFrustum(const Sphere &s){
    IntersectionResult result = INSIDE;
    float distance;

    for(int i=0; i < 6; i++) {
        distance = planes[i].distance(s.pos);
        if (distance >= s.r){
            //            cout<<"outside of plane "<<i<<" "<<planes[i]<<endl;
            return OUTSIDE;
        }else if (distance > -s.r)
            result =  INTERSECT;
    }
    return result;
}

Camera::IntersectionResult Camera::pointInSphereFrustum(const vec3 &p) {

    if(boundingSphere.contains(p)){
        return INSIDE;
    }else{
        return OUTSIDE;
    }

}

Camera::IntersectionResult Camera::sphereInSphereFrustum(const Sphere &s){
    if(boundingSphere.intersect(s)){
        return INSIDE;
    }else{
        return OUTSIDE;
    }
}

int Camera::sideOfPlane(const Plane &plane)
{
    int positive = 0, negative = 0;
    for(int i = 0 ;i < 8 ; ++i){
        float t = plane.distance(vertices[i]);
        if(t>0) positive++; else if(t<0) negative++;
        if(positive && negative) return 0;
    }
    return (positive) ? 1 : -1;
}

vec2 Camera::projectedIntervall(const vec3 &d)
{
    vec2 ret(1000000,-1000000);
    for(int i = 0 ;i < 8 ; ++i){
        float t = glm::dot(d,vertices[i]);
        ret.x = glm::min(ret.x,t);
        ret.y = glm::max(ret.y,t);
    }
    return ret;
}

bool Camera::intersectSAT(Camera *other)
{
    //check planes of this camera
    for(int i = 0 ; i < 6 ; ++i){
        if(other->sideOfPlane(planes[i])>0){ // other is entirely on positive side
            return false;
        }
    }

    //check planes of other camera
    for(int i = 0 ; i < 6 ; ++i){
        if(this->sideOfPlane(other->planes[i])>0){ // this is entirely on positive side
            return false;
        }
    }


    //test cross product of pairs of edges, one from each polyhedron
    //since the overlap of the projected intervall is checked parallel edges doesn't have to be tested
    // -> 6 edges for each frustum
    for(int i = 0 ; i < 6 ; ++i){
        auto e1 = this->getEdge(i);
        for(int j = 0 ; j < 6 ; ++j){
            auto e2 = other->getEdge(j);
            vec3 d = glm::cross(e1.first-e1.second,e2.first-e2.second);

            vec2 i1 = this->projectedIntervall(d);
            vec2 i2 = other->projectedIntervall(d);

            if(i1.x>i2.y || i1.y<i2.x)
                return false;
        }
    }

    return true;
}

std::pair<vec3, vec3> Camera::getEdge(int i)
{
    switch(i){
    case 0:
        return std::pair<vec3, vec3>(vertices[0],vertices[4]);
    case 1:
        return std::pair<vec3, vec3>(vertices[1],vertices[5]);
    case 2:
        return std::pair<vec3, vec3>(vertices[2],vertices[6]);
    case 3:
        return std::pair<vec3, vec3>(vertices[3],vertices[7]);
    case 4:
        return std::pair<vec3, vec3>(vertices[0],vertices[1]);
    case 5:
        return std::pair<vec3, vec3>(vertices[0],vertices[2]);
    default:
        std::cerr<<"Camera::getEdge"<<endl;
        return std::pair<vec3, vec3>();
    }
}



std::ostream& operator<<(std::ostream& os, const Camera& ca){
    os<<ca.name;
    //    os<<"Nearplane= ("<<ca.nw*2<<" x "<<ca.nh*2<<") Farplane= ("<<ca.fw*2<<" x "<<ca.fh*2<<")";
    return os;
}



//===================================================================================================


void PerspectiveCamera::setProj(float fovy, float aspect, float zNear, float zFar){
    fovy = degreesToRadians(fovy);
    this->fovy = fovy;
    this->aspect = aspect;
    this->zNear = zNear;
    this->zFar = zFar;


    tang = (float)tan(fovy * 0.5) ;
    nh = zNear * tang;
    nw = nh * aspect;
    fh = zFar  * tang;
    fw = fh * aspect;

    proj = glm::perspective(fovy,aspect,zNear,zFar);
}

void PerspectiveCamera::recalculatePlanes()
{
    vec3 right = vec3(model[0]);
    vec3 up = vec3(model[1]);
    vec3 dir = -vec3(model[2]);

    vec3 nearplanepos = position + dir*zNear;
    vec3 farplanepos = position + dir*zFar;

    //near plane
    planes[0].set(nearplanepos,-dir);
    //far plane
    planes[1].set(farplanepos,dir);


    //calcuate 4 corners of nearplane
    vertices[0] = nearplanepos + nh * up - nw * right;
    vertices[1] = nearplanepos + nh * up + nw * right;
    vertices[2] = nearplanepos - nh * up - nw * right;
    vertices[3] = nearplanepos - nh * up + nw * right;
    //calcuate 4 corners of farplane
    vertices[4] = farplanepos + fh * up - fw * right;
    vertices[5] = farplanepos + fh * up + fw * right;
    vertices[6] = farplanepos - fh * up - fw * right;
    vertices[7] = farplanepos - fh * up + fw * right;

    //side planes
    planes[2].set(position,vertices[1],vertices[0]); //top
    planes[3].set(position,vertices[2],vertices[3]); //bottom
    planes[4].set(position,vertices[0],vertices[2]); //left
    planes[5].set(position,vertices[3],vertices[1]); //right


    //    vec3 fbr = farplanepos - fh * up + fw * right;
    vec3 fbr = farplanepos - fh * up;
    vec3 sphereMid = (nearplanepos+farplanepos)*0.5f;
    float r = glm::distance(fbr,sphereMid);

    boundingSphere.r = r;
    boundingSphere.pos = sphereMid;

    //    cout<<"recalculatePlanes"<<endl;
    //    cout<<zNear<<" "<<zFar<<endl;
    //    cout<<sphereMid<<" "<<fbr<<endl;
    //    cout<<r<<endl;

}

std::ostream& operator<<(std::ostream& os, const PerspectiveCamera& ca){
    os<<"Type: Perspective Camera\n";
    os<<"Name='"<<ca.name<<"' Fovy="<<ca.fovy<<" Aspect="<<ca.aspect<<" zNear="<<ca.zNear<<" zFar="<<ca.zFar<<"\n";
    os<<static_cast<const Camera&>(ca);
    return os;
}

//=========================================================================================================================

void OrthographicCamera::setProj( float left, float right,float bottom,float top,float near,  float far){
    this->left = left;
    this->right = right;
    this->bottom = bottom;
    this->top = top;
    this->zNear = near;
    this->zFar = far;

    nh = (top-bottom)/2;
    nw = (right-left)/2;

    fh = nh;
    fw = nw;
//    fh = (top-bottom)/2;
//    fw = (right-left)/2;
    proj = glm::ortho(left,right,bottom,top,near,far);
}

void OrthographicCamera::recalculatePlanes()
{
    vec3 right = vec3(model[0]);
    vec3 up = vec3(model[1]);
    vec3 dir = -vec3(model[2]);

    vec3 nearplanepos = position + dir*zNear;
    vec3 farplanepos = position + dir*zFar;

    //near plane
    planes[0].set(nearplanepos,-dir);
    //far plane
    planes[1].set(farplanepos,dir);


    //calcuate 4 corners of nearplane
    vertices[0] = nearplanepos + nh * up - nw * right;
    vertices[1] = nearplanepos + nh * up + nw * right;
    vertices[2] = nearplanepos - nh * up - nw * right;
    vertices[3] = nearplanepos - nh * up + nw * right;
    //calcuate 4 corners of farplane
    vertices[4] = farplanepos + fh * up - fw * right;
    vertices[5] = farplanepos + fh * up + fw * right;
    vertices[6] = farplanepos - fh * up - fw * right;
    vertices[7] = farplanepos - fh * up + fw * right;

    //side planes
    planes[2].set(position,vertices[1],vertices[0]); //top
    planes[3].set(position,vertices[2],vertices[3]); //bottom
    planes[4].set(position,vertices[0],vertices[2]); //left
    planes[5].set(position,vertices[3],vertices[1]); //right


    //    vec3 fbr = farplanepos - fh * up + fw * right;
    vec3 fbr = farplanepos - fh * up;
    vec3 sphereMid = (nearplanepos+farplanepos)*0.5f;
    float r = glm::distance(fbr,sphereMid);

    boundingSphere.r = r;
    boundingSphere.pos = sphereMid;
}



std::ostream& operator<<(std::ostream& os, const OrthographicCamera& ca){
    os<<"Type: Orthographic Camera";
    os<<"Name='"<<ca.name<<"' left="<<ca.left<<" right="<<ca.right<<" bottom="<<ca.bottom<<" top="<<ca.top<<" zNear="<<ca.zNear<<" zFar="<<ca.zFar<<"\n";
    os<<static_cast<const Camera&>(ca);
    return os;
}

