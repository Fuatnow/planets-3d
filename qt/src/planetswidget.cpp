#include "planetswidget.h"
#include <QDir>
#include <QMouseEvent>
#include <QOpenGLFramebufferObject>
#include <QApplication>
#include <limits>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

PlanetsWidget::PlanetsWidget(QWidget* parent) : QOpenGLWidget(parent), placing(universe), camera(universe),
    screenshotDir(QDir::homePath() + "/Pictures/Planets3D-Screenshots/"), highResSphereTris(QOpenGLBuffer::IndexBuffer),
    lowResSphereLines(QOpenGLBuffer::IndexBuffer), circleLines(QOpenGLBuffer::IndexBuffer) {
    /* We want mouse movement events. */
    setMouseTracking(true);

    /* Don't let people make the widget really small. */
    setMinimumSize(QSize(100, 100));

#ifdef PLANETS3D_QT_USE_SDL_GAMEPAD
    initSDL();
#endif
}

void PlanetsWidget::initializeGL() {
    initializeOpenGLFunctions();

    /* Load the shaders from the qrc. */
    shaderTexture.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/texture.vsh");
    shaderTexture.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/texture.fsh");

    /* Bind the attribute handles. */
    shaderTexture.bindAttributeLocation("vertex", vertex);
    shaderTexture.bindAttributeLocation("uv", uv);

    shaderTexture.link();

    /* Get the uniform values */
    shaderTexture_cameraMatrix = shaderTexture.uniformLocation("cameraMatrix");
    shaderTexture_modelMatrix = shaderTexture.uniformLocation("modelMatrix");

    /* Load the shaders from the qrc. */
    shaderColor.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/color.vsh");
    shaderColor.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/color.fsh");

    /* Bind the attribute handle. */
    shaderColor.bindAttributeLocation("vertex", vertex);

    shaderColor.link();

    /* Get the uniform values */
    shaderColor_cameraMatrix = shaderColor.uniformLocation("cameraMatrix");
    shaderColor_modelMatrix = shaderColor.uniformLocation("modelMatrix");
    shaderColor_color = shaderColor.uniformLocation("color");

    /* Clear to black with depth of 1.0 */
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepthf(1.0f);

    /* Set and enable depth test function. */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    /* Cull back faces. */
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    /* Enable alpha blending. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* If the platform has GL_LINE_SMOOTH use it. */
#ifdef GL_LINE_SMOOTH
    glEnable(GL_LINE_SMOOTH);
#endif

    /* This vertex attribute should always be enabled. */
    shaderColor.enableAttributeArray(vertex);

    /* The one and only texture. */
    QImage img(":/textures/planet.png");

    texture = new QOpenGLTexture(img);

    /* Begin vertex/index buffer allocation. */

    const static Sphere<64, 32> highResSphere;
    const static Sphere<32, 16> lowResSphere;
    const static Circle<64> circle;

    highResSphereVerts.create();
    highResSphereVerts.bind();
    highResSphereVerts.allocate(highResSphere.verts, highResSphere.vertexCount * sizeof(Vertex));

    highResSphereTris.create();
    highResSphereTris.bind();
    highResSphereTris.allocate(highResSphere.triangles, highResSphere.triangleCount * sizeof(unsigned int));
    highResSphereTriCount = highResSphere.triangleCount;

    lowResSphereVerts.create();
    lowResSphereVerts.bind();
    lowResSphereVerts.allocate(lowResSphere.verts, lowResSphere.vertexCount * sizeof(Vertex));

    lowResSphereLines.create();
    lowResSphereLines.bind();
    lowResSphereLines.allocate(lowResSphere.lines, lowResSphere.lineCount * sizeof(unsigned int));
    lowResSphereLineCount = lowResSphere.lineCount;

    circleVerts.create();
    circleVerts.bind();
    circleVerts.allocate(circle.verts, circle.vertexCount * sizeof(Vertex));

    circleLines.create();
    circleLines.bind();
    circleLines.allocate(circle.lines, circle.lineCount * sizeof(unsigned int));
    circleLineCount = circle.lineCount;

    QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
    QOpenGLBuffer::release(QOpenGLBuffer::IndexBuffer);

    /* End vertex/index buffer allocation. */

    /* If we haven't rendered any frames yet, start the timer. */
    if (frameCount == 0) {
        totalTime.start();
        frameTime.start();
    }
}

