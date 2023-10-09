#pragma once

#include <fstream>

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