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

	float angle = 0.0f;
	float dt = 0.0f;
	float last_time = 0.0f;

	while ( !glfwWindowShouldClose( window ) ) 
	{
		glfwPollEvents();

		//  compute delta time
		float current_time = glfwGetTime();
		dt = current_time - last_time;
		last_time = current_time;

		//  rotate model
		angle += 50.0 * dt;
		if ( angle > 360.0f ) angle -= 360.0f;

		//  update model

		//  draw
		renderer.draw();
	}

	release( window, renderer );
	return 0;
}