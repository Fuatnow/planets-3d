#include "planetswindow.h"
#include "shaders.h"

#include <algorithm>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <SDL_image.h>

PlanetsWindow::PlanetsWindow(int argc, char *argv[]) : placing(universe), camera(universe), totalFrames(0) {
    initSDL();
    initGL();

    for(int i = 0; i < argc; ++i) {
        if(universe.load(argv[i])) break;
    }

    if(universe.size() == 0){
        universe.generateRandom(100, 1.0e3f, 1.0f, 100.0f);
    }
}

PlanetsWindow::~PlanetsWindow(){
    SDL_GL_DeleteContext(contextSDL);
    SDL_DestroyWindow(windowSDL);

    SDL_Quit();
}

void PlanetsWindow::initSDL(){
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) == -1) {
        printf("ERROR: Unable to init SDL! \"%s\"", SDL_GetError());
        abort();
    }

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,  1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,  8);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    windowSDL = SDL_CreateWindow("Planets3D", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!windowSDL){
        printf("ERROR: Unable to create window!");
        abort();
    }

    contextSDL = SDL_GL_CreateContext(windowSDL);

    SDL_GL_SetSwapInterval(0);

    /* TODO - I may want to handle the cursor myself... */
    SDL_ShowCursor(SDL_ENABLE);
}

void PlanetsWindow::initGL(){
    SDL_GL_MakeCurrent(windowSDL, contextSDL);

#ifdef PLANETS3D_WITH_GLEW
    GLuint glewstatus = glewInit();
    if(glewstatus != GLEW_OK){
        printf("GLEW ERROR: %s", glewGetErrorString(glewstatus));
        abort();
    }
    if(!GLEW_VERSION_4_0){
        printf("WARNING: OpenGL 4.0 support NOT detected, things probably won't work.");
    }
#endif

    initShaders();

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepthf(1.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifdef GL_LINE_SMOOTH
    glEnable(GL_LINE_SMOOTH);
#endif

    glEnableVertexAttribArray(vertex);

    planetTexture = loadTexture("planet.png");
}

GLuint PlanetsWindow::loadTexture(const char* filename){
    SDL_Surface* image = IMG_Load(filename);

    if(image == nullptr){
        printf("Failed to load texture! Error: %s\n", IMG_GetError());
        return 0;
    }

    SDL_Surface* converted = SDL_ConvertSurface(image, SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888), 0);
    SDL_FreeSurface(image);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, converted->pixels);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    SDL_FreeSurface(converted);

    return texture;
}

GLuint compileShader(const char *source, GLenum shaderType){
    GLuint shader = glCreateShader(shaderType);

    glShaderSource(shader, 1, (const GLchar**)&source, 0);

    glCompileShader(shader);

    int isCompiled,maxLength;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

    if(maxLength > 1){
        char *infoLog = new char[maxLength];

        glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog);

        if(isCompiled == GL_FALSE) {
            printf("ERROR: failed to compile shader!\n\tLog: %s", infoLog);
            delete[] infoLog;

            return 0;
        }else{
            printf("INFO: succesfully compiled shader.\n\tLog: %s", infoLog);
            delete[] infoLog;
        }
    }else{
        if(isCompiled == GL_FALSE) {
            printf("ERROR: failed to compile shader! No log availible.");
            return 0;
        }
    }
    return shader;
}

int linkShaderProgram(GLuint vsh, GLuint fsh){
    GLuint program = glCreateProgram();

    glAttachShader(program, vsh);
    glAttachShader(program, fsh);

    glLinkProgram(program);

    int isLinked, maxLength;
    glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

    if(maxLength > 1){
        char *infoLog = new char[maxLength];

        glGetProgramInfoLog(program, maxLength, &maxLength, infoLog);

        if(isLinked == GL_FALSE){
            printf("ERROR: failed to link shader program!\n\tLog: %s", infoLog);
            delete[] infoLog;

            return 0;
        }else{
            printf("INFO: succesfully linked shader.\n\tLog: %s", infoLog);
            delete[] infoLog;
        }
    }else{
        if(isLinked == GL_FALSE){
            printf("ERROR: failed to link shader program! No log availible.");
            return 0;
        }
    }

    return program;
}

void PlanetsWindow::initShaders(){
    shaderColor_vsh = compileShader(color_vertex_src,     GL_VERTEX_SHADER);
    shaderColor_fsh = compileShader(color_fragment_src,   GL_FRAGMENT_SHADER);
    shaderColor = linkShaderProgram(shaderColor_vsh, shaderColor_fsh);

    glUseProgram(shaderColor);
    shaderColor_color = glGetUniformLocation(shaderColor, "color");
    shaderColor_cameraMatrix = glGetUniformLocation(shaderColor, "cameraMatrix");
    shaderColor_modelMatrix = glGetUniformLocation(shaderColor, "modelMatrix");

    glBindAttribLocation(shaderColor, vertex, "vertex");

    shaderTexture_vsh = compileShader(texture_vertex_src,   GL_VERTEX_SHADER);
    shaderTexture_fsh = compileShader(texture_fragment_src, GL_FRAGMENT_SHADER);
    shaderTexture = linkShaderProgram(shaderTexture_vsh, shaderTexture_fsh);

    glUseProgram(shaderTexture);
    shaderTexture_cameraMatrix = glGetUniformLocation(shaderTexture, "cameraMatrix");
    shaderTexture_modelMatrix = glGetUniformLocation(shaderTexture, "modelMatrix");

    glBindAttribLocation(shaderTexture, vertex, "vertex");
    glBindAttribLocation(shaderTexture, uv, "uv");
}

