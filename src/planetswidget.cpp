#include "planetswidget.h"
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/norm.hpp>

PlanetsWidget::PlanetsWidget(QWidget* parent) : QGLWidget(QGLFormat(QGL::AccumBuffer | QGL::SampleBuffers), parent) {
    this->setMouseTracking(true);

    this->doScreenshot = false;

#ifndef NDEBUG
    framerate = 60000;
#else
    framerate = 60;
#endif

    framecount = 0;
    placingStep = None;
    delay = 0;
    simspeed = 1.0;
    stepsPerFrame = 100;
    totalTime = QTime::currentTime();
    frameTime = QTime::currentTime();

    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateGL()));

    placing.position = glm::vec3(0.0f);
    placing.velocity = glm::vec3(0.0f, velocityfac, 0.0f);
    placing.mass = 100.0f;

    displaysettings = 000;

    gridRange = 50;
    gridColor = glm::vec4(0.8f, 1.0f, 1.0f, 0.4f);

    selected = NULL;
    following = NULL;
    load("default.xml");
}

PlanetsWidget::~PlanetsWidget() {
    this->deleteAll();
    qDebug()<< "average fps: " << framecount/(totalTime.msecsTo(QTime::currentTime()) * 0.001f);
}

void PlanetsWidget::initializeGL() {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearAccum(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0f);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glEnable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);

    glEnableClientState(GL_VERTEX_ARRAY);

    QImage img(":/textures/planet.png");
    texture = bindTexture(img);
}

