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
	glm::vec2 UV;
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

static void create_buffer( 
	vk::PhysicalDevice physical_device, 
	vk::Device device, 
	vk::DeviceSize buffer_size, 
	vk::BufferUsageFlags buffer_usage,
	vk::MemoryPropertyFlags buffer_properties, 
	vk::Buffer* buffer, 
	vk::DeviceMemory* buffer_memory 
)
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


static vk::CommandBuffer create_command_buffer( vk::Device device, vk::CommandPool commandPool )
{
	// Command buffer to hold transfer commands
	vk::CommandBuffer commandBuffer;

	// Command buffer details
	vk::CommandBufferAllocateInfo allocInfo {};
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	// Allocate command buffer from pool
	commandBuffer = device.allocateCommandBuffers( allocInfo ).front();

	// Information to begin command buffer record
	vk::CommandBufferBeginInfo beginInfo {};
	// Only using command buffer once, then become unvalid
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	// Begin records transfer commands
	commandBuffer.begin( beginInfo );
	return commandBuffer;
}

static void submit_command_buffer(
	vk::Device device,
	vk::CommandPool commandPool,
	vk::Queue queue,
	vk::CommandBuffer commandBuffer
)
{
	// End record commands
	commandBuffer.end();

	// Queue submission info
	vk::SubmitInfo submitInfo {};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Submit transfer commands to transfer queue and wait until it finishes
	queue.submit( 1, &submitInfo, nullptr );
	queue.waitIdle();

	// Free temporary command buffer
	device.freeCommandBuffers( commandPool, 1, &commandBuffer );
}

static void copy_buffer( 
	vk::Device device, 
	vk::Queue transfer_queue,
	vk::CommandPool transfer_command_pool, 
	vk::Buffer src_buffer, 
	vk::Buffer dst_buffer, 
	vk::DeviceSize bufferSize 
)
{
	// Command buffer to hold transfer commands
	vk::CommandBuffer transferCommandBuffer = create_command_buffer( device, transfer_command_pool );

	// Region of data to copy from and to
	vk::BufferCopy bufferCopyRegion {};
	bufferCopyRegion.srcOffset = 0;		// From the start of first buffer...
	bufferCopyRegion.dstOffset = 0;		// ...copy to the start of second buffer
	bufferCopyRegion.size = bufferSize;

	// Copy src buffer to dst buffer
	transferCommandBuffer.copyBuffer( src_buffer, dst_buffer, bufferCopyRegion );

	// Submit and free
	submit_command_buffer( 
		device, 
		transfer_command_pool,
		transfer_queue, 
		transferCommandBuffer 
	);
}

static void copy_image_buffer( vk::Device device, vk::Queue transferQueue,
	vk::CommandPool transferCommandPool, vk::Buffer srcBuffer, vk::Image dstImage,
	uint32_t width, uint32_t height )
{
	// Create buffer
	vk::CommandBuffer transferCommandBuffer = create_command_buffer( device, transferCommandPool );

	vk::BufferImageCopy imageRegion {};
	// All data of image is tightly packed
	// -- Offset into data
	imageRegion.bufferOffset = 0;
	// -- Row length of data to calculate data spacing
	imageRegion.bufferRowLength = 0;
	// -- Image height of data to calculate data spacing
	imageRegion.bufferImageHeight = 0;

	// Which aspect to copy (here: colors)
	imageRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	// Mipmap level to copy
	imageRegion.imageSubresource.mipLevel = 0;
	// Starting array layer if array
	imageRegion.imageSubresource.baseArrayLayer = 0;
	// Number of layers to copy starting at baseArray
	imageRegion.imageSubresource.layerCount = 1;
	// Offset into image (as opposed to raw data into bufferOffset)
	imageRegion.imageOffset = vk::Offset3D { 0, 0, 0 };
	// Size of region to copy (xyz values)
	imageRegion.imageExtent = vk::Extent3D { width, height, 1 };

	// Copy buffer to image
	transferCommandBuffer.copyBufferToImage( srcBuffer,
		dstImage, vk::ImageLayout::eTransferDstOptimal, 1, &imageRegion );

	submit_command_buffer( 
		device, 
		transferCommandPool,
		transferQueue, 
		transferCommandBuffer 
	);
}

