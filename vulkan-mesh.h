#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include "vulkan-utils.hpp"

struct MeshData
{
	glm::mat4 Model;
};

class VulkanMesh
{
public:
	VulkanMesh( 
		vk::PhysicalDevice physical_device, 
		vk::Device device, 
		vk::Queue transfer_queue, 
		vk::CommandPool transfer_command_pool, 
		std::vector<VulkanVertex>* vertices,
		std::vector<uint32_t>* indices,
		int texture_id
	);
	VulkanMesh() = default;
	~VulkanMesh() = default;

	size_t get_vertex_count() const { return VertexCount; }
	vk::Buffer get_vertex_buffer() const { return VertexBuffer; }

	size_t get_index_count() const { return IndexCount; }
	vk::Buffer get_index_buffer() const { return IndexBuffer; }

	MeshData get_mesh_data() const { return MeshData; }
	void set_model_matrix( const glm::mat4& matrix ) { MeshData.Model = matrix; }

	void release_buffers();

	int get_texture_id() const { return TextureID; }

private:
	vk::PhysicalDevice PhysicalDevice;
	vk::Device Device;
	
	size_t VertexCount;
	vk::Buffer VertexBuffer;
	vk::DeviceMemory VertexBufferMemory;

	size_t IndexCount;
	vk::Buffer IndexBuffer;
	vk::DeviceMemory IndexBufferMemory;

	MeshData MeshData;
	int TextureID;

	void setup_vertex_buffer( vk::Queue transfer_queue, vk::CommandPool transfer_command_pool, std::vector<VulkanVertex>* vertices );
	void setup_index_buffer( vk::Queue transfer_queue, vk::CommandPool transfer_command_pool, std::vector<uint32_t>* indices );
};