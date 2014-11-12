#ifndef PLACINGINTERFACE_H
#define PLACINGINTERFACE_H

#include <planet.h>
#include <camera.h>

class PlacingInterface{
    PlanetsUniverse &universe;

public:
    enum PlacingStep{
        NotPlacing,
        FreePositionXY,
        FreePositionZ,
        FreeVelocity,
        Firing,
        OrbitalPlanet,
        OrbitalPlane
    };

    PlacingStep step;
    Planet planet;
    glm::mat4 rotation;
    float orbitalRadius;

    float firingSpeed;
    float firingMass;

    bool handleMouseMove(const glm::ivec2 &pos, const glm::ivec2 &delta, const int &windowW, const int &windowH, const Camera &camera, bool &holdMouse);
    bool handleMouseClick(const glm::ivec2 &pos, const int &windowW, const int &windowH, const Camera &camera);
    bool handleMouseWheel(float delta);

    /* Will change the camera position! */
    bool handleAnalogStick(const glm::vec2 &pos, Camera &camera, const int64_t &delay);

    void enableFiringMode(bool enable);
    void beginInteractiveCreation(){ step = FreePositionXY; universe.resetSelected(); }
    void beginOrbitalCreation(){ if(universe.isSelectedValid()) step = OrbitalPlanet; }

    PlacingInterface(PlanetsUniverse &universe);
};

#endif // PLACINGINTERFACE_H
