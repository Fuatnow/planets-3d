#ifndef CAMERA_H
#define CAMERA_H

#include "common.h"

class Camera {
public:
    Camera();

    glm::vec3 position;
    float distance,xrotation,zrotation;

    void setup();
};

#endif // CAMERA_H
