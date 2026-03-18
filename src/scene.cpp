#include "headers.h"

//TODO(Nate): Interface into scene entities
//For now we hard code
void CreateScene(State *state)
{
	debug("starting scene creation");
	MegaBuffer *mega_buffer = &state->mega_buffer;
	int mesh_counts[MAX_MESHES] = {0};

	mesh_counts[SKULL] = 1;
	mesh_counts[CONE] = 1;
	mesh_counts[CUBE] = 1;
	mesh_counts[CYLINDER] = 1;
	mesh_counts[SPHERE] = 1;

	//Enumerate our list of valid meshes
	//Create one command per desired mesh
	//with instanceCount equal to the desired count

	//fill out the commands
	VkDrawIndexedIndirectCommand commands[MAX_MESHES];
	InstanceData instances[MAX_INSTANCES];
	int draw_count = 0;
	int instance_cursor = 0;
	printf("mesh_count = %d\n", mega_buffer->mesh_count);
	int x_pos = 0;
	for (u32 i = 0; i < mega_buffer->mesh_count; i++)
	{
		//i in any given loop will be the mesh region associated 
		//with the enumeration MeshType
		printf("region[%d]: vertex_count=%d, index_count=%d\n", i, mega_buffer->regions[i].vertex_count, mega_buffer->regions[i].index_count);
		if (mesh_counts[i] == 0)
		{
			continue;
		}

		commands[draw_count].indexCount = mega_buffer->regions[i].index_count;
		commands[draw_count].instanceCount = mesh_counts[i];
		commands[draw_count].firstIndex = mega_buffer->regions[i].index_offset;
		commands[draw_count].vertexOffset = mega_buffer->regions[i].vertex_offset;
		commands[draw_count].firstInstance = instance_cursor;

		for (int j = 0; j < mesh_counts[i]; j++)
		{
			instances[instance_cursor].transform = HMM_Translate(HMM_V3(x_pos, 0, 0)); 
			instance_cursor++;
			x_pos += 3.0f;
		}

		draw_count++;
	}

	for (u32 i = 0; i < draw_count; i++)
	{
		printf("command[%d], indexCount = %d, instanceCount = %d, firstIndex = %d, vertexOffset  = %d\n", i, commands[i].indexCount, commands[i].instanceCount, commands[i].firstIndex,commands[i].vertexOffset);
	}

	state->scene.draw_count = draw_count;

	//Create instance buffer
	VkBufferCreateInfo instance_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(InstanceData) * MAX_INSTANCES,
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo instance_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_CPU_ONLY,
		.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
	};

	VmaAllocationInfo instance_result = {};

	validate(vmaCreateBuffer(state->context->allocator, &instance_buffer_info, &instance_allocation_info, &state->scene.instance_buffer, &state->scene.instance_allocation, &instance_result), "could not create instance buffer");

	memcpy(instance_result.pMappedData, instances, sizeof(InstanceData) * instance_cursor);

	state->scene.instance_pointer = (InstanceData*)instance_result.pMappedData;

	VkBufferDeviceAddressInfo address_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = state->scene.instance_buffer,
	};

	state->scene.instance_buffer_address = vkGetBufferDeviceAddress(state->context->device, &address_info);


	//Create indirect buffer
	VkBufferCreateInfo indirect_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(VkDrawIndexedIndirectCommand) * MAX_MESHES,
		.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo indirect_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_CPU_ONLY,
	};

	VmaAllocationInfo indirect_result = {};

	validate(vmaCreateBuffer(state->context->allocator, &indirect_buffer_info, &indirect_allocation_info, &state->scene.indirect_buffer, &state->scene.indirect_allocation, &indirect_result), "could not create indirect buffer");

	memcpy(indirect_result.pMappedData, commands, sizeof(VkDrawIndexedIndirectCommand) * draw_count);

	state->scene.indirect_pointer = (VkDrawIndexedIndirectCommand*)indirect_result.pMappedData;

	debug("created scene!");

}