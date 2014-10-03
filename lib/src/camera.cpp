#include "camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(PlanetsUniverse &u) : universe(u) {
    reset();
}

void Camera::bound(){
    distance = glm::clamp(distance, 10.0f, 1.0e4f);
    xrotation = glm::clamp(xrotation, -90.0f, 90.0f);
    zrotation = glm::mod(zrotation, 360.0f);
}

void Camera::reset(){
    distance = 100.0f;
    xrotation = 45.0f;
    zrotation = 0.0f;
}

const glm::mat4 &Camera::setup(){
    switch(followingState){
    case WeightedAverage:
        position = glm::vec3();

        if(universe.size() != 0){
            float totalmass = 0.0f;

            for(const auto& i : universe){
                position += i.second.position * i.second.mass();
                totalmass += i.second.mass();
            }
            position /= totalmass;
        }
        break;
    case PlainAverage:
        position = glm::vec3();

        if(universe.size() != 0){
            for(const auto& i : universe){
                position += i.second.position;
            }
            position /= universe.size();
        }
        break;
    case Single:
        if(universe.isValid(following)){
            position = universe[following].position;
            break;
        }
    default:
        position = glm::vec3();
    }

    camera = glm::translate(projection, glm::vec3(0.0f, 0.0f, -distance));
    camera = glm::rotate(camera, xrotation - 90.0f, glm::vec3(1.0f, 0.0f, 0.0f));
    camera = glm::rotate(camera, zrotation, glm::vec3(0.0f, 0.0f, 1.0f));
    camera = glm::translate(camera, -position);
    return camera;
}

Ray Camera::getRay(const glm::ivec2 &pos, const int &windowW, const int &windowH, bool normalize, float startDepth, float endDepth) const {
    Ray ray;

    glm::mat4 model;
    glm::vec3 windowCoord(pos.x, windowH - pos.y, startDepth);
    glm::vec4 viewport(0.0f, 0.0f, windowW, windowH);

    ray.origin = glm::unProject(windowCoord, model, camera, viewport);

    windowCoord.z = endDepth;

    ray.direction = ray.origin - glm::unProject(windowCoord, model, camera, viewport);

    if(normalize){
        ray.direction = glm::normalize(ray.direction);
    }

    return ray;
}
void Camera::followNext(){
    if(!universe.isEmpty()){
        followingState = Single;
        PlanetsUniverse::const_iterator current = universe.find(following);

        if(current == universe.cend()){
            current = universe.cbegin();
        }else if(++current == universe.cend()){
            current = universe.cbegin();
        }

        following = current->first;
    }
}

void Camera::followPrevious(){
    if(!universe.isEmpty()){
        followingState = Single;
        PlanetsUniverse::const_iterator current = universe.find(following);

        if(current == universe.cend()){
            current = universe.cbegin();
        }else{
            if(current == universe.cbegin()){
                current = universe.cend();
            }
            --current;
        }

        following = current->first;
    }
}

void Camera::followSelection(){
    following = universe.selected;
    followingState = Single;
}

void Camera::clearFollow(){
    following = 0;
    followingState = FollowNone;
}

void Camera::followPlainAverage(){
    followingState = PlainAverage;
}

void Camera::followWeightedAverage(){
    followingState = WeightedAverage;
}
