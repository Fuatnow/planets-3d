project(Planets3D)
cmake_minimum_required(VERSION 2.8)

set(Planets3D_VERSION_MAJOR 0)
set(Planets3D_VERSION_MINOR 4)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/")

include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp" @ONLY)

file(GLOB HEADERS "include/*.h")
file(GLOB SOURCES "src/*.cpp")
file(GLOB FORMS "forms/*.ui")
set(RESOURCES "resources.qrc")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Planets3D.rc.in" "${CMAKE_CURRENT_SOURCE_DIR}/Planets3D.rc" @ONLY)
set(WINDRES "Planets3D.rc")

find_package(Qt5Widgets)
include_directories(${Qt5OpenGL_INCLUDE_DIRS})
add_definitions(${Qt5OpenGL_DEFINITIONS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5OpenGL_EXECUTABLE_COMPILE_FLAGS}")

qt5_wrap_ui(FORM_HEADERS ${FORMS})
qt5_add_resources(RESOURCES_RCC ${RESOURCES})

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${FORM_HEADERS} ${HEADERS} ${RESOURCES_RCC} ${WINDRES})

qt5_use_modules(${PROJECT_NAME} OpenGL)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${OPENGL_gl_LIBRARY})