void PlanetsWidget::resizeGL(int width, int height) {
    glViewport(0, 0, width, height);

    camera.resizeViewport(width, height);
}

void PlanetsWidget::paintGL() {
    int delay = frameTime.nsecsElapsed() / 1000;
    frameTime.start();

#ifdef PLANETS3D_QT_USE_SDL_GAMEPAD
    pollGamepad();
    doControllerAxisInput(delay);
#endif

    /* Don't advance if placing. */
    if (placing.step == PlacingInterface::NotPlacing || placing.step == PlacingInterface::Firing)
        universe.advance(delay);

    render();

    update();

    emit updateAverageFPSStatusMessage(tr("average fps: %1").arg(++frameCount * 1.0e3f / totalTime.elapsed()));
    emit updateFPSStatusMessage(tr("fps: %1").arg(1.0e6f / delay));
}

void PlanetsWidget::render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    camera.setup();

    if (!hidePlanets) {
        /* Only used for drawing the planets. */
        shaderTexture.bind();

        /* Upload the updated camera matrix. */
        glUniformMatrix4fv(shaderTexture_cameraMatrix, 1, GL_FALSE, glm::value_ptr(camera.camera));

        shaderTexture.enableAttributeArray(uv);

        /* Our one and only texture, which apparently doesn't stay bound between frames... */
        texture->bind();

        highResSphereVerts.bind();
        highResSphereTris.bind();

        for (const auto& i : universe)
            drawPlanet(i.second);

        highResSphereVerts.release();
        highResSphereTris.release();

        /* That's the only thing that uses the uv coords. */
        shaderTexture.disableAttributeArray(uv);
    }

    /* Everything else uses the color shader. */
    shaderColor.bind();

    /* Upload the updated camera matrix. */
    glUniformMatrix4fv(shaderColor_cameraMatrix, 1, GL_FALSE, glm::value_ptr(camera.camera));

    lowResSphereVerts.bind();
    lowResSphereLines.bind();

    if(drawPlanetColors)
        for(const auto& i : universe)
            drawPlanetWireframe(i.second, i.first);
    else if(!hidePlanets && universe.isSelectedValid())
        drawPlanetWireframe(universe.getSelected());

    lowResSphereVerts.release();
    lowResSphereLines.release();

    if (drawPlanetTrails) {
        shaderColor.setUniformValue(shaderColor_modelMatrix, QMatrix4x4());
        shaderColor.setUniformValue(shaderColor_color, trailColor);

        for (const auto& i : universe) {
            shaderColor.setAttributeArray(vertex, GL_FLOAT, i.second.path.data(), 3);
            glDrawArrays(GL_LINE_STRIP, 0, GLsizei(i.second.path.size()));
        }
    }

    switch (placing.step) {
    case PlacingInterface::FreeVelocity: {
        float length = glm::length(placing.planet.velocity) / universe.velocityfac;

        if (length > 0.0f) {
            glm::mat4 matrix = glm::translate(placing.planet.position);
            matrix = glm::scale(matrix, glm::vec3(placing.planet.radius()));
            matrix *= placing.rotation;
            glUniformMatrix4fv(shaderColor_modelMatrix, 1, GL_FALSE, glm::value_ptr(matrix));
            shaderColor.setUniformValue(shaderColor_color, trailColor);

            /* A simple arrow. */
            float verts[] = {  0.1f, 0.1f, 0.0f,
                               0.1f,-0.1f, 0.0f,
                              -0.1f,-0.1f, 0.0f,
                              -0.1f, 0.1f, 0.0f,

                               0.1f, 0.1f, length,
                               0.1f,-0.1f, length,
                              -0.1f,-0.1f, length,
                              -0.1f, 0.1f, length,

                               0.2f, 0.2f, length,
                               0.2f,-0.2f, length,
                              -0.2f,-0.2f, length,
                              -0.2f, 0.2f, length,

                               0.0f, 0.0f, length + 0.4f };

            static const GLubyte indexes[] = {  0,  1,  2,       2,  3,  0,

                                                1,  0,  5,       4,  5,  0,
                                                2,  1,  6,       5,  6,  1,
                                                3,  2,  7,       6,  7,  2,
                                                0,  3,  4,       7,  4,  3,

                                                5,  4,  9,       8,  9,  4,
                                                6,  5, 10,       9, 10,  5,
                                                7,  6, 11,      10, 11,  6,
                                                4,  7,  8,      11,  8,  7,

                                                9,  8, 12,
                                               10,  9, 12,
                                               11, 10, 12,
                                                8, 11, 12 };

            shaderColor.setAttributeArray(vertex, GL_FLOAT, verts, 3);
            glDrawElements(GL_TRIANGLES, sizeof(indexes), GL_UNSIGNED_BYTE, indexes);
        }
    }
    case PlacingInterface::FreePositionXY:
    case PlacingInterface::FreePositionZ:
        lowResSphereVerts.bind();
        lowResSphereLines.bind();

        drawPlanetWireframe(placing.planet);

        lowResSphereVerts.release();
        lowResSphereLines.release();
        break;
    case PlacingInterface::OrbitalPlane:
    case PlacingInterface::OrbitalPlanet:
        if (universe.isSelectedValid() && placing.orbitalRadius > 0.0f) {
            glm::mat4 matrix = glm::translate(universe.getSelected().position);
            matrix = glm::scale(matrix, glm::vec3(placing.orbitalRadius));
            matrix *= placing.rotation;
            glUniformMatrix4fv(shaderColor_modelMatrix, 1, GL_FALSE, glm::value_ptr(matrix));
            shaderColor.setUniformValue(shaderColor_color, trailColor);

            circleVerts.bind();
            circleLines.bind();

            shaderColor.setAttributeBuffer(vertex, GL_FLOAT, 0, 3, sizeof(Vertex));
            glDrawElements(GL_LINES, circleLineCount, GL_UNSIGNED_INT, nullptr);

            circleVerts.release();
            circleLines.release();

            lowResSphereVerts.bind();
            lowResSphereLines.bind();

            drawPlanetWireframe(placing.planet);

            lowResSphereVerts.release();
            lowResSphereLines.release();
        }
        break;
    default: break;
    }

    if (grid.draw) {
        grid.update(camera);

        /* The grid doesn't write to the depth buffer. */
        glDepthMask(GL_FALSE);

        shaderColor.setAttributeArray(vertex, GL_FLOAT, grid.points.data(), 2);

        glm::vec4 color = grid.color;
        color.a *= grid.alphafac;

        QMatrix4x4 matrix;
        matrix.scale(grid.scale);
        shaderColor.setUniformValue(shaderColor_modelMatrix, matrix);
        glUniform4fv(shaderColor_color, 1, glm::value_ptr(color));

        glDrawArrays(GL_LINES, 0, GLsizei(grid.points.size()));

        matrix.scale(0.5f);
        shaderColor.setUniformValue(shaderColor_modelMatrix, matrix);
        color.a = grid.color.a - color.a;
        glUniform4fv(shaderColor_color, 1, glm::value_ptr(color));

        glDrawArrays(GL_LINES, 0, GLsizei(grid.points.size()));

        glDepthMask(GL_TRUE);
    }
}