void PlanetsWidget::resizeGL(int width, int height) {
    if (height == 0)
        height = 1;

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glLoadMatrixf(glm::value_ptr(glm::perspective(45.0f, float(width)/float(height), 0.1f, 10000.0f)));

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void PlanetsWidget::paintGL() {
    float time = 0.0f;
    if(placingStep == None){
        time = simspeed * delay * 20.0f / stepsPerFrame;

        for(int s = 0; s < stepsPerFrame; s++){
            QMutableListIterator<Planet> i(planets);
            while (i.hasNext()) {
                Planet &planet = i.next();
                QMutableListIterator<Planet> o(i);
                while (o.hasNext()) {
                    Planet &other = o.next();

                    if(other == planet){
                        continue;
                    }
                    else{
                        glm::vec3 direction = other.position - planet.position;
                        float distance = glm::length2(direction);
                        float frc = gravityconst * ((other.mass * planet.mass) / distance);

                        planet.velocity += direction * frc * time / planet.mass;
                        other.velocity -= direction * frc * time / other.mass;

                        distance = sqrt(distance);

                        if(distance < planet.getRadius() + other.getRadius() / 2.0f){
                            planet.position = (other.position * other.mass + planet.position * planet.mass) / (other.mass + planet.mass);
                            planet.velocity = (other.velocity * other.mass + planet.velocity * planet.mass) / (other.mass + planet.mass);
                            planet.mass += other.mass;
                            if(&other == selected){
                                selected = &planet;
                            }
                            o.remove();
                            planet.path.clear();
                        }
                    }
                }

                planet.position += planet.velocity * time;
                planet.updatePath();
            }
        }
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if(displaysettings & MotionBlur){
        glAccum(GL_RETURN, 1.0f);
        glClear(GL_ACCUM_BUFFER_BIT);
    }

    glMatrixMode(GL_MODELVIEW_MATRIX);
    glLoadIdentity();

    if(followState == Single && following != NULL){
        camera.position = following->position;
    }else if(followState == WeightedAverage){
        camera.position = glm::vec3(0.0f);
        float totalmass = 0.0f;

        for(QMutableListIterator<Planet> i(planets); i.hasNext();) {
            Planet &planet = i.next();
            camera.position += planet.position * planet.mass;
            totalmass += planet.mass;
        }
        camera.position /= totalmass;
    }else if(followState == PlainAverage){
        camera.position = glm::vec3(0.0f);

        for(QMutableListIterator<Planet> i(planets); i.hasNext();) {
            camera.position += i.next().position;
        }
        camera.position /= planets.size();
    }
    else{
        camera.position = glm::vec3(0.0f);
    }

    camera.setup();

    glEnable(GL_TEXTURE_2D);

    for(QMutableListIterator<Planet> i(planets); i.hasNext();) {
        i.next().draw();
    }

    glDisable(GL_TEXTURE_2D);

    if(displaysettings & MotionBlur){
        glAccum(GL_ADD, -0.002f * delay);
        glAccum(GL_ACCUM, 0.999f);
    }

    if(selected){
        selected->drawBounds();
    }

    if(displaysettings & PlanetTrails){
        for(QMutableListIterator<Planet> i(planets); i.hasNext();) {
            i.next().drawPath();
        }
    }

    if(placingStep != None){
        placing.drawBounds();
    }

    if(placingStep == FreeVelocity){
        float length = glm::length(placing.velocity) / velocityfac;

        if(length > 0.0f){
            glPushMatrix();
            glTranslatef(placing.position.x, placing.position.y, placing.position.z);
            glMultMatrixf(glm::value_ptr(placingRotation));

            GLUquadric *cylinder = gluNewQuadric();
            gluQuadricOrientation(cylinder, GLU_OUTSIDE);
            gluCylinder(cylinder, 0.1f, 0.1f, length, 64, 1);

            GLUquadric *cap = gluNewQuadric();
            gluQuadricOrientation(cap, GLU_INSIDE);
            gluDisk(cap, 0.0f, 0.1f, 64, 1);
            glTranslatef(0.0f, 0.0f, length);
            gluCylinder(cylinder, 0.2f, 0.0f, 0.4f, 64, 1);

            gluDisk(cap, 0.1f, 0.2f, 64, 1);
            glPopMatrix();
        }

    }

    float scale = pow(4, floor(log10(camera.distance)));

    glScalef(scale, scale, scale);

    if(displaysettings & SolidLineGrid){
        if(gridPoints.size() != (gridRange * 2 + 1) * 4){
            gridPoints.clear();
            for(int i = -gridRange; i <= gridRange; i++){
                gridPoints.push_back(glm::vec2(i,-gridRange));
                gridPoints.push_back(glm::vec2(i, gridRange));

                gridPoints.push_back(glm::vec2(-gridRange, i));
                gridPoints.push_back(glm::vec2( gridRange, i));
            }
        }

        glColor4fv(glm::value_ptr(gridColor));

        glVertexPointer(2, GL_FLOAT, 0, &gridPoints[0]);
        glDrawArrays(GL_LINES, 0, gridPoints.size());

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
    if(displaysettings & PointGrid){
        if(gridPoints.size() != pow(gridRange * 2 + 1, 2)){
            gridPoints.clear();
            for(int x = -gridRange; x <= gridRange; x++){
                for(int y = -gridRange; y <= gridRange; y++){
                    gridPoints.push_back(glm::vec2(x, y));
                }
            }
        }

        glColor4fv(glm::value_ptr(gridColor));

        glVertexPointer(2, GL_FLOAT, 0, &gridPoints[0]);
        glDrawArrays(GL_POINTS, 0, gridPoints.size());

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    if(this->doScreenshot){
        QDir dir = QDir::homePath() + "/Pictures/Planets3D Screenshots/";
        if(!dir.exists()){
            dir.mkpath(dir.absolutePath());
        }
        QString filename = dir.path() + "/shot%1.png";
        int i = 1;
        while(QFile::exists(filename.arg(i, 4, 10, QChar('0')))){
            i++;
        }
        filename = filename.arg(i, 4, 10, QChar('0'));
        qDebug() << "Screenshot saved to: "<< filename;

        QImage img = this->grabFrameBuffer();
        img.save(filename);

        this->doScreenshot = false;
    }

    delay = qMax(frameTime.msecsTo(QTime::currentTime()), 1);
    frameTime = QTime::currentTime();

    framecount++;

    // TODO - only update the simspeed label when simspeed has changed.
    emit updateSimspeedStatusMessage(tr("simulation speed: %1").arg(simspeed));
    emit updateAverageFPSStatusMessage(tr("average fps: %1").arg(framecount / (totalTime.msecsTo(QTime::currentTime()) * 0.001f)));
    emit updateFPSStatusMessage(tr("fps: %1").arg(1000.0f / delay));

    timer->start(qMax(0, (1000 / framerate) - delay));
}

void PlanetsWidget::mouseMoveEvent(QMouseEvent* e){
    if(placingStep == FreePositionXY){
        // set placing XY position based on grid
        glClear(GL_DEPTH_BUFFER_BIT);

        glColorMask(0, 0, 0, 0);
        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_MODELVIEW_MATRIX);
        glLoadIdentity();
        camera.setup();

        static const float plane[] = { 10.0f, 10.0f, 0.0f, 1.0e-5f,
                                       10.0f,-10.0f, 0.0f, 1.0e-5f,
                                      -10.0f,-10.0f, 0.0f, 1.0e-5f,
                                      -10.0f, 10.0f, 0.0f, 1.0e-5f};

        glVertexPointer(4, GL_FLOAT, 0, plane);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        glm::ivec4 viewport;
        glm::mat4 modelview,projection;

        glGetIntegerv(GL_VIEWPORT, glm::value_ptr(viewport));
        glGetFloatv(GL_MODELVIEW_MATRIX, glm::value_ptr(modelview));
        glGetFloatv(GL_PROJECTION_MATRIX, glm::value_ptr(projection));

        glm::vec3 windowCoord(e->x(),viewport[3]-e->y(),0);

        glReadPixels(windowCoord.x, windowCoord.y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &windowCoord.z);

        placing.position = glm::unProject(windowCoord,modelview,projection,viewport);

        glEnable(GL_CULL_FACE);
        glColorMask(1, 1, 1, 1);

        this->lastmousepos = e->pos();
    }
    else if(placingStep == FreePositionZ){
        // set placing Z position
        placing.position.z += (lastmousepos.y() - e->y()) / 10.0f;
        this->lastmousepos = e->pos();
    }
    else if(placingStep == FreeVelocity){
        // set placing velocity
        float xdelta = (lastmousepos.x() - e->x()) / 20.0f;
        float ydelta = (lastmousepos.y() - e->y()) / 20.0f;
        placingRotation *= glm::rotate(xdelta, 1.0f, 0.0f, 0.0f);
        placingRotation *= glm::rotate(ydelta, 0.0f, 1.0f, 0.0f);
        placing.velocity = glm::vec3(placingRotation * glm::vec4(0.0f, 0.0f, glm::length(placing.velocity), 0.0f));
        QCursor::setPos(this->mapToGlobal(this->lastmousepos));
    }
    else if(e->buttons().testFlag(Qt::MiddleButton)){
        camera.xrotation += ((150.0f * (lastmousepos.y() - e->y())) / this->height());
        camera.zrotation += ((300.0f * (lastmousepos.x() - e->x())) / this->width());
        QCursor::setPos(this->mapToGlobal(this->lastmousepos));

        camera.xrotation = glm::min(glm::max(camera.xrotation, -90.0f), 90.0f);
        camera.zrotation = fmod(camera.zrotation, 360.0f);

        this->setCursor(Qt::SizeAllCursor);
    }
    else{
        this->lastmousepos = e->pos();
    }
}

void PlanetsWidget::mouseDoubleClickEvent(QMouseEvent* e){
    if(e->button() == Qt::MiddleButton){
        camera.distance = 10.0f;
        camera.xrotation = 45.0f;
        camera.zrotation = 0.0f;
    }
}

void PlanetsWidget::mousePressEvent(QMouseEvent* e){
    if(placingStep == FreePositionXY){
        if(e->button() == Qt::LeftButton){
            placingStep = FreePositionZ;
            setCursor(QCursor(Qt::BlankCursor));
        }
    }
    else if(placingStep == FreePositionZ){
        if(e->button() == Qt::LeftButton){
            placingStep = FreeVelocity;
        }
    }
    else if(placingStep == FreeVelocity){
        if(e->button() == Qt::LeftButton){
            placingStep = None;
            selected = &createPlanet(placing.position, placing.velocity, placing.mass);
            this->setCursor(Qt::ArrowCursor);
        }
    }
    else if(e->button() == Qt::LeftButton){
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW_MATRIX);
        glLoadIdentity();
        camera.setup();

        for(QMutableListIterator<Planet> i(planets); i.hasNext();) {
            i.next().drawBounds(GLU_FILL, true);
        }

        glm::vec4 color;
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);

        glReadPixels(e->x(), viewport[3] - e->y(), 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(color));

        selected = NULL;

        if(color.a == 0){
            return;
        }
        QColor selectedcolor = QColor::fromRgbF(color.r,color.g,color.b);

        for(QMutableListIterator<Planet> i(planets); i.hasNext();) {
            Planet *planet = &i.next();
            if(planet->selectionColor == selectedcolor){
                this->selected = planet;
            }
        }
    }
}

void PlanetsWidget::mouseReleaseEvent(QMouseEvent *e){
    if(e->button() == Qt::MiddleButton){
        this->setCursor(Qt::ArrowCursor);
    }
}

void PlanetsWidget::wheelEvent(QWheelEvent* e){
    if(placingStep == FreePositionXY || placingStep == FreePositionZ){
        placing.mass += e->delta()*(placing.mass * 1.0e-3f);
        glm::max(placing.mass, 0.1f);
    }
    else if(placingStep == FreeVelocity){
        placing.velocity = glm::vec3(placingRotation * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) * glm::max(0.0f, glm::length(placing.velocity) + (e->delta() * velocityfac * 1.0e-3f)));
    }
    else {
        camera.distance -= e->delta() * camera.distance * 0.0005f;

        camera.distance = glm::max(camera.distance, 0.1f);
        camera.distance = glm::min(camera.distance, 1000.0f);
    }
}