int PlanetsWindow::run(){
    running = true;

    std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point last_time = std::chrono::high_resolution_clock::now();

    while(running) {
        std::chrono::high_resolution_clock::time_point current = std::chrono::high_resolution_clock::now();
        int64_t delay = std::chrono::duration_cast<std::chrono::microseconds>(current - last_time).count();
        last_time = current;

        doEvents();

        if(placing.step == PlacingInterface::NotPlacing || placing.step == PlacingInterface::Firing){
            universe.advance(delay);
        }

        paint();

        SDL_GL_SwapWindow(windowSDL);

        ++totalFrames;
    }

    std::chrono::high_resolution_clock::time_point current = std::chrono::high_resolution_clock::now();
    printf("Total Time: %f seconds.\n", (std::chrono::duration_cast<std::chrono::microseconds>(current - start_time).count() * 1.0e-6f));
    printf("Total Frames: %i.\n", totalFrames);
    printf("Average Draw Time: %fms.\n", (std::chrono::duration_cast<std::chrono::microseconds>(current - start_time).count() * 1.0e-3f) / float(totalFrames));
    printf("Average Framerate: %f fps.\n", (1.0f / (std::chrono::duration_cast<std::chrono::microseconds>(current - start_time).count() / float(totalFrames))) * 1.0e6f);

    return 0;
}

void PlanetsWindow::paint(){
    SDL_GL_MakeCurrent(windowSDL, contextSDL);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    camera.setup();

    glUseProgram(shaderTexture);

    glUniformMatrix4fv(shaderTexture_cameraMatrix, 1, GL_FALSE, glm::value_ptr(camera.camera));
    glUniformMatrix4fv(shaderTexture_modelMatrix, 1, GL_FALSE, glm::value_ptr(glm::mat4()));

    glEnableVertexAttribArray(uv);

    for(const auto& i : universe){
        drawPlanet(i.second);
    }

    glDisableVertexAttribArray(uv);

    glUseProgram(shaderColor);

    glUniformMatrix4fv(shaderColor_cameraMatrix, 1, GL_FALSE, glm::value_ptr(camera.camera));
    glUniformMatrix4fv(shaderColor_modelMatrix, 1, GL_FALSE, glm::value_ptr(glm::mat4()));

    if(universe.isSelectedValid()){
        drawPlanetWireframe(universe.getSelected());
    }

    glUniform4fv(shaderColor_color, 1, glm::value_ptr(glm::vec4(1.0f)));

    switch(placing.step){
    case PlacingInterface::FreeVelocity:{
        float length = glm::length(placing.planet.velocity) / PlanetsUniverse::velocityfac;

        if(length > 0.0f){
            glm::mat4 matrix = glm::translate(placing.planet.position);
            matrix = glm::scale(matrix, glm::vec3(placing.planet.radius()));
            matrix *= placing.rotation;
            glUniformMatrix4fv(shaderColor_modelMatrix, 1, GL_FALSE, glm::value_ptr(matrix));

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

            glVertexAttribPointer(vertex, 3, GL_FLOAT, GL_FALSE, 0, verts);
            glDrawElements(GL_TRIANGLES, sizeof(indexes), GL_UNSIGNED_BYTE, indexes);
        }
    }
    case PlacingInterface::FreePositionXY:
    case PlacingInterface::FreePositionZ:
        drawPlanetWireframe(placing.planet);
        break;
    case PlacingInterface::OrbitalPlane:
    case PlacingInterface::OrbitalPlanet:
        if(universe.isSelectedValid() && placing.orbitalRadius > 0.0f){
            glm::mat4 matrix = glm::translate(universe.getSelected().position);
            matrix = glm::scale(matrix, glm::vec3(placing.orbitalRadius));
            matrix *= placing.rotation;
            glUniformMatrix4fv(shaderColor_modelMatrix, 1, GL_FALSE, glm::value_ptr(matrix));

            glVertexAttribPointer(vertex, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), glm::value_ptr(circle.verts[0].position));
            glDrawElements(GL_LINES, circle.lineCount, GL_UNSIGNED_INT, circle.lines);

            drawPlanetWireframe(placing.planet);
        }
        break;
    default: break;
    }


    /* TODO - implement */
}

void PlanetsWindow::toggleFullscreen(){
    fullscreen = !fullscreen;

    if(fullscreen){
        SDL_DisplayMode mode;
        SDL_GetDesktopDisplayMode(0, &mode);
        SDL_SetWindowDisplayMode(windowSDL, &mode);
    }
    SDL_SetWindowFullscreen(windowSDL, SDL_bool(fullscreen));
}

