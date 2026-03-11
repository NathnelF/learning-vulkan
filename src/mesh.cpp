#include "headers.h"

// create mega buffer
//     load each .glb file
//     separate vertex and index data
//     copy all vertex data then all index data to staging buffer
//     transfer staging buffer to gpu only memory
//
struct RawMesh
{
  Vertex *vertices;
  u32 *indices;
  u32 vertex_count;
  u32 index_count;
};

// each mesh has vertex and index data
//
void extract_mesh(RawMesh *raw_mesh, Arena *scratch, cgltf_mesh *mesh_data)
{
  // get the first primitive
  cgltf_primitive *primitive = &mesh_data->primitives[0];

  cgltf_accessor *position_accessor = NULL;
  cgltf_accessor *normal_accessor = NULL;
  cgltf_accessor *uv_accessor = NULL;

  for (int i = 0; i < (int)primitive->attributes_count; i++)
  {
    cgltf_attribute *attribute = &primitive->attributes[i];
    if (attribute->type == cgltf_attribute_type_position)
    {
      position_accessor = attribute->data;
    }
    if (attribute->type == cgltf_attribute_type_normal)
    {
      normal_accessor = attribute->data;
    }
    if (attribute->type == cgltf_attribute_type_texcoord)
    {
      uv_accessor = attribute->data;
    }
  }

  if (!position_accessor)
  {
    err("data has no positon attribute");
  }

  u32 vertex_count = (u32)position_accessor->count;
  raw_mesh->vertex_count = vertex_count;
  raw_mesh->vertices =
    (Vertex *)ArenaPush(scratch, sizeof(Vertex) * vertex_count);

  // extract positions
  for (u32 i = 0; i < vertex_count; i++)
  {
    float tmp[3];
    cgltf_accessor_read_float(position_accessor, i, tmp, 3);
    raw_mesh->vertices[i].x = tmp[0];
    raw_mesh->vertices[i].y = tmp[1];
    raw_mesh->vertices[i].z = tmp[2];
  }

  // extract normals
  if (normal_accessor)
  {
    for (u32 i = 0; i < vertex_count; i++)
    {
      float tmp[3];
      cgltf_accessor_read_float(normal_accessor, i, tmp, 3);
      raw_mesh->vertices[i].nx = tmp[0];
      raw_mesh->vertices[i].ny = tmp[1];
      raw_mesh->vertices[i].nz = tmp[2];
    }
  }

  // extract uvs
  if (uv_accessor)
  {
    for (u32 i = 0; i < vertex_count; i++)
    {
      float tmp[3];
      cgltf_accessor_read_float(uv_accessor, i, tmp, 2);
      raw_mesh->vertices[i].u = tmp[0];
      raw_mesh->vertices[i].v = tmp[1];
    }
  }

  // extract indices
  if (primitive->indices)
  {
    u32 index_count = (u32)primitive->indices->count;
    raw_mesh->indices = (u32 *)ArenaPush(scratch, sizeof(u32) * index_count);
    raw_mesh->index_count = index_count;
    for (u32 i = 0; i < index_count; i++)
    {
      raw_mesh->indices[i] =
        (u32)cgltf_accessor_read_index(primitive->indices, i);
    }
  }
  else
  {
    // generate sequential indices
    u32 index_count = vertex_count;
    raw_mesh->indices = (u32 *)ArenaPush(scratch, sizeof(u32) * index_count);
    raw_mesh->index_count = index_count;
    for (u32 i = 0; i < index_count; i++)
    {
      raw_mesh->indices[i] = i;
    }
  }
  debug("extracted mesh!");
}