void PlanetsWidget::takeScreenshot() {
    QElapsedTimer t;
    t.start();

    makeCurrent();

    QString filename = screenshotDir.absoluteFilePath("shot%1.png");
    int i = 0;
    while (QFile::exists(filename.arg(++i, 4, 10, QChar('0'))));
    filename = filename.arg(i, 4, 10, QChar('0'));

    QOpenGLFramebufferObjectFormat fmt;
    /* Skip the Alpha component. */
    fmt.setInternalTextureFormat(GL_RGB);
    /* We need a depth buffer. */
    fmt.setAttachment(QOpenGLFramebufferObject::Depth);

    /* Basically we just want as many as possible if they're supported. */
    if (context()->hasExtension("GL_EXT_framebuffer_multisample") && context()->hasExtension("GL_EXT_framebuffer_blit"))
        fmt.setSamples(64);

    /* TODO - It would be easy enough to make this support arbitrarily sized screenshots.
     * The camera and viewport would have to be resized though, and there would need to be UI for it... */
    QOpenGLFramebufferObject sample(this->size(), fmt);
    sample.bind();

    render();

    /* Get the image to save. */
    QImage img = sample.toImage();

    if (!img.isNull() && img.save(filename))
        statusBarMessage(("Screenshot saved to: \"" + filename + "\", operation took %1ms").arg(t.elapsed()) , 10000);

    doneCurrent();
}

