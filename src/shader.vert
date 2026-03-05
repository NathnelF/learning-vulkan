#version 450
layout(location = 0) in vec3 pos;
layout(location = 0) out vec4 vertex_color;

void main()
{
    gl_Position = vec4(pos, 1.0);
    vertex_color = vec4(0.35, 0.15, 0.0, 0.0);
}
