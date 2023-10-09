#include "vulkan-renderer.h"

VulkanRenderer::VulkanRenderer( GLFWwindow* window )
	: window( window )
{}

VulkanRenderer::~VulkanRenderer()
{}

int VulkanRenderer::init()
{
	try
	{
		create_instance();
		retrieve_physical_device();
	}
	catch ( const std::runtime_error& err )
	{
		printf( "ERROR: %s\n", err.what() );
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void VulkanRenderer::release()
{
	instance.destroy();
}

void VulkanRenderer::create_instance()
{
	//  application info
	vk::ApplicationInfo app_info {};
	app_info.pApplicationName = "Vulkan-o";
	app_info.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
	app_info.pEngineName = "N/A";
	app_info.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
	app_info.apiVersion = VK_API_VERSION_1_1;

	//  vulkan creation info
	std::vector<const char*> instance_exts;
	vk::InstanceCreateInfo create_info {};
	create_info.pApplicationInfo = &app_info;

	//  setup extensions for glfw
	uint32_t glfw_ext_count = 0;
	const char** glfw_exts = glfwGetRequiredInstanceExtensions( &glfw_ext_count );
	for ( uint32_t i = 0; i < glfw_ext_count; i++ )
	{
		instance_exts.push_back( glfw_exts[i] );
	}

	//  extensions
	if ( !check_instance_extensions_support( instance_exts ) ) throw std::runtime_error( "VkInstance doesn't support required extensions!" );
	create_info.enabledExtensionCount = (uint32_t)instance_exts.size();
	create_info.ppEnabledExtensionNames = instance_exts.data();

	//  validation layers
	create_info.enabledLayerCount = 0;
	create_info.ppEnabledLayerNames = nullptr;

	//  create instance
	instance = vk::createInstance( create_info );
}

bool VulkanRenderer::check_instance_extensions_support( const std::vector<const char*>& extensions )
{
	auto supported_extensions = vk::enumerateInstanceExtensionProperties();

	for ( const auto& extension : extensions )
	{
		bool has_extension = false;
		for ( const auto& supported_extension : supported_extensions )
		{
			if ( strcmp( extension, supported_extension.extensionName ) == 0 )
			{
				has_extension = true;
				break;
			}
		}

		if ( !has_extension ) return false;
	}

	return true;
}

void VulkanRenderer::retrieve_physical_device()
{
	std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
	if ( devices.size() == 0 ) throw std::runtime_error( "Can't find any GPU that supports Vulkan" );

	for ( const auto& device : devices )
	{
		if ( check_device_suitable( device ) )
		{
			main_device.physical_device = device;
			break;
		}
	}
}

bool VulkanRenderer::check_device_suitable( vk::PhysicalDevice device )
{
	return false;
}

VulkanQueueFamilyIndices VulkanRenderer::get_queue_families( vk::PhysicalDevice device )
{
	return VulkanQueueFamilyIndices();
}