Planet &PlanetsWidget::createPlanet(glm::vec3 position, glm::vec3 velocity, float mass){
    Planet planet;
    planet.position = position;
    planet.velocity = velocity;
    planet.mass = mass;
    planets.append(planet);
    return planets.back();
}

void PlanetsWidget::deleteAll(){
    QMutableListIterator<Planet> i(planets);
    while (i.hasNext()) {
        i.next();
        i.remove();
    }
    selected = NULL;
}

void PlanetsWidget::centerAll(){
    QMutableListIterator<Planet> i(planets);
    glm::vec3 averagePosition(0.0f),averageVelocity(0.0f);
    float totalmass = 0.0f;
    while (i.hasNext()) {
        Planet &planet = i.next();

        averagePosition += planet.position * planet.mass;
        averageVelocity += planet.velocity * planet.mass;
        totalmass += planet.mass;
    }
    averagePosition /= totalmass;
    averageVelocity /= totalmass;

    i.toFront();
    while (i.hasNext()) {
        Planet &planet = i.next();

        planet.position -= averagePosition;
        planet.velocity -= averageVelocity;
        planet.path.clear();
    }
}

void PlanetsWidget::beginInteractiveCreation(){
    placingStep = FreePositionXY;
    selected = NULL;
}

bool PlanetsWidget::load(const QString &filename){
    if(!QFile::exists(filename)){
        qDebug(qPrintable(tr("\"%1\" does not exist!").arg(filename)));
        return false;
    }
    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        return false;
    }

    QXmlStreamReader xml(&file);

    if(xml.readNextStartElement()) {
        if (xml.name() == "planets-3d-universe"){
            deleteAll();
            while(xml.readNextStartElement()) {
                if(xml.name() == "planet"){
                    Planet planet;

                    planet.mass = xml.attributes().value("mass").toString().toFloat();

                    while(xml.readNextStartElement()){
                        if(xml.name() == "position"){
                            planet.position.x = xml.attributes().value("x").toString().toFloat();
                            planet.position.y = xml.attributes().value("y").toString().toFloat();
                            planet.position.z = xml.attributes().value("z").toString().toFloat();
                            xml.readNext();
                        }
                        if(xml.name() == "velocity"){
                            planet.velocity.x = xml.attributes().value("x").toString().toFloat() * velocityfac;
                            planet.velocity.y = xml.attributes().value("y").toString().toFloat() * velocityfac;
                            planet.velocity.z = xml.attributes().value("z").toString().toFloat() * velocityfac;
                            xml.readNext();
                        }
                    }
                    planets.append(planet);
                    xml.readNext();
                }
            }
            if(xml.hasError()){
                qDebug(qPrintable(tr("\"%1\" had error: %2").arg(filename).arg(xml.errorString())));
                return false;
            }
        }

        else{
            qDebug(qPrintable(tr("\"%1\" is not a valid universe file!").arg(filename)));
            return false;
        }
    }


    return true;
}

bool PlanetsWidget::save(const QString &filename){
    QFile file(filename);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)){
        return false;
    }

    QXmlStreamWriter xml(&file);

    xml.setAutoFormatting(true);

    xml.writeStartDocument();
    xml.writeStartElement("planets-3d-universe");

    QMutableListIterator<Planet> i(planets);
    while (i.hasNext()) {
        const Planet &planet = i.next();

        xml.writeStartElement("planet");

        xml.writeAttribute("mass", QString::number(planet.mass));

        xml.writeStartElement("position"); {
            xml.writeAttribute("x", QString::number(planet.position.x));
            xml.writeAttribute("y", QString::number(planet.position.y));
            xml.writeAttribute("z", QString::number(planet.position.z));
        } xml.writeEndElement();

        xml.writeStartElement("velocity"); {
            xml.writeAttribute("x", QString::number(planet.velocity.x / velocityfac));
            xml.writeAttribute("y", QString::number(planet.velocity.y / velocityfac));
            xml.writeAttribute("z", QString::number(planet.velocity.z / velocityfac));
        } xml.writeEndElement();

        xml.writeEndElement();
    }
    xml.writeEndElement();

    xml.writeEndDocument();

    return true;
}
