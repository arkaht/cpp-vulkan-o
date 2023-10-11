#include "vulkan-mesh.h"

VulkanMesh::VulkanMesh(
	vk::PhysicalDevice physical_device,
	vk::Device device,
	vk::Queue transfer_queue,
	vk::CommandPool transfer_command_pool,
	std::vector<VulkanVertex>* vertices,
	std::vector<uint32_t>* indices,
	int texture_id
)
	: PhysicalDevice( physical_device ), Device( device ),
	  VertexCount( vertices->size() ), IndexCount( indices->size() ),
	  TextureID( texture_id )
{
	MeshData.Model = glm::mat4( 1.0f );

	setup_vertex_buffer( transfer_queue, transfer_command_pool, vertices );
	setup_index_buffer( transfer_queue, transfer_command_pool, indices );
}

void VulkanMesh::release_buffers()
{
	Device.destroyBuffer( VertexBuffer, nullptr );
	Device.freeMemory( VertexBufferMemory, nullptr );

	Device.destroyBuffer( IndexBuffer, nullptr );
	Device.freeMemory( IndexBufferMemory, nullptr );
}

void VulkanMesh::setup_vertex_buffer( vk::Queue transfer_queue, vk::CommandPool transfer_command_pool, std::vector<VulkanVertex>* vertices )
{
	vk::DeviceSize buffer_size = sizeof( VulkanVertex ) * vertices->size();

	//  temporary buffer to stage vertex data before transfering to GPU
	vk::Buffer staging_buffer;
	vk::DeviceMemory staging_buffer_memory;
	create_buffer(
		PhysicalDevice,
		Device,
		buffer_size,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		&staging_buffer,
		&staging_buffer_memory
	);

	//  map memory to staging buffer
	void* data;
	Device.mapMemory( staging_buffer_memory, {}, buffer_size, {}, &data );
	memcpy( data, vertices->data(), (size_t)buffer_size );
	Device.unmapMemory( staging_buffer_memory );

	// Create buffer with vk::BufferUsageFlagBits::eTransferDst to mark as recipient
	// of transfer data Buffer memory need to be vk::MemoryPropertyFlagBits::eDeviceLocal
	// meaning memory is on GPU only and not CPU-accessible
	create_buffer( 
		PhysicalDevice, 
		Device, 
		buffer_size, 
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, 
		vk::MemoryPropertyFlagBits::eDeviceLocal, 
		&VertexBuffer,
		&VertexBufferMemory
	);

	//  copy staging buffer to vertex buffer on GPU
	copy_buffer( Device, transfer_queue, transfer_command_pool, staging_buffer, VertexBuffer, buffer_size );

	//  release staging buffer
	Device.destroyBuffer( staging_buffer, nullptr );
	Device.freeMemory( staging_buffer_memory, nullptr );
}

void VulkanMesh::setup_index_buffer( vk::Queue transfer_queue, vk::CommandPool transfer_command_pool, std::vector<uint32_t>* indices )
{
	vk::DeviceSize buffer_size = sizeof( uint32_t ) * indices->size();

	vk::Buffer staging_buffer;
	vk::DeviceMemory staging_buffer_memory;
	create_buffer( 
		PhysicalDevice, 
		Device, 
		buffer_size, 
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		&staging_buffer, 
		&staging_buffer_memory
	);

	void* data;
	Device.mapMemory( staging_buffer_memory, {}, buffer_size, {}, &data );
	memcpy( data, indices->data(), static_cast<size_t>( buffer_size ) );
	Device.unmapMemory( staging_buffer_memory );

	// This time with vk::BufferUsageFlagBits::eIndexBuffer,
	// &indexBuffer and &indexBufferMemory
	create_buffer( 
		PhysicalDevice, 
		Device, 
		buffer_size,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		&IndexBuffer, 
		&IndexBufferMemory 
	);

	// Copy to IndexBuffer
	copy_buffer( 
		Device,
		transfer_queue,
		transfer_command_pool,
		staging_buffer, 
		IndexBuffer, 
		buffer_size 
	);

	Device.destroyBuffer( staging_buffer );
	Device.freeMemory( staging_buffer_memory );
}