void PlanetsWindow::doEvents(){
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        switch (event.type) {
        case SDL_QUIT:
            onClose();
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym){
            case SDLK_ESCAPE:
                onClose();
                break;
            case SDLK_p:
                placing.beginInteractiveCreation();
                break;
            case SDLK_o:
                placing.beginOrbitalCreation();
                break;
            }
            break;
        case SDL_WINDOWEVENT:
            switch(event.window.event){
            case SDL_WINDOWEVENT_RESIZED:
                if(event.window.data1 < 1 || event.window.data2 < 1){
                    SDL_SetWindowSize(windowSDL, std::max(event.window.data1, 1), std::max(event.window.data2, 1));
                    break;
                }
                onResized(event.window.data1, event.window.data2);
                break;
            }
            break;
        case SDL_MOUSEWHEEL:
            if(!placing.handleMouseWheel(event.wheel.y * 0.2f)){
                camera.distance -= event.wheel.y * camera.distance * 0.1f;

                camera.bound();
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if(event.button.button == SDL_BUTTON_LEFT){
                glm::ivec2 pos(event.button.x, event.button.y);
                if(!placing.handleMouseClick(pos, windowWidth, windowHeight, camera)){
                    camera.selectUnder(pos, windowWidth, windowHeight);
                }
            }
            /* Always show cursor when mouse button is released. */
            SDL_SetRelativeMouseMode(SDL_FALSE);
            break;
        case SDL_MOUSEMOTION:
            /* yrel is inverted compared to the delta calculated in Qt. */
            glm::ivec2 delta(event.motion.xrel, -event.motion.yrel);

            bool holdCursor = false;

            if(!placing.handleMouseMove(glm::ivec2(event.motion.x, event.motion.y), delta, windowWidth, windowHeight, camera, holdCursor)){
                if(event.motion.state & SDL_BUTTON_MMASK){
                    camera.distance -= delta.y * camera.distance * 1.0e-2f;
                    camera.bound();
                }else if(event.motion.state & SDL_BUTTON_RMASK){
                    camera.xrotation += delta.y * 0.01f;
                    camera.zrotation += delta.x * 0.01f;

                    camera.bound();

                    holdCursor = true;
                }
            }
            SDL_SetRelativeMouseMode(SDL_bool(holdCursor));
            break;
        }
    }
}

void PlanetsWindow::onClose(){
    /* TODO - make warning message and/or menu */
    running = false;
}

void PlanetsWindow::onResized(uint32_t width, uint32_t height){
    windowWidth = width;
    windowHeight = height;

    glViewport(0, 0, width, height);

    camera.resizeViewport(width, height);
}

void PlanetsWindow::drawPlanet(const Planet &planet){
    glm::mat4 matrix = glm::translate(planet.position);
    matrix = glm::scale(matrix, glm::vec3(planet.radius()));
    glUniformMatrix4fv(shaderTexture_modelMatrix, 1, GL_FALSE, glm::value_ptr(matrix));

    glVertexAttribPointer(vertex, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), glm::value_ptr(highResSphere.verts[0].position));
    glVertexAttribPointer(uv,     2, GL_FLOAT, GL_FALSE, sizeof(Vertex), glm::value_ptr(highResSphere.verts[0].uv));
    glDrawElements(GL_TRIANGLES, highResSphere.triangleCount, GL_UNSIGNED_INT, highResSphere.triangles);
}

void PlanetsWindow::drawPlanetColor(const Planet &planet, const uint32_t &color){
    glUniform4fv(shaderColor_color, 1, glm::value_ptr(uintColorToVec4(color)));

    glm::mat4 matrix = glm::translate(planet.position);
    matrix = glm::scale(matrix, glm::vec3(planet.radius() * 1.05f));
    glUniformMatrix4fv(shaderColor_modelMatrix, 1, GL_FALSE, glm::value_ptr(matrix));

    glVertexAttribPointer(vertex, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), glm::value_ptr(lowResSphere.verts[0].position));
    glDrawElements(GL_TRIANGLES, lowResSphere.triangleCount, GL_UNSIGNED_INT, lowResSphere.triangles);
}

void PlanetsWindow::drawPlanetWireframe(const Planet &planet, const uint32_t &color){
    glUniform4fv(shaderColor_color, 1, glm::value_ptr(uintColorToVec4(color)));

    glm::mat4 matrix = glm::translate(planet.position);
    matrix = glm::scale(matrix, glm::vec3(planet.radius() * 1.05f));
    glUniformMatrix4fv(shaderColor_modelMatrix, 1, GL_FALSE, glm::value_ptr(matrix));

    glVertexAttribPointer(vertex, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), glm::value_ptr(lowResSphere.verts[0].position));
    glDrawElements(GL_LINES, lowResSphere.lineCount, GL_UNSIGNED_INT, lowResSphere.lines);
}

const Sphere<64, 32> PlanetsWindow::highResSphere = Sphere<64, 32>();
const Sphere<32, 16> PlanetsWindow::lowResSphere  = Sphere<32, 16>();

const Circle<64> PlanetsWindow::circle = Circle<64>();

const GLuint PlanetsWindow::vertex = 0;
const GLuint PlanetsWindow::uv     = 1;
