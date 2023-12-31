#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "stb_image.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "math-utils.hpp"
#include "vulkan-utils.hpp"
#include "vulkan-mesh.h"
#include "vulkan-mesh-model.h"

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

	VulkanMesh* create_mesh( 
		std::vector<VulkanVertex>* vertices, 
		std::vector<uint32_t>* indices,
		int texture_id
	);
	VulkanMeshModel* create_mesh_model( const std::string& file );
	void update_model( int id, glm::mat4 matrix );

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
	std::vector<VulkanMeshModel> MeshModels;
	vk::DescriptorPool ViewProjDescriptorPool;
	vk::DescriptorSetLayout DescriptorSetLayout;
	std::vector<vk::DescriptorSet> DescriptorSets;

	std::vector<vk::Buffer> ViewProjUniformBuffers;
	std::vector<vk::DeviceMemory> ViewProjUniformBuffersMemory;

	/*std::vector<vk::Buffer> ModelUniformDynBuffers;
	std::vector<vk::DeviceMemory> ModelUniformDynBuffersMemory;*/

	vk::PushConstantRange PushConstantRange;

	//  color
	vk::Image ColorImage;
	vk::DeviceMemory ColorImageMemory;
	vk::ImageView ColorImageView;

	//  depth
	vk::Image DepthBufferImage;
	vk::ImageView DepthBufferImageView;
	vk::DeviceMemory DepthBufferImageMemory;
	vk::Format DepthBufferFormat;

	//  textures
	std::vector<vk::Image> TextureImages;
	std::vector<vk::ImageView> TextureImageViews;
	std::vector<vk::DeviceMemory> TextureImageMemories;

	//  sampler
	vk::SampleCountFlagBits MSAASamples { vk::SampleCountFlagBits::e1 };
	vk::Sampler TextureSampler;
	vk::DescriptorPool SamplerDescriptorPool;
	vk::DescriptorSetLayout SamplerDescriptorSetLayout;
	std::vector<vk::DescriptorSet> SamplerDescriptorSets;

	const int MAX_OBJECTS = 20;
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
	vk::ImageView create_image_view( 
		vk::Image image, 
		vk::Format format, 
		vk::ImageAspectFlagBits aspect_flags,
		uint32_t mip_levels
	);
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
	void create_push_constant_range();
	void create_color_buffer_image();
	void create_depth_buffer_image();
	vk::ShaderModule create_shader_module( const std::vector<char>& code );

	vk::Image create_image( 
		uint32_t width,
		uint32_t height,
		uint32_t mip_levels,
		vk::SampleCountFlagBits samples,
		vk::Format format,
		vk::ImageTiling tiling,
		vk::ImageUsageFlags use_flags,
		vk::MemoryPropertyFlags prop_flags,
		vk::DeviceMemory* image_memory
	);
	int create_texture_image( const std::string& file, uint32_t* mip_levels );
	int create_texture( const std::string& file );
	void create_texture_sampler();
	int create_texture_descriptor( vk::ImageView image_view );

	void record_commands( uint32_t image_idx );

	bool check_instance_extensions_support( const std::vector<const char*>& extensions );
	bool check_validation_layer_support();

	void retrieve_physical_device();
	bool check_device_suitable( const vk::PhysicalDevice& device );
	bool check_device_extension_support( const vk::PhysicalDevice& device );
	
	void update_uniform_buffers( uint32_t image_idx );

	void allocate_dynamic_buffer_transfer_space();

	stbi_uc* load_texture_file( const std::string& path, int* width, int* height, vk::DeviceSize* image_size );

	VulkanSwapchainDetails get_swapchain_details( const vk::PhysicalDevice& device );
	vk::SurfaceFormatKHR get_best_surface_format( const std::vector<vk::SurfaceFormatKHR>& formats );
	vk::PresentModeKHR get_best_presentation_mode( const std::vector<vk::PresentModeKHR>& modes );
	vk::Extent2D get_swap_extent( const vk::SurfaceCapabilitiesKHR& capabilities );
	vk::Format select_supported_format( const std::vector<vk::Format>& formats, vk::ImageTiling tiling, vk::FormatFeatureFlags feature_flags );

	VulkanQueueFamilyIndices get_queue_families( const vk::PhysicalDevice& device );
};

