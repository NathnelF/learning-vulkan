#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference_uvec2 : require

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 vertex_color;

struct InstanceData {
    mat4 transform;
};

layout (buffer_reference, scalar) readonly buffer InstanceBuffer {
    InstanceData instances[];
};

layout(push_constant) uniform PushConstants {
    mat4 view_projection;
    uvec2 instance_buffer_address;
};


void main()
{
    InstanceBuffer instance_buffer = InstanceBuffer(instance_buffer_address);
    mat4 model = instance_buffer.instances[gl_InstanceIndex].transform;
    gl_Position = view_projection * model * vec4(pos, 1.0);
    vertex_color = vec4(0.35, 0.15, 0.0, 1.0);
}
