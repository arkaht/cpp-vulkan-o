#pragma once

#include <fstream>

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

const std::vector<const char*> VulkanDeviceExtensions
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const bool VulkanEnableValidationLayers = false;
const std::vector<const char*> VulkanValidationLayers
{
	"VK_LAYER_KHRONOS_validation",
};

struct VulkanQueueFamilyIndices
{
	int GraphicsFamily = -1;
	int PresentationFamily = -1;

	bool is_valid() 
	{ 
		return GraphicsFamily >= 0
			&& PresentationFamily >= 0; 
	}
};

struct VulkanSwapchainDetails
{
	vk::SurfaceCapabilitiesKHR SurfaceCapabilities;  //  displaying capabilities (e.g. image size/extent)
	std::vector<vk::SurfaceFormatKHR> Formats;  //  images formats (e.g. RGBA)
	std::vector<vk::PresentModeKHR> PresentationModes;

	bool is_valid() 
	{ 
		return !PresentationModes.empty() 
			&& !Formats.empty(); 
	}
};

struct VulkanSwapchainImage
{
	vk::Image Image;
	vk::ImageView ImageView;
};

struct VulkanVertex
{
	glm::vec3 Position;
	glm::vec3 Color;
};

static std::vector<char> read_shader_file( const std::string& filename )
{
	//  open shader file
	std::ifstream file { filename, std::ios::binary | std::ios::ate };
	if ( !file.is_open() ) throw std::runtime_error( "Failed to open the file " + filename );

	//  buffer preparation
	size_t file_size = (size_t)file.tellg();  //  get size from pointer position
	std::vector<char> buffer( file_size );
	
	//  read & close
	file.seekg( 0 );
	file.read( buffer.data(), file_size );
	file.close();

	return buffer;
}

static uint32_t find_memory_type_index( vk::PhysicalDevice physical_device, uint32_t types, vk::MemoryPropertyFlags properties )
{
	// Get properties of physical device
	vk::PhysicalDeviceMemoryProperties memoryProperties = physical_device.getMemoryProperties();

	for ( uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i )
	{
		// We iterate through each bit, shifting of 1 (with i) each time.
		// This way we go through each type to check it is allowed.
		if ( ( types & ( 1 << i ) ) &&
			// Desired property bit flags are part of memory type's property flags.
			// By checking the equality, we check that all properties are available at the
			// same time, and not only one property is common.
			( memoryProperties.memoryTypes[i].propertyFlags & properties ) == properties )
		{
			// If this type is an allowed type and has the flags we want, then i
			// is the current index of the memory type we want to use. Return it.
			return i;
		}
	}

	return 0;
}

static void create_buffer( vk::PhysicalDevice physical_device, vk::Device device, vk::DeviceSize buffer_size, vk::BufferUsageFlags buffer_usage, vk::MemoryPropertyFlags buffer_properties, vk::Buffer* buffer, vk::DeviceMemory* buffer_memory )
{
	// Buffer info
	vk::BufferCreateInfo buffer_info {};
	buffer_info.size = buffer_size;
	// Multiple types of buffers
	buffer_info.usage = buffer_usage;
	// Is vertex buffer sharable ? Here: no.
	buffer_info.sharingMode = vk::SharingMode::eExclusive;
	*buffer = device.createBuffer( buffer_info );

	// Get buffer memory requirements
	vk::MemoryRequirements memoryRequirements;
	device.getBufferMemoryRequirements( *buffer, &memoryRequirements );

	// Allocate memory to buffer
	vk::MemoryAllocateInfo memory_alloc_info {};
	memory_alloc_info.allocationSize = memoryRequirements.size;
	memory_alloc_info.memoryTypeIndex = find_memory_type_index(
		physical_device,
		// Index of memory type on physical device that has required bit flags
		memoryRequirements.memoryTypeBits,
		buffer_properties
	);

	// Allocate memory to VkDeviceMemory
	auto result = device.allocateMemory( &memory_alloc_info, nullptr, buffer_memory );
	if ( result != vk::Result::eSuccess )
	{
		throw std::runtime_error( "Failed to allocate vertex buffer memory" );
	}

	// Allocate memory to given vertex buffer
	device.bindBufferMemory( *buffer, *buffer_memory, 0 );
}

static void copy_buffer( vk::Device device, vk::Queue transfer_queue, vk::CommandPool transfer_command_pool, vk::Buffer src_buffer, vk::Buffer dst_buffer, vk::DeviceSize bufferSize )
{
	// Command buffer to hold transfer commands
	vk::CommandBuffer transfer_command_buffer;

	// Command buffer details
	vk::CommandBufferAllocateInfo alloc_info {};
	alloc_info.level = vk::CommandBufferLevel::ePrimary;
	alloc_info.commandPool = transfer_command_pool;
	alloc_info.commandBufferCount = 1;

	// Allocate command buffer from pool
	transfer_command_buffer = device.allocateCommandBuffers( alloc_info ).front();

	// Information to begin command buffer record
	vk::CommandBufferBeginInfo begin_info {};
	// Only using command buffer once, then become unvalid
	begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	// Begin records transfer commands
	transfer_command_buffer.begin( begin_info );

	// Region of data to copy from and to
	vk::BufferCopy buffer_copy_region {};
	buffer_copy_region.srcOffset = 0;		// From the start of first buffer...
	buffer_copy_region.dstOffset = 0;		// ...copy to the start of second buffer
	buffer_copy_region.size = bufferSize;

	// Copy src buffer to dst buffer
	transfer_command_buffer.copyBuffer( src_buffer, dst_buffer, buffer_copy_region );

	// End record commands
	transfer_command_buffer.end();

	// Queue submission info
	vk::SubmitInfo submit_info {};
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &transfer_command_buffer;

	// Submit transfer commands to transfer queue and wait until it finishes
	transfer_queue.submit( submit_info );
	transfer_queue.waitIdle();

	// Free temporary command buffer
	device.freeCommandBuffers( transfer_command_pool, transfer_command_buffer );
}