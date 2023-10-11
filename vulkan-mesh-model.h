#pragma once

#include <glm/glm.hpp>
#include <assimp/scene.h>

#include "vulkan-mesh.h"

class VulkanMeshModel
{
public:
	VulkanMeshModel();
	VulkanMeshModel( std::vector<VulkanMesh> meshes );
	~VulkanMeshModel();

	size_t get_mesh_count() const { return Meshes.size(); }
	VulkanMesh* get_mesh( size_t id );

	glm::mat4 get_model_matrix() const { return ModelMatrix; }
	void set_model_matrix( glm::mat4 matrix ) { ModelMatrix = matrix; }
	void release_mesh_model();

	static std::vector<std::string> get_materials( const aiScene* scene );
	static VulkanMesh load_mesh(
		vk::PhysicalDevice phys_device,
		vk::Device device,
		vk::Queue transfer_queue,
		vk::CommandPool transfer_command_pool,
		aiMesh* mesh,
		const aiScene* scene,
		std::vector<int> texture_ids
	);
	static std::vector<VulkanMesh> load_node(
		vk::PhysicalDevice phys_device,
		vk::Device device,
		vk::Queue transfer_queue,
		vk::CommandPool transfer_command_pool,
		aiNode* node,
		const aiScene* scene,
		std::vector<int> texture_ids
	);

private:
	std::vector<VulkanMesh> Meshes;
	glm::mat4 ModelMatrix;
};