static void transition_image_layout( 
	vk::Device device, 
	vk::Queue queue, 
	vk::CommandPool commandPool,
	vk::Image image, 
	vk::ImageLayout oldLayout, 
	vk::ImageLayout newLayout,
	uint32_t mip_levels
)
{
	vk::CommandBuffer commandBuffer = create_command_buffer( device, commandPool );

	vk::ImageMemoryBarrier imageMemoryBarrier {};
	imageMemoryBarrier.oldLayout = oldLayout;
	imageMemoryBarrier.newLayout = newLayout;
	// Queue family to transition from
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	// Queue family to transition to
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	// Image being accessed and modified as part fo barrier
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	// First mip level to start alterations on
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	// Number of mip levels to alter starting from baseMipLevel
	imageMemoryBarrier.subresourceRange.levelCount = mip_levels;
	// First layer to starts alterations on
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	// Number of layers to alter starting from baseArrayLayer
	imageMemoryBarrier.subresourceRange.layerCount = 1;

	vk::PipelineStageFlags srcStage;
	vk::PipelineStageFlags dstStage;

	// If transitioning from new image to image ready to receive data
	if ( oldLayout == vk::ImageLayout::eUndefined &&
		newLayout == vk::ImageLayout::eTransferDstOptimal )
	{
		// Memory access stage transition must happen after this stage
		imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eNone;
		// Memory access stage transition must happen before this stage
		imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		// Transfer from old layout to new layout has to occur after any
		// point of the top of the pipeline and before it attemps to to a
		// transfer write at the transfer stage of the pipeline
		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if ( oldLayout == vk::ImageLayout::eTransferDstOptimal &&
		newLayout == vk::ImageLayout::eShaderReadOnlyOptimal )
	{
		// Transfer is finished
		imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		// Before the shader reads
		imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}

	commandBuffer.pipelineBarrier(
		// Pipeline stages (match to src and dst AccessMasks)
		srcStage, dstStage,
		// Dependency flags
		{},
		// Memory barrier count and data
		0, nullptr,
		// Buffer memory barrier count and data
		0, nullptr,
		// Image memory barrier count and data
		1, &imageMemoryBarrier
	);

	submit_command_buffer( device, commandPool, queue, commandBuffer );
}

static void generate_mipmaps( 
	vk::PhysicalDevice phys_device,
	vk::Device device, 
	vk::Queue queue, 
	vk::CommandPool command_pool,
	vk::Image image, 
	vk::Format image_format,
	int32_t image_width, 
	int32_t image_height, 
	uint32_t mip_levels 
)
{
	//  check image format supports linear blitting
	vk::FormatProperties format_properties = phys_device.getFormatProperties( image_format );
	if ( !( format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear ) )
	{
		throw std::runtime_error( "Texture image format does not support linear blitting" );
	}

	vk::CommandBuffer command_buffer = create_command_buffer( device, command_pool );

	// The fields set below will remain the same for all barriers.
	// On the contrary, subresourceRange.miplevel, oldLayout, newLayout, srcAccessMask,
	// and dstAccessMask will be changed for each transition.
	vk::ImageMemoryBarrier barrier {};
	barrier.image = image;
	barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
	barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mip_width = image_width;
	int32_t mip_height = image_height;

	// This loop will record each of the blitImage commands.
	// Note that the loop variable starts at 1, not 0.
	for ( uint32_t i = 1; i < mip_levels; i++ )
	{
		// First, we transition level i - 1 to vk::ImageLayout::eTransferSrcOptimal.
		// This transition will wait for level i - 1 to be filled, either from the
		//  previous blit command, or from vk::CommandBuffer::copyBufferToImage.
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

		// The current blit command will wait on this transition.
		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eTransfer, {},
			0, nullptr, 0, nullptr, 1, &barrier );

		// Next, we specify the regions that will be used in the blit operation.
		// The source mip level is i - 1 and the destination mip level is i.
		// The two elements of the srcOffsets array determine the 3D region
		// that data will be blitted from. dstOffsets determines the region
		// that data will be blitted to.
		vk::ImageBlit blit {};
		blit.srcOffsets[0] = vk::Offset3D { 0, 0, 0 };
		blit.srcOffsets[1] = vk::Offset3D { mip_width, mip_height, 1 };
		blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		// The X and Y dimensions of the dstOffsets[1] are divided by two since each mip
		// level is half the size of the previous level.The Z dimension of
		// srcOffsets[1] and dstOffsets[1] must be 1, since a 2D image has
		// a depth of 1.
		blit.dstOffsets[0] = vk::Offset3D { 0, 0, 0 };
		blit.dstOffsets[1] = vk::Offset3D { mip_width > 1 ? mip_width / 2 : 1,
			mip_height > 1 ? mip_height / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;


		// Now, we record the blit command. Note that textureImage is used for both
		// the srcImage and dstImage parameter. This is because we're blitting between
		// different levels of the same image. The source mip level was just transitioned
		// to vk::ImageLayout::eTransferSrcOptimal and the destination level
		// is still in vk::ImageLayout::eTransferDstOptimal from createTextureImage.
		// The last parameter is the same filtering options here that we had when making
		// the vk::Sampler. We use the vk::Filter::eLinear to enable interpolation.
		command_buffer.blitImage(
			image, vk::ImageLayout::eTransferSrcOptimal,
			image, vk::ImageLayout::eTransferDstOptimal,
			1, &blit,
			vk::Filter::eLinear );

		// This barrier transitions mip level i - 1 to
		// vk::ImageLayout::eShaderReadOnlyOptimal. This transition waits on the
		// current blit command to finish. All sampling operations will wait
		// on this transition to finish.
		barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader, {},
			0, nullptr,
			0, nullptr,
			1, &barrier );

		// At the end of the loop, we divide the current mip dimensions by two. We
		// check each dimension before the division to ensure that dimension never
		// becomes 0. This handles cases where the image is not square, since one
		// of the mip dimensions would reach 1 before the other dimension. When
		// this happens, that dimension should remain 1 for all remaining
		// levels.
		if ( mip_width > 1 ) mip_width /= 2;
		if ( mip_height > 1 ) mip_height /= 2;
	}

	// Before we end the command buffer, we insert one more pipeline barrier. This barrier
	// transitions the last mip level from vk::ImageLayout::eTransferDstOptimal to
	// vk::ImageLayout::eShaderReadOnlyOptimal. This wasn't handled by the loop, since the
	// last mip level is never blitted from.
	barrier.subresourceRange.baseMipLevel = mip_levels - 1;
	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

	command_buffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eFragmentShader, {},
		0, nullptr,
		0, nullptr,
		1, &barrier );

	submit_command_buffer( device, command_pool, queue, command_buffer );
}