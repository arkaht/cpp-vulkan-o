#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include "vulkan-utils.hpp"


class VulkanRenderer
{
public:
	VulkanRenderer( GLFWwindow* window );
	~VulkanRenderer();

	int init();
	void release();

private:
	GLFWwindow* window;
	vk::Instance instance;

	void create_instance();
	bool check_instance_extensions_support( const std::vector<const char*>& extensions );

	void retrieve_physical_device();
	bool check_device_suitable( vk::PhysicalDevice device );
	VulkanQueueFamilyIndices get_queue_families( vk::PhysicalDevice device );

	struct
	{
		vk::PhysicalDevice physical_device;
		vk::Device logical_device;
	} main_device;
};

