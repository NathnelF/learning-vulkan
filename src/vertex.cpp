#include "headers.h"

void CreateVertexBuffer(State *state, Vertex *vertices, int num_vertices)
{
  VkBufferCreateInfo vertex_buffer_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = sizeof(Vertex) * num_vertices,
    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  VmaAllocationCreateInfo alloc_info = {
    .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
  };

  validate(vmaCreateBuffer(state->context->allocator,
                           &vertex_buffer_info,
                           &alloc_info,
                           &state->context->vertex_buffer.buffer,
                           &state->context->vertex_buffer.allocation,
                           NULL),
           "could not create vertex buffer");

  validate(vmaMapMemory(state->context->allocator,
                        state->context->vertex_buffer.allocation,
                        &state->context->vertex_buffer.cpu_pointer),
           "could not map memory to cpu pointer");

  memcpy(state->context->vertex_buffer.cpu_pointer,
         vertices,
         sizeof(Vertex) * num_vertices);

  debug("created vertex buffer!");
}
