#include "vulkan-mesh-model.h"

VulkanMeshModel::VulkanMeshModel() 
{}

VulkanMeshModel::VulkanMeshModel( std::vector<VulkanMesh> meshes )
	: Meshes( meshes ), ModelMatrix( glm::mat4( 1.0f ) ) 
{}

VulkanMeshModel::~VulkanMeshModel()
{}

VulkanMesh* VulkanMeshModel::get_mesh( size_t id )
{
	if ( id >= Meshes.size() ) throw std::runtime_error( "Attempted to access a mesh outside bounds" );
	return &Meshes[id];
}

void VulkanMeshModel::release_mesh_model()
{
	for ( auto& mesh : Meshes )
	{
		mesh.release_buffers();
	}
}

std::vector<std::string> VulkanMeshModel::get_materials( const aiScene* scene )
{
	std::vector<std::string> textures( scene->mNumMaterials );

	printf( "Texture Count: %d\n", textures.size() );
	for ( size_t i = 0; i < textures.size(); i++ )
	{
		textures[i] = "";

		aiMaterial* material = scene->mMaterials[i];
		if ( material->GetTextureCount( aiTextureType_DIFFUSE ) )
		{
			aiString path;
			if ( material->GetTexture( aiTextureType_DIFFUSE, 0, &path ) == AI_SUCCESS )
			{
				//  remove absolute directory info
				std::string str_path( path.data );
				int id = str_path.rfind( "\\" );
				textures[i] = str_path.substr( id + 1 );
				printf( "Texture %d: %s\n", i, str_path.c_str() );
			}
		}
	}

	return textures;
}

VulkanMesh VulkanMeshModel::load_mesh( 
	vk::PhysicalDevice phys_device, 
	vk::Device device, 
	vk::Queue transfer_queue, 
	vk::CommandPool transfer_command_pool, 
	aiMesh* mesh, 
	const aiScene* scene, 
	std::vector<int> texture_ids 
)
{
	std::vector<VulkanVertex> vertices( mesh->mNumVertices );
	std::vector<uint32_t> indices;

	//  copy vertices
	for ( size_t i = 0; i < mesh->mNumVertices; i++ )
	{
		VulkanVertex& vertex = vertices[i];

		//  position
		vertex.Position = {
			mesh->mVertices[i].x,
			mesh->mVertices[i].y,
			mesh->mVertices[i].z,
		};

		//  color
		vertex.Color = { 1.0f, 1.0f, 1.0f };

		//  UV coords
		if ( mesh->mTextureCoords[0] )
		{
			vertex.UV = {
				mesh->mTextureCoords[0][i].x,
				mesh->mTextureCoords[0][i].y,
			};
		}
		else
		{
			vertex.UV = { 0.0f, 0.0f };
		}
	}

	//  copy indices
	for ( size_t i = 0; i < mesh->mNumFaces; i++ )
	{
		aiFace face = mesh->mFaces[i];
		for ( size_t j = 0; j < face.mNumIndices; j++ )
		{
			indices.push_back( face.mIndices[j] );
		}
	}

	//  create mesh
	VulkanMesh new_mesh(
		phys_device,
		device,
		transfer_queue,
		transfer_command_pool,
		&vertices,
		&indices,
		texture_ids[mesh->mMaterialIndex]
	);
	printf( "New Mesh: %s\n", mesh->mName.data );
	return new_mesh;
}

std::vector<VulkanMesh> VulkanMeshModel::load_node( 
	vk::PhysicalDevice phys_device, 
	vk::Device device, 
	vk::Queue transfer_queue, 
	vk::CommandPool transfer_command_pool, 
	aiNode* node, 
	const aiScene* scene, 
	std::vector<int> texture_ids 
)
{
	std::vector<VulkanMesh> meshes;
	for ( size_t i = 0; i < node->mNumMeshes; i++ )
	{
		//  load mesh
		meshes.push_back(
			load_mesh(
				phys_device,
				device,
				transfer_queue,
				transfer_command_pool,
				scene->mMeshes[node->mMeshes[i]],
				scene,
				texture_ids
			)
		);
	}

	//  recursive loading
	for ( size_t i = 0; i < node->mNumChildren; i++ )
	{
		std::vector<VulkanMesh> new_meshes = load_node(
			phys_device,
			device,
			transfer_queue,
			transfer_command_pool,
			node->mChildren[i],
			scene,
			texture_ids
		);
		meshes.insert( meshes.end(), new_meshes.begin(), new_meshes.end());
	}

	return meshes;
}
