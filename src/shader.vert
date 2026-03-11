#version 450
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

layout(location = 0) out vec4 vertex_color;

void main()
{
    gl_Position = pc.mvp * vec4(pos, 1.0);
    vertex_color = vec4(0.35, 0.15, 0.0, 1.0);
}