void PlanetsWidget::mouseMoveEvent(QMouseEvent* e) {
    glm::ivec2 delta(lastMousePos.x() - e->x(), lastMousePos.y() - e->y());

    bool holdCursor = false;

    if (!placing.handleMouseMove(glm::ivec2(e->x(), e->y()), delta, camera, holdCursor)) {
        if (e->buttons().testFlag(Qt::MiddleButton)) {
            camera.distance -= delta.y * camera.distance * 1.0e-2f;
            setCursor(Qt::SizeVerCursor);
            camera.bound();
        } else if (e->buttons().testFlag(Qt::RightButton)) {
            camera.xrotation += delta.y * 0.01f;
            camera.zrotation += delta.x * 0.01f;

            camera.bound();

            holdCursor = true;
        }
    }
    if (holdCursor) {
        QCursor::setPos(mapToGlobal(lastMousePos));
        setCursor(Qt::BlankCursor);
    } else {
        lastMousePos = e->pos();
    }
}

void PlanetsWidget::mouseDoubleClickEvent(QMouseEvent* e){
    switch(e->button()){
    case Qt::LeftButton:
        /* Double clicking the left button while not placing sets or clears the planet currently being followed. */
        if (placing.step == PlacingInterface::NotPlacing) {
            if (universe.isSelectedValid()) {
                camera.followSelection();
            } else {
                camera.clearFollow();
                camera.position = glm::vec3();
            }
        }
        break;
    case Qt::MiddleButton:
    case Qt::RightButton:
        /* Double clicking the middle or right button resets the camera. */
        camera.reset();
        break;
    default: break;
    }
}

void PlanetsWidget::mousePressEvent(QMouseEvent* e){
    if(e->button() == Qt::LeftButton){
        glm::ivec2 pos(e->x(), e->y());

        /* Send click to placement system. If it doesn't use it and planets aren't hidden, select under the cursor. */
        if(!placing.handleMouseClick(pos, camera) && !hidePlanets)
            camera.selectUnder(pos);
    }
}

void PlanetsWidget::mouseReleaseEvent(QMouseEvent *e){
    setCursor(Qt::ArrowCursor);
}

void PlanetsWidget::wheelEvent(QWheelEvent* e){
    if (!placing.handleMouseWheel(e->delta() * 1.0e-3f)) {
        camera.distance -= e->delta() * camera.distance * 5.0e-4f;

        camera.bound();
    }
}

void PlanetsWidget::drawPlanet(const Planet& planet) {
    glm::mat4 matrix = glm::translate(planet.position);
    matrix = glm::scale(matrix, glm::vec3(planet.radius() * drawScale));
    glUniformMatrix4fv(shaderTexture_modelMatrix, 1, GL_FALSE, glm::value_ptr(matrix));

    shaderTexture.setAttributeBuffer(vertex, GL_FLOAT, 0,                     3, sizeof(Vertex));
    shaderTexture.setAttributeBuffer(uv,     GL_FLOAT, 3 * sizeof(glm::vec3), 2, sizeof(Vertex));
    glDrawElements(GL_TRIANGLES, highResSphereTriCount, GL_UNSIGNED_INT, nullptr);
}

void PlanetsWidget::drawPlanetWireframe(const Planet& planet, const QColor& color) {
    shaderColor.setUniformValue(shaderColor_color, color);

    glm::mat4 matrix = glm::translate(planet.position);
    matrix = glm::scale(matrix, glm::vec3(planet.radius() * drawScale * 1.05f));
    glUniformMatrix4fv(shaderColor_modelMatrix, 1, GL_FALSE, glm::value_ptr(matrix));

    shaderColor.setAttributeBuffer(vertex, GL_FLOAT, 0, 3, sizeof(Vertex));
    glDrawElements(GL_LINES, lowResSphereLineCount, GL_UNSIGNED_INT, nullptr);
}

#ifdef PLANETS3D_QT_USE_SDL_GAMEPAD

#include <SDL.h>

/* TODO - I should probably keep this code in one place somehow... */

const int16_t triggerDeadzone = 16;

void PlanetsWidget::initSDL() {
    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) == -1)
        qDebug("ERROR: Unable to init SDL! \"%s\"", SDL_GetError());

    SDL_GameControllerEventState(SDL_ENABLE);
}

