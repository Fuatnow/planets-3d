#pragma once

#include "placinginterface.h"
#include "planetsuniverse.h"
#include "spheregenerator.h"
#include "grid.h"
#include <QElapsedTimer>
#include <QTimer>
#include <QDir>

class QMouseEvent;

#if QT_VERSION >= 0x050000
#include <QOpenGLFunctions>
#include <QOpenGLShader>
#else
#include <QGLFunctions>
#include <QGLShader>

/* All of these have the same API as the new 5.X versions. */
#define QOpenGLShader               QGLShader
#define QOpenGLShaderProgram        QGLShaderProgram
#define QOpenGLFunctions            QGLFunctions
#define initializeOpenGLFunctions   initializeGLFunctions
#endif

#include <QGLWidget>

class PlanetsWidget : public QGLWidget, public QOpenGLFunctions {
    Q_OBJECT
private:
    QOpenGLShaderProgram shaderTexture;
    int shaderTexture_cameraMatrix, shaderTexture_modelMatrix;

    QOpenGLShaderProgram shaderColor;
    int shaderColor_cameraMatrix, shaderColor_modelMatrix, shaderColor_color;

    const static int vertex, uv;

    Camera camera;

    /* If true paintGL will save a screenshot after rendering a frame. */
    bool doScreenshot;

    /* Total amount of frames drawn since the creation of the widget. */
    int frameCount;
    /* The desired refresh rate, in milliseconds. */
    int refreshRate;

    /* This timer updates the frame after the refresh interval has passed. */
    QTimer timer;
    /* Time since the start of the frame. */
    QElapsedTimer frameTime;
    /* Total amount of time the widget has been running. */
    QElapsedTimer totalTime;

    /* The position of the mouse cursor last mouse movement event. */
    QPoint lastMousePos;

    const static Sphere<64, 32> highResSphere;
    const static Sphere<32, 16> lowResSphere;
    const static Circle<64> circle;

    const static QColor trailColor;

public:
    PlanetsWidget(QWidget *parent = 0);

    PlanetsUniverse universe;

    PlacingInterface placing;

    Grid grid;

    float drawScale;
    bool drawPlanetTrails;
    bool drawPlanetColors;
    bool hidePlanets;

    QDir screenshotDir;

signals:
    void updateFPSStatusMessage(const QString &text);
    void updateAverageFPSStatusMessage(const QString &text);
    void updatePlanetCountMessage(const QString &text);

public slots:
    void beginInteractiveCreation(){ placing.beginInteractiveCreation(); }
    void enableFiringMode(bool enable){ placing.enableFiringMode(enable); }
    void beginOrbitalCreation(){ placing.beginOrbitalCreation(); }
    void takeScreenshot(){ doScreenshot = true; }

    void setGridRange(int value);

    void followNext() { camera.followNext(); }
    void followPrevious() { camera.followPrevious(); }
    void followSelection() { camera.followSelection(); }
    void clearFollow() { camera.clearFollow(); camera.position = glm::vec3(); }
    void followPlainAverage() { camera.followPlainAverage(); }
    void followWeightedAverage() { camera.followWeightedAverage(); }

protected:
    void initializeGL();
    void resizeGL(int width, int height);
    void paintGL();
    void mouseMoveEvent(QMouseEvent* e);
    void mousePressEvent(QMouseEvent* e);
    void mouseReleaseEvent(QMouseEvent* e);
    void mouseDoubleClickEvent(QMouseEvent* e);
    void wheelEvent(QWheelEvent* e);

    void drawPlanet(const Planet &planet);
    void drawPlanetColor(const Planet &planet, const QColor &color);
    void drawPlanetWireframe(const Planet &planet, const QColor &color = 0xff00ff00);
};
