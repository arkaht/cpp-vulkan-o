#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

#include "vulkan-renderer.h"

GLFWwindow* init_window( std::string title, int width, int height )
{
	glfwInit();
	glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
	glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );

	return glfwCreateWindow( width, height, title.c_str(), nullptr, nullptr );
}

void release( GLFWwindow* window, VulkanRenderer& renderer )
{
	renderer.release();

	glfwDestroyWindow( window );
	glfwTerminate();
}

int main() 
{
	GLFWwindow* window = init_window( "Vulkan-o", 1280, 720 );

	VulkanRenderer renderer( window );
	if ( renderer.init() == EXIT_FAILURE ) return EXIT_FAILURE;

	while ( !glfwWindowShouldClose( window ) ) 
	{
		glfwPollEvents();
		renderer.draw();
	}

	release( window, renderer );
	return 0;
}