void PlanetsWidget::pollGamepad() {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_CONTROLLERDEVICEADDED:
            /* This will allow us to recieve events from the controller. */
            SDL_GameControllerOpen(event.cdevice.which);
            break;
        case SDL_CONTROLLERBUTTONUP:
            /* If we haven't picked a controller yet, use this one. */
            if (controller == nullptr || event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE)
                controller = SDL_GameControllerOpen(event.cbutton.which);

            /* Ignore events from other controllers. */
            if (event.cbutton.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)))
                doControllerButtonPress(event.cbutton.button);

            break;
        }
    }
}

void PlanetsWidget::doControllerButtonPress(const uint8_t& button) {
    /* When simulating mouse events we use the center of the screen (in pixels). */
    glm::ivec2 centerScreen(width() / 2, height() / 2);

    switch(button) {
    case SDL_CONTROLLER_BUTTON_BACK:
        QApplication::closeAllWindows();
        break;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        camera.reset();
        break;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
        camera.position = glm::vec3();
        break;
    case SDL_CONTROLLER_BUTTON_A:
        /* TODO - there should probably be a seperate function for this... */
        if (!placing.handleMouseClick(centerScreen, camera))
            camera.selectUnder(centerScreen);
        break;
    case SDL_CONTROLLER_BUTTON_X:
        universe.deleteSelected();
        break;
    case SDL_CONTROLLER_BUTTON_Y:
        if (universe.isSelectedValid())
            placing.beginOrbitalCreation();
        else
            placing.beginInteractiveCreation();
        break;
    case SDL_CONTROLLER_BUTTON_B:
        /* If trigger is not being held down pause/resume. */
        if (speedTriggerLast < triggerDeadzone)
            /* TODO - This messes up the interface... */
            universe.simspeed = universe.simspeed <= 0.0f ? 1.0f : 0.0f;

        /* If the trigger is being held down lock to current speed. */
        speedTriggerInUse = false;
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        camera.followPrevious();
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        camera.followPrevious();
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        camera.clearFollow();
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        if (camera.followingState == Camera::WeightedAverage)
            camera.followPlainAverage();
        else
            camera.followWeightedAverage();
        break;
    }
}

void PlanetsWidget::doControllerAxisInput(int64_t delay) {
    /* TODO - lots of magic numbers in this function... */
    if (controller != nullptr) {
        /* We only need it as a float, might as well not call and convert it repeatedly... */
        const float int16_max = std::numeric_limits<Sint16>::max();
        /* As we compare to length2 we need to use the square of Sint16's maximum value. */
        const float stickDeadzone = 0.1f * int16_max * int16_max;

        /* Multiply analog stick value by this to map from int16_max to delay converted to seconds. */
        float stickFac = delay * 1.0e-6f / int16_max;

        glm::vec2 right(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX),
                        SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY));

        if (glm::length2(right) > stickDeadzone) {
            /* Map values to the proper range. */
            right *= stickFac;

            bool rsMod = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;

            if (rsMod) {
                camera.distance += right.y * camera.distance;
            } else {
                camera.xrotation += right.y;
                camera.zrotation += right.x;
            }
        }

        camera.bound();

        glm::vec2 left(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX),
                       SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY));

        if (glm::length2(left) > stickDeadzone) {
            /* Map values to the proper range. */
            left *= stickFac;

            bool lsMod = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;

            if (!placing.handleAnalogStick(left, lsMod, camera)) {
                /* If the camera is following something stick is used only for zoom. */
                if (lsMod || camera.followingState != Camera::FollowNone)
                    camera.distance += left.y * camera.distance;
                else
                    camera.position += glm::vec3(glm::vec4(left.x, 0.0f, -left.y, 0.0f) * camera.camera) * camera.distance;
            }
        }

        int16_t speedTriggerCurrent = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

        /* If the trigger has gone from disengaged (< deadzone) to engaged (> deadzone) we enable using it as speed input. */
        if (speedTriggerInUse || (speedTriggerCurrent > triggerDeadzone && speedTriggerLast <= triggerDeadzone)) {
            universe.simspeed = float(speedTriggerCurrent * 8) / int16_max;
            universe.simspeed *= universe.simspeed;

            speedTriggerInUse = true;
        }

        speedTriggerLast = speedTriggerCurrent;
    }
}
#endif

const QColor PlanetsWidget::trailColor = QColor(0xcc, 0xff, 0xff, 0xff);

const int PlanetsWidget::vertex = 0;
const int PlanetsWidget::uv     = 1;