void CreateMegaBuffer(State *state, const char **mesh_paths, int path_count)
{
  Arena *scratch = &state->scratch_arena;
  MegaBuffer *mega_buffer = &state->mega_buffer;

  RawMesh *raw_meshes =
    (RawMesh *)ArenaPush(scratch, sizeof(RawMesh) * MAX_MESHES);

  int current_mesh = 0;

  for (int i = 0; i < path_count; i++)
  {
    cgltf_options options = {};
    cgltf_data *data = NULL;

    cgltf_result parse_result =
      cgltf_parse_file(&options, mesh_paths[i], &data);
    if (parse_result != cgltf_result_success)
    {
      err("could not parse mesh file at %s", mesh_paths[i]);
    }
    cgltf_result buffer_result = cgltf_load_buffers(&options, data, NULL);
    if (buffer_result != cgltf_result_success)
    {
      err("could not load buffers from file at %s", mesh_paths[i]);
    }

    cgltf_mesh *meshes = data->meshes;
    int meshes_count = (int)data->meshes_count;
    for (int j = 0; j < meshes_count; j++)
    {
      if (current_mesh >= MAX_MESHES)
      {
        err("exceeded max meshes %d", MAX_MESHES);
      }
      extract_mesh(&raw_meshes[current_mesh], scratch, &meshes[j]);
      current_mesh++;
    }
    cgltf_free(data);
  }

  // now all our raw mesh data is stored in a scratch arena
  //
  //  transition from scratch arena to staging area
  int total_meshes = current_mesh;
  u64 total_vertex_bytes = 0;
  u64 total_index_bytes = 0;
  for (int i = 0; i < total_meshes; i++)
  {
    total_vertex_bytes += sizeof(Vertex) * raw_meshes[i].vertex_count;
    total_index_bytes += sizeof(u32) * raw_meshes[i].index_count;
  }

  u64 vertex_region_start = 0;
  // align to 16 bytes TODO(Nate): understand why this works
  u64 index_region_start = (total_vertex_bytes + 15) & ~(u64)15;
  u64 total_bytes = total_index_bytes + total_vertex_bytes;

  mega_buffer->vertex_region_offset = vertex_region_start;
  mega_buffer->index_region_offset = index_region_start;

  VkBufferCreateInfo staging_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = total_bytes,
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
  };

  VmaAllocationCreateInfo staging_alloc_info = {
    .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .usage = VMA_MEMORY_USAGE_CPU_ONLY,
  };

  VkBuffer staging_buffer;
  VmaAllocation staging_allocation;
  VmaAllocationInfo staging_result = {};
  validate(vmaCreateBuffer(state->context->allocator,
                           &staging_info,
                           &staging_alloc_info,
                           &staging_buffer,
                           &staging_allocation,
                           &staging_result),
           "could not allocate staging buffer");

  // memcpy from cpu to staging buffer
  u8 *base = (u8 *)staging_result.pMappedData;
  u32 vertex_position = 0;
  u32 index_position = 0;

  // memcpy from scratch arena into staging buffer
  for (int i = 0; i < total_meshes; i++)
  {
    MeshRegion *region = &mega_buffer->regions[i];
    mega_buffer->mesh_count += 1;

    region->vertex_offset = vertex_position;
    region->index_offset = index_position;

    region->vertex_count = raw_meshes[i].vertex_count;
    region->index_count = raw_meshes[i].index_count;

    memcpy(base + vertex_region_start + vertex_position * sizeof(Vertex),
           raw_meshes[i].vertices,
           sizeof(Vertex) * raw_meshes[i].vertex_count);

    memcpy(base + index_region_start + index_position * sizeof(u32),
           raw_meshes[i].indices,
           sizeof(u32) * raw_meshes[i].index_count);

    vertex_position += raw_meshes[i].vertex_count;
    index_position += raw_meshes[i].index_count;
  }

  // create device local gpu memory for mega buffer
  VkBufferCreateInfo mega_buffer_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = total_bytes,
    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
             VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  VmaAllocationCreateInfo mega_alloc_info = {
    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
  };

  validate(vmaCreateBuffer(state->context->allocator,
                           &mega_buffer_info,
                           &mega_alloc_info,
                           &mega_buffer->buffer,
                           &mega_buffer->allocation,
                           NULL),
           "could not create mega buffer on gpu");

  //  transition from stagin area to gpu read only VRAM
  VkCommandBufferAllocateInfo command_buffer_alloc_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = state->context->frame_context[0].command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };

  VkCommandBuffer buffer;
  vkAllocateCommandBuffers(
    state->context->device, &command_buffer_alloc_info, &buffer);

  VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(buffer, &begin_info);

  VkBufferCopy region = { 0, 0, total_bytes };
  vkCmdCopyBuffer(buffer, staging_buffer, mega_buffer->buffer, 1, &region);
  vkEndCommandBuffer(buffer);

  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &buffer,
  };

  vkQueueSubmit(state->context->queue, 1, &submit, VK_NULL_HANDLE);
  vkQueueWaitIdle(state->context->queue);

  vkFreeCommandBuffers(state->context->device,
                       state->context->frame_context[0].command_pool,
                       1,
                       &buffer);

  vmaDestroyBuffer(
    state->context->allocator, staging_buffer, staging_allocation);

  ArenaReset(scratch);
  debug("created mega buffer");
}
