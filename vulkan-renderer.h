#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

struct VulkanMainDevice
{
	vk::PhysicalDevice physical_device;
	vk::Device logical_device;
};

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
};

