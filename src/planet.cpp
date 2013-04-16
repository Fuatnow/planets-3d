#include "planet.h"

QColor Planet::nextSelectionColor = QColor(1, 1, 1);

Planet::Planet() : position(0.0f) {
    mass = 1000.0f;

    selectionColor = nextSelectionColor;

    nextSelectionColor.setRed(nextSelectionColor.red() + 1);
    if(nextSelectionColor.red() == 0) {
        nextSelectionColor.setRed(1);
        nextSelectionColor.setGreen(nextSelectionColor.green() + 1);
        if(nextSelectionColor.green() == 0){
            nextSelectionColor.setGreen(1);
            nextSelectionColor.setBlue(nextSelectionColor.blue() + 1);
        }
    }
}

Planet::~Planet(){

}

void Planet::draw(){
    glPushMatrix();
    glTranslatef(position.x, position.y, position.z);

    GLUquadric *sphere = gluNewQuadric();
    gluQuadricTexture(sphere, GL_TRUE);
    gluQuadricOrientation(sphere, GLU_OUTSIDE);
    gluSphere(sphere,this->getRadius(),64,32);

    glPopMatrix();
}

void Planet::updatePath(){
    if(path.size() < 1 || glm::distance(path.back(), this->position) > 0.05f){
        path.push_back(this->position);
    }

    // doing this even if it hasn't been recorded allows it to get shorter.
    if(path.size() > pathLength){
        path.erase(path.begin());
    }
}

void Planet::drawPath(){
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, &path[0]);
    glDrawArrays(GL_LINE_STRIP, 0, path.size());
}

float Planet::getRadius(){
    return pow(3.0f * mass / 4.0f * M_PI, 1.0f / 3.0f) / 10.0f;
}

void Planet::drawBounds(GLenum drawmode, bool selectioncolor){
    if(selectioncolor){
        glColor3f(selectionColor.redF(), selectionColor.greenF(), selectionColor.blueF());
    }
    else{
        glColor3f(0.0f, 1.0f, 0.0f);
    }

    glPushMatrix();
    glTranslatef(position.x, position.y, position.z);

    GLUquadric *sphere = gluNewQuadric();
    gluQuadricOrientation(sphere, GLU_OUTSIDE);
    gluQuadricDrawStyle(sphere, drawmode);
    gluSphere(sphere,this->getRadius() * 1.02f, 32, 16);

    glColor3f(1.0f, 1.0f, 1.0f);

    glPopMatrix();
}

bool Planet::operator ==(const Planet &p2){
    return this == &p2;
}

unsigned int Planet::pathLength = 200;
