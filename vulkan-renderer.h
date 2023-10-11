#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "math-utils.hpp"
#include "vulkan-utils.hpp"
#include "vulkan-mesh.h"

struct ViewProjection
{
	glm::mat4 View;
	glm::mat4 Projection;
};

class VulkanRenderer
{
public:
	VulkanRenderer( GLFWwindow* window );
	~VulkanRenderer();

	int init();
	void release();

	void draw();

	VulkanMesh* create_mesh( std::vector<VulkanVertex>* vertices, std::vector<uint32_t>* indices );

private:
	GLFWwindow* Window;
	vk::Instance Instance;
	vk::SurfaceKHR Surface;
	vk::SwapchainKHR Swapchain;
	vk::Format SwapchainImageFormat;
	vk::Extent2D SwapchainExtent;
	std::vector<VulkanSwapchainImage> SwapchainImages;
	std::vector<vk::Framebuffer> SwapchainFrameBuffers;

	vk::Pipeline GraphicsPipeline;
	vk::CommandPool GraphicsCommandPool;
	std::vector<vk::CommandBuffer> CommandBuffers;
	vk::PipelineLayout PipelineLayout;
	vk::RenderPass RenderPass;

	std::vector<vk::Semaphore> ImageAvailableSemaphores;
	std::vector<vk::Semaphore> RenderFinishedSemaphores;
	std::vector<vk::Fence> DrawFences;

	vk::Queue GraphicsQueue;
	vk::Queue PresentationQueue;

	ViewProjection Matrices;
	std::vector<VulkanMesh> Meshes;
	vk::DescriptorPool DescriptorPool;
	vk::DescriptorSetLayout DescriptorSetLayout;
	std::vector<vk::DescriptorSet> DescriptorSets;

	std::vector<vk::Buffer> ViewProjUniformBuffers;
	std::vector<vk::DeviceMemory> ViewProjUniformBuffersMemory;

	std::vector<vk::Buffer> ModelUniformDynBuffers;
	std::vector<vk::DeviceMemory> ModelUniformDynBuffersMemory;

	const int MAX_OBJECTS = 2;
	vk::DeviceSize MinUniformBufferOffset;
	size_t ModelUniformAlignement;
	MeshData* ModelTransferSpace;

	const int MAX_FRAME_DRAWS = 2;  //  should be less than SwapchainImages count
	int CurrentFrame = 0;

	struct
	{
		vk::PhysicalDevice Physical;
		vk::Device Logical;
	} MainDevices;

	void create_instance();
	void create_logical_device();
	vk::SurfaceKHR create_surface();
	vk::ImageView create_image_view( vk::Image image, vk::Format format, vk::ImageAspectFlagBits aspect_flags );
	void create_swapchain();
	void create_graphics_pipeline();
	void create_render_pass();
	void create_frame_buffers();
	void create_graphics_command_pool();
	void create_graphics_command_buffers();
	void create_synchronisation();
	void create_descriptor_pool();
	void create_descriptor_set_layout();
	void create_descriptor_sets();
	void create_uniform_buffers();
	vk::ShaderModule create_shader_module( const std::vector<char>& code );
	
	void record_commands();

	bool check_instance_extensions_support( const std::vector<const char*>& extensions );
	bool check_validation_layer_support();

	void retrieve_physical_device();
	bool check_device_suitable( const vk::PhysicalDevice& device );
	bool check_device_extension_support( const vk::PhysicalDevice& device );
	
	void update_uniform_buffers( uint32_t image_idx );

	void allocate_dynamic_buffer_transfer_space();

	VulkanSwapchainDetails get_swapchain_details( const vk::PhysicalDevice& device );
	vk::SurfaceFormatKHR get_best_surface_format( const std::vector<vk::SurfaceFormatKHR>& formats );
	vk::PresentModeKHR get_best_presentation_mode( const std::vector<vk::PresentModeKHR>& modes );
	vk::Extent2D get_swap_extent( const vk::SurfaceCapabilitiesKHR& capabilities );
	
	VulkanQueueFamilyIndices get_queue_families( const vk::PhysicalDevice& device );
};

