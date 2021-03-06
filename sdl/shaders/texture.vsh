#version 130

in vec4 vertex;
in vec3 normal;
in vec3 tangent;
in vec2 uv;

uniform mat4 cameraMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;

out vec2 texCoord;

out mat3 N;

void main() {
    gl_Position = cameraMatrix * modelMatrix * vertex;
    texCoord = uv;

    /* Create the view-space normal matrix. */
    vec3 n = normalize(viewMatrix * vec4(normal, 0.0)).xyz;
    vec3 t = normalize(viewMatrix * vec4(tangent, 0.0)).xyz;
    N = mat3(t, -cross(n, t), n);
}
