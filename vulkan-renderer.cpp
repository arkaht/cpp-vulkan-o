#include "vulkan-renderer.h"

#include <set>

VulkanRenderer::VulkanRenderer( GLFWwindow* window )
	: Window( window )
{}

VulkanRenderer::~VulkanRenderer()
{}

int VulkanRenderer::init()
{
	try
	{
		//  device
		create_instance();
		Surface = create_surface();
		retrieve_physical_device();
		create_logical_device();

		//  pipeline
		create_swapchain();
		create_render_pass();
		create_descriptor_set_layout();
		create_push_constant_range();
		create_graphics_pipeline();
		create_depth_buffer_image();
		create_frame_buffers();
		create_graphics_command_pool();

		//  objects
		float aspect_ratio = (float)SwapchainExtent.width / (float)SwapchainExtent.height;
		Matrices.Projection = glm::perspective( 
			glm::radians( 45.0f ), 
			aspect_ratio, 
			0.1f, 
			100.0f 
		);
		Matrices.Projection[1][1] *= -1.0f;  //  in Vulkan, Y is downward meanwhile in glm, it's upward
		Matrices.View = glm::lookAt( 
			glm::vec3( 2.0f, 1.0f, 2.0f ), 
			glm::vec3( 0.0f, 0.0f, -5.0f ), 
			glm::vec3( 0.0f, 1.0f, 0.0f ) 
		);

		std::vector<VulkanVertex> mesh_vertices1
		{
			{{-0.4f,  0.4f, 0.0f}, {1.0f, 0.0f, 0.0f}},	// 0
			{{-0.4f, -0.4f, 0.0f}, {0.0f, 1.0f, 0.0f}},	// 1
			{{ 0.4f, -0.4f, 0.0f}, {0.0f, 0.0f, 1.0f}},	// 2
			{{ 0.4f,  0.4f, 0.0f}, {1.0f, 1.0f, 0.0f}},	// 3
		};
		std::vector<VulkanVertex> mesh_vertices2
		{
			{{-0.2f,  0.6f, 0.0f}, {1.0f, 0.0f, 0.0f}},	// 0
			{{-0.2f, -0.6f, 0.0f}, {0.0f, 1.0f, 0.0f}},	// 1
			{{ 0.2f, -0.6f, 0.0f}, {0.0f, 0.0f, 1.0f}},	// 2
			{{ 0.2f,  0.6f, 0.0f}, {1.0f, 1.0f, 0.0f}},	// 3
		};
		std::vector<uint32_t> mesh_indices
		{
			0, 1, 2,
			2, 3, 0
		};
		VulkanMesh* mesh1 = create_mesh( &mesh_vertices1, &mesh_indices );
		VulkanMesh* mesh2 = create_mesh( &mesh_vertices2, &mesh_indices );

		//  data
		//allocate_dynamic_buffer_transfer_space();
		create_uniform_buffers();
		create_descriptor_pool();
		create_descriptor_sets();

		//  commands
		create_graphics_command_buffers();
		create_synchronisation();
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
	MainDevices.Logical.waitIdle();

	//  release models allocation
	_aligned_free( ModelTransferSpace );

	//  release meshes
	for ( auto& mesh : Meshes )
	{
		mesh.release_buffers();
	}
	Meshes.clear();

	//  release swapchain image views
	for ( auto& image : SwapchainImages )
	{
		MainDevices.Logical.destroyImageView( image.ImageView );
	}

	//  release swapchain image views
	for ( auto& framebuffer : SwapchainFrameBuffers )
	{
		MainDevices.Logical.destroyFramebuffer( framebuffer );
	}

	//  release semaphores
	for ( int i = 0; i < MAX_FRAME_DRAWS; i++ )
	{
		MainDevices.Logical.destroySemaphore( RenderFinishedSemaphores[i] );
		MainDevices.Logical.destroySemaphore( ImageAvailableSemaphores[i] );
		MainDevices.Logical.destroyFence( DrawFences[i] );
	}

	//  release buffers
	for ( int i = 0; i < ViewProjUniformBuffers.size(); i++ )
	{
		MainDevices.Logical.destroyBuffer( ViewProjUniformBuffers[i] );
		MainDevices.Logical.freeMemory( ViewProjUniformBuffersMemory[i] );
		/*MainDevices.Logical.destroyBuffer( ModelUniformDynBuffers[i] );
		MainDevices.Logical.freeMemory( ModelUniformDynBuffersMemory[i] );*/
	}

	//  release depth buffer
	MainDevices.Logical.destroyImageView( DepthBufferImageView );
	MainDevices.Logical.destroyImage( DepthBufferImage );
	MainDevices.Logical.freeMemory( DepthBufferImageMemory );

	//  release logical device
	MainDevices.Logical.destroyDescriptorPool( DescriptorPool );
	MainDevices.Logical.destroyDescriptorSetLayout( DescriptorSetLayout );
	MainDevices.Logical.destroyCommandPool( GraphicsCommandPool );
	MainDevices.Logical.destroyPipeline( GraphicsPipeline );
	MainDevices.Logical.destroyRenderPass( RenderPass );
	MainDevices.Logical.destroyPipelineLayout( PipelineLayout );
	MainDevices.Logical.destroySwapchainKHR( Swapchain );
	MainDevices.Logical.destroy();

	//  release instance
	Instance.destroySurfaceKHR( Surface );
	Instance.destroy();
}

void VulkanRenderer::draw()
{
	// 0. Freeze code until the drawFences[currentFrame] is open
	MainDevices.Logical.waitForFences( DrawFences[CurrentFrame], VK_TRUE, std::numeric_limits<uint32_t>::max() );
	MainDevices.Logical.resetFences( DrawFences[CurrentFrame] );

	// 1. Get next available image to draw and set a semaphore to signal
	// when we're finished with the image.
	uint32_t image_idx = MainDevices.Logical.acquireNextImageKHR(
		Swapchain,
		std::numeric_limits<uint32_t>::max(),
		ImageAvailableSemaphores[CurrentFrame],
		VK_NULL_HANDLE
	).value;

	record_commands( image_idx );
	update_uniform_buffers( image_idx );

	// 2. Submit command buffer to queue for execution, make sure it waits
	// for the image to be signaled as available before drawing, and
	// signals when it has finished rendering.
	vk::SubmitInfo submit_info {};
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &ImageAvailableSemaphores[CurrentFrame];

	// Keep doing command buffer until imageAvailable is true
	vk::PipelineStageFlags wait_stages[]
	{
		vk::PipelineStageFlagBits::eColorAttachmentOutput,
	};
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;

	// Command buffer to submit
	submit_info.pCommandBuffers = &CommandBuffers[image_idx];

	// Semaphores to signal when command buffer finishes
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &RenderFinishedSemaphores[CurrentFrame];
	GraphicsQueue.submit( submit_info, DrawFences[CurrentFrame] );

	// 3. Present image to screen when it has signalled finished rendering
	vk::PresentInfoKHR present_info {};
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &RenderFinishedSemaphores[CurrentFrame];
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &Swapchain;
	present_info.pImageIndices = &image_idx;
	PresentationQueue.presentKHR( present_info );

	//  increase frame
	CurrentFrame = ( CurrentFrame + 1 ) % MAX_FRAME_DRAWS;
}

VulkanMesh* VulkanRenderer::create_mesh( std::vector<VulkanVertex>* vertices, std::vector<uint32_t>* indices )
{
	VulkanMesh mesh(
		MainDevices.Physical,
		MainDevices.Logical,
		GraphicsQueue,
		GraphicsCommandPool,
		vertices,
		indices
	);

	Meshes.push_back( mesh );
	return &Meshes.back();
}

void VulkanRenderer::update_model( int id, glm::mat4 matrix )
{
	if ( id >= Meshes.size() ) return;

	Meshes[id].set_model_matrix( matrix );
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
	if ( VulkanEnableValidationLayers )
	{
		if ( !check_validation_layer_support() )
		{
			throw std::runtime_error( "Validation layers requested, but not available!" );
		}

		create_info.enabledLayerCount = (uint32_t)VulkanValidationLayers.size();
		create_info.ppEnabledLayerNames = VulkanValidationLayers.data();
	}
	else
	{
		create_info.enabledLayerCount = 0;
		create_info.ppEnabledLayerNames = nullptr;
	}

	//  create instance
	Instance = vk::createInstance( create_info );
}


void VulkanRenderer::create_logical_device()
{
	VulkanQueueFamilyIndices indices = get_queue_families( MainDevices.Physical );

	//  setup queues
	std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
	std::set<int> queue_family_indices = {
		indices.GraphicsFamily,
		indices.PresentationFamily,
	};

	for ( int index : queue_family_indices )
	{
		vk::DeviceQueueCreateInfo queue_create_info {};
		queue_create_info.queueFamilyIndex = index;
		queue_create_info.queueCount = 1;

		float priority = 1.0f;
		queue_create_info.pQueuePriorities = &priority;

		queue_create_infos.push_back( queue_create_info );
	}

	vk::DeviceCreateInfo device_create_info {};

	//  queues
	device_create_info.queueCreateInfoCount = (uint32_t)queue_create_infos.size();
	device_create_info.pQueueCreateInfos = queue_create_infos.data();
	//  extensions
	device_create_info.enabledExtensionCount = (uint32_t)VulkanDeviceExtensions.size();
	device_create_info.ppEnabledExtensionNames = VulkanDeviceExtensions.data();
	//  features
	vk::PhysicalDeviceFeatures device_features {};
	device_create_info.pEnabledFeatures = &device_features;

	//  create device
	MainDevices.Logical = MainDevices.Physical.createDevice( device_create_info );

	//  queue accesses
	GraphicsQueue = MainDevices.Logical.getQueue( indices.GraphicsFamily, 0 );
	PresentationQueue = MainDevices.Logical.getQueue( indices.PresentationFamily, 0 );
}

vk::SurfaceKHR VulkanRenderer::create_surface()
{
	VkSurfaceKHR surface;

	VkResult result = glfwCreateWindowSurface( Instance, Window, nullptr, &surface );
	if ( result != VK_SUCCESS )
	{
		throw std::runtime_error( "Failed to create a VkSurface!" );
	}

	return surface;
}

vk::ImageView VulkanRenderer::create_image_view( vk::Image image, vk::Format format, vk::ImageAspectFlagBits aspect_flags )
{
	vk::ImageViewCreateInfo create_info {};
	create_info.image = image;
	create_info.format = format;
	create_info.viewType = vk::ImageViewType::e2D;

	create_info.components.r = vk::ComponentSwizzle::eIdentity;
	create_info.components.g = vk::ComponentSwizzle::eIdentity;
	create_info.components.b = vk::ComponentSwizzle::eIdentity;
	create_info.components.a = vk::ComponentSwizzle::eIdentity;

	create_info.subresourceRange.aspectMask = aspect_flags;
	create_info.subresourceRange.baseMipLevel = 0;
	create_info.subresourceRange.levelCount = 1;
	create_info.subresourceRange.baseArrayLayer = 0;
	create_info.subresourceRange.layerCount = 1;

	return MainDevices.Logical.createImageView( create_info );
}

void VulkanRenderer::create_swapchain()
{
	VulkanSwapchainDetails details = get_swapchain_details( MainDevices.Physical );

	//  get best settings
	vk::SurfaceFormatKHR surface_format = get_best_surface_format( details.Formats );
	vk::PresentModeKHR presentation_mode = get_best_presentation_mode( details.PresentationModes );
	vk::Extent2D extent = get_swap_extent( details.SurfaceCapabilities );

	//  setup create info
	vk::SwapchainCreateInfoKHR create_info {};
	create_info.surface = Surface;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.presentMode = presentation_mode;
	create_info.imageExtent = extent;

	//  set number of images (triple-buffering)
	uint32_t image_count = details.SurfaceCapabilities.minImageCount + 1;
	if ( details.SurfaceCapabilities.maxImageCount > 0  //  not limitless
		&& details.SurfaceCapabilities.maxImageCount < image_count )
	{
		image_count = details.SurfaceCapabilities.maxImageCount;
	}
	create_info.minImageCount = image_count;

	create_info.imageArrayLayers = 1;  //  layers per image
	create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;  //  image attachment (e.g. depth, stencil)
	create_info.preTransform = details.SurfaceCapabilities.currentTransform;
	create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;  //  window blend
	create_info.clipped = VK_TRUE;

	//  queue management
	VulkanQueueFamilyIndices indices = get_queue_families( MainDevices.Physical );
	uint32_t family_indices[]
	{
		(uint32_t)indices.GraphicsFamily,
		(uint32_t)indices.PresentationFamily
	};

	//  share images between these families queues
	if ( indices.GraphicsFamily != indices.PresentationFamily )
	{
		create_info.imageSharingMode = vk::SharingMode::eConcurrent;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = family_indices;
	}
	else
	{
		create_info.imageSharingMode = vk::SharingMode::eExclusive;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = nullptr;
	}

	create_info.oldSwapchain = VK_NULL_HANDLE;  //  has something to do w/ window resizing and more..

	//  create swapchain
	Swapchain = MainDevices.Logical.createSwapchainKHR( create_info );
	SwapchainImageFormat = surface_format.format;
	SwapchainExtent = extent;

	//  retrieve swapchain images
	std::vector<vk::Image> images = MainDevices.Logical.getSwapchainImagesKHR( Swapchain );
	for ( vk::Image image : images )
	{
		VulkanSwapchainImage swapchain_image {};
		swapchain_image.Image = image;
		swapchain_image.ImageView = create_image_view( image, SwapchainImageFormat, vk::ImageAspectFlagBits::eColor );
		SwapchainImages.push_back( swapchain_image );
	}
}

void VulkanRenderer::create_graphics_pipeline()
{
	//  read code
	auto vertex_code = read_shader_file( "shaders/vert.spv" );
	auto fragment_code = read_shader_file( "shaders/frag.spv" );

	//  create shader modules
	vk::ShaderModule vertex_module = create_shader_module( vertex_code );
	vk::ShaderModule fragment_module = create_shader_module( fragment_code );

	//  vertex shader create info
	vk::PipelineShaderStageCreateInfo vertex_shader_create_info {};
	vertex_shader_create_info.stage = vk::ShaderStageFlagBits::eVertex;
	vertex_shader_create_info.module = vertex_module;
	vertex_shader_create_info.pName = "main";  //  pointer to main function

	//  fragment shader create info
	vk::PipelineShaderStageCreateInfo fragment_shader_create_info {};
	fragment_shader_create_info.stage = vk::ShaderStageFlagBits::eFragment;
	fragment_shader_create_info.module = fragment_module;
	fragment_shader_create_info.pName = "main";  //  pointer to main function

	//  setup pipeline
	vk::PipelineShaderStageCreateInfo stages[]
	{
		vertex_shader_create_info,
		fragment_shader_create_info,
	};

	//  create pipeline
	// Vertex description
	// -- Binding, data layout
	vk::VertexInputBindingDescription binding_description {};
	// Binding position. Can bind multiple streams of data.
	binding_description.binding = 0;
	// Size of a single vertex data object, like in OpenGL
	binding_description.stride = sizeof( VulkanVertex );
	// How ot move between data after each vertex.
	// vk::VertexInputRate::eVertex: move onto next vertex
	// vk::VertexInputRate::eInstance: move to a vertex for the next instance.
	// Draw each first vertex of each instance, then the next vertex etc.
	binding_description.inputRate = vk::VertexInputRate::eVertex;

	// Different attributes
	std::array<vk::VertexInputAttributeDescription, 2> attribute_descriptions;

	// Position attributes
	// -- Binding of first attribute. Relate to binding description.
	attribute_descriptions[0].binding = 0;
	// Location in shader
	attribute_descriptions[0].location = 0;
	// Format and size of the data(here: vec3)
	attribute_descriptions[0].format = vk::Format::eR32G32B32Sfloat;
	// Offset of data in vertex, like in OpenGL. The offset function automatically find it.
	attribute_descriptions[0].offset = offsetof( VulkanVertex, Position );

	// Color attributes
	attribute_descriptions[1].binding = 0;
	attribute_descriptions[1].location = 1;
	attribute_descriptions[1].format = vk::Format::eR32G32B32Sfloat;
	attribute_descriptions[1].offset = offsetof( VulkanVertex, Color );

	// -- VERTEX INPUT STAGE --
	vk::PipelineVertexInputStateCreateInfo vertex_input_create_info {};
	vertex_input_create_info.vertexBindingDescriptionCount = 1;
	// List of vertex binding desc. (data spacing, stride...)
	vertex_input_create_info.vertexAttributeDescriptionCount = (uint32_t)attribute_descriptions.size();
	vertex_input_create_info.pVertexBindingDescriptions = &binding_description;
	// List of vertex attribute desc. (data format and where to bind to/from)
	vertex_input_create_info.pVertexAttributeDescriptions = attribute_descriptions.data();

	// -- INPUT ASSEMBLY --
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_create_info {};
	// How to assemble vertices
	input_assembly_create_info.topology = vk::PrimitiveTopology::eTriangleList;
	// When you want to restart a primitive, e.g. with a strip
	input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

	// -- VIEWPORT AND SCISSOR --
	// Create a viewport info struct
	vk::Viewport viewport {};
	viewport.x = 0.0f; // X start coordinate
	viewport.y = 0.0f; // Y start coordinate
	viewport.width = (float)SwapchainExtent.width; // Width of viewport
	viewport.height = (float)SwapchainExtent.height; // Height of viewport
	viewport.minDepth = 0.0f; // Min framebuffer depth
	viewport.maxDepth = 1.0f; // Max framebuffer depth

	// Create a scissor info struct, everything outside is cut
	vk::Rect2D scissor {};
	scissor.offset = vk::Offset2D { 0, 0 };
	scissor.extent = SwapchainExtent;

	vk::PipelineViewportStateCreateInfo viewport_state_create_info {};
	viewport_state_create_info.viewportCount = 1;

	viewport_state_create_info.pViewports = &viewport;
	viewport_state_create_info.scissorCount = 1;
	viewport_state_create_info.pScissors = &scissor;

	// -- DYNAMIC STATE --
	// This will be alterable, so you don't have to create an entire pipeline when you want to change
	// parameters. We won't use this feature, this is an example.
	/*
	vector<vk::DynamicState> dynamicStateEnables;
	// Viewport can be resized in the command buffer
	// with vkCmdSetViewport(commandBuffer, 0, 1, &newViewport);
	dynamicStateEnables.push_back(vk::DynamicState::eViewport);
	// Scissors can be resized in the command buffer
	// with vkCmdSetScissor(commandBuffer, 0, 1, &newScissor);
	dynamicStateEnables.push_back(vk::DynamicState::eScissor);
	vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	dynamicStateCreateInfo.dynamicStateCount =
	static_cast<uint32_t>(dynamicStateEnables.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();
	*/

	// -- RASTERIZER --
	vk::PipelineRasterizationStateCreateInfo rasterizer_create_info {};
	// Treat elements beyond the far plane like being on the far place,
	// needs a GPU device feature
	rasterizer_create_info.depthClampEnable = VK_FALSE;
	// Whether to discard data and skip rasterizer. When you want
	// a pipeline without framebuffer.
	rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE;
	// How to handle filling points between vertices. Here, considers things inside
	// the polygon as a fragment. VK_POLYGON_MODE_LINE will consider element inside
	// polygones being empty (no fragment). May require a device feature.
	rasterizer_create_info.polygonMode = vk::PolygonMode::eFill;
	// How thick should line be when drawn
	rasterizer_create_info.lineWidth = 1.0f;
	// Culling. Do not draw back of polygons
	rasterizer_create_info.cullMode = vk::CullModeFlagBits::eBack;

	// Widing to know the front face of a polygon
	rasterizer_create_info.frontFace = vk::FrontFace::eCounterClockwise;
	// Whether to add a depth offset to fragments. Good for stopping
	// "shadow acne" in shadow mapping. Is set, need to set 3 other values.
	rasterizer_create_info.depthBiasEnable = VK_FALSE;

	// -- MULTISAMPLING --
	// Not for textures, only for edges
	vk::PipelineMultisampleStateCreateInfo multisampling_create_info {};
	// Enable multisample shading or not
	multisampling_create_info.sampleShadingEnable = VK_FALSE;
	// Number of samples to use per fragment
	multisampling_create_info.rasterizationSamples = vk::SampleCountFlagBits::e1;

	// -- BLENDING --
	// How to blend a new color being written to the fragment, with the old value
	vk::PipelineColorBlendStateCreateInfo color_blending_create_info {};
	// Alternative to usual blending calculation
	color_blending_create_info.logicOpEnable = VK_FALSE;
	// Enable blending and choose colors to apply blending to
	vk::PipelineColorBlendAttachmentState color_blend_attachment {};
	color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR |
		vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
		vk::ColorComponentFlagBits::eA;
	color_blend_attachment.blendEnable = VK_TRUE;
	// Blending equation:
	// (srcColorBlendFactor * new color) colorBlendOp (dstColorBlendFactor * old color)
	color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
	color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	color_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
	// Replace the old alpha with the new one: (1 * new alpha) + (0 * old alpha)
	color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
	color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
	color_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;

	color_blending_create_info.attachmentCount = 1;
	color_blending_create_info.pAttachments = &color_blend_attachment;

	// -- PIPELINE LAYOUT --
	// TODO: apply future descriptorset layout
	vk::PipelineLayoutCreateInfo pipeline_layout_create_info {};
	pipeline_layout_create_info.setLayoutCount = 1;
	pipeline_layout_create_info.pSetLayouts = &DescriptorSetLayout;
	pipeline_layout_create_info.pushConstantRangeCount = 1;
	pipeline_layout_create_info.pPushConstantRanges = &PushConstantRange;

	// Create pipeline layout
	PipelineLayout = MainDevices.Logical.createPipelineLayout( pipeline_layout_create_info );

	// -- DEPTH STENCIL TESTING --
	vk::PipelineDepthStencilStateCreateInfo depth_stencil_create_info {};
	depth_stencil_create_info.depthTestEnable = true;
	depth_stencil_create_info.depthWriteEnable = true;
	depth_stencil_create_info.depthCompareOp = vk::CompareOp::eLess;
	depth_stencil_create_info.depthBoundsTestEnable = false;
	depth_stencil_create_info.stencilTestEnable = false;

	// -- PASSES --
	// Passes are composed of a sequence of subpasses that can pass data from one to another

	// -- GRAPHICS PIPELINE CREATION --
	vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info {};
	graphics_pipeline_create_info.stageCount = 2;
	graphics_pipeline_create_info.pStages = stages;
	graphics_pipeline_create_info.pVertexInputState = &vertex_input_create_info;
	graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
	graphics_pipeline_create_info.pViewportState = &viewport_state_create_info;
	graphics_pipeline_create_info.pDynamicState = nullptr;
	graphics_pipeline_create_info.pRasterizationState = &rasterizer_create_info;
	graphics_pipeline_create_info.pMultisampleState = &multisampling_create_info;
	graphics_pipeline_create_info.pColorBlendState = &color_blending_create_info;
	graphics_pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
	graphics_pipeline_create_info.layout = PipelineLayout;
	// Renderpass description the pipeline is compatible with.
	// This pipeline will be used by the render pass.
	graphics_pipeline_create_info.renderPass = RenderPass;
	// Subpass of render pass to use with pipeline. Usually one pipeline by subpass.
	graphics_pipeline_create_info.subpass = 0;
	// When you want to derivate a pipeline from an other pipeline OR
	graphics_pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
	// Index of pipeline being created to derive from (in case of creating multiple at once)
	graphics_pipeline_create_info.basePipelineIndex = -1;

	// The handle is a cache when you want to save your pipeline to create an other later
	auto result = MainDevices.Logical.createGraphicsPipeline( VK_NULL_HANDLE, graphics_pipeline_create_info );
	// We could have used createGraphicsPipelines to create multiple pipelines at once.
	if ( result.result != vk::Result::eSuccess ) throw std::runtime_error( "Cound not create a graphics pipeline" );
	GraphicsPipeline = result.value;

	//  destroy modules
	MainDevices.Logical.destroyShaderModule( vertex_module );
	MainDevices.Logical.destroyShaderModule( fragment_module );
}

void VulkanRenderer::create_render_pass()
{
	vk::RenderPassCreateInfo render_pass_create_info {};

	// Attachement description : describe color buffer output, depth buffer output...
	// e.g. (location = 0) in the fragment shader is the first attachment
	vk::AttachmentDescription color_attachment {};
	// Format to use for attachment
	color_attachment.format = SwapchainImageFormat;
	// Number of samples to write for multisampling
	color_attachment.samples = vk::SampleCountFlagBits::e1;
	// What to do with attachement before renderer. Here, clear when we start the render pass.
	color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
	// What to do with attachement after renderer. Here, store the render pass.
	color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
	// What to do with stencil before renderer. Here, don't care, we don't use stencil.
	color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	// What to do with stencil after renderer. Here, don't care, we don't use stencil.
	color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	
	// Framebuffer images will be stored as an image, but image can have different layouts
	// to give optimal use for certain operations
	// Image data layout before render pass starts
	color_attachment.initialLayout = vk::ImageLayout::eUndefined;
	// Image data layout after render pass
	color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

	//  select depth format
	std::vector<vk::Format> formats {
		vk::Format::eD32SfloatS8Uint,
		vk::Format::eD32Sfloat,
		vk::Format::eD24UnormS8Uint
	};
	DepthBufferFormat = select_supported_format(
		formats,
		vk::ImageTiling::eOptimal,
		vk::FormatFeatureFlagBits::eDepthStencilAttachment
	);

	//  depth attachment
	vk::AttachmentDescription depth_attachment {};
	depth_attachment.format = DepthBufferFormat;
	depth_attachment.samples = vk::SampleCountFlagBits::e1;
	depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
	depth_attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
	depth_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	depth_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	depth_attachment.initialLayout = vk::ImageLayout::eUndefined;
	depth_attachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	std::vector<vk::AttachmentDescription> render_pass_attachments
	{
		color_attachment,
		depth_attachment,
	};

	render_pass_create_info.attachmentCount = (uint32_t)render_pass_attachments.size();
	render_pass_create_info.pAttachments = render_pass_attachments.data();

	// Attachment reference uses an attachment index that refers to index
	// in the attachement list passed to renderPassCreateInfo
	vk::AttachmentReference color_attachment_reference {};
	color_attachment_reference.attachment = 0;
	// Layout of the subpass (between initial and final layout)
	color_attachment_reference.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::AttachmentReference depth_attachment_reference {};
	depth_attachment_reference.attachment = 1;
	depth_attachment_reference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	// Subpass description, will reference attachements
	vk::SubpassDescription subpass {};
	// Pipeline type the subpass will be bound to.
	// Could be compute pipeline, or nvidia raytracing...
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_reference;
	subpass.pDepthStencilAttachment = &depth_attachment_reference;

	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses = &subpass;

	// Subpass dependencies: transitions between subpasses + from the last subpass to what
	// happens after. Need to determine when layout transitions occur using subpass
	// dependencies. Will define implicitly layout transitions.
	std::array<vk::SubpassDependency, 2> subpass_dependencies;
	// -- From layout undefined to color attachment optimal
	// ---- Transition must happens after
	// External: from outside the subpasses
	subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	// Which stage of the pipeline has to happen before
	subpass_dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
	subpass_dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
	// ---- But must happens before
	// Conversion should happen before the first subpass starts
	subpass_dependencies[0].dstSubpass = 0;
	subpass_dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	// ...and before the color attachment attempts to read or write
	subpass_dependencies[0].dstAccessMask =
		vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
	subpass_dependencies[0].dependencyFlags = {}; // No dependency flag
	// -- From layout color attachment optimal to image layout present
	// ---- Transition must happens after
	subpass_dependencies[1].srcSubpass = 0;
	subpass_dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	subpass_dependencies[1].srcAccessMask =
		vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
	// ---- But must happens before
	subpass_dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpass_dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
	subpass_dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
	subpass_dependencies[1].dependencyFlags = vk::DependencyFlags();

	render_pass_create_info.dependencyCount = static_cast<uint32_t>( subpass_dependencies.size() );
	render_pass_create_info.pDependencies = subpass_dependencies.data();

	RenderPass = MainDevices.Logical.createRenderPass( render_pass_create_info );
}

void VulkanRenderer::create_frame_buffers()
{
	SwapchainFrameBuffers.resize( SwapchainImages.size() );

	for ( size_t i = 0; i < SwapchainFrameBuffers.size(); i++ )
	{
		//  setup attachments
		std::vector<vk::ImageView> attachments
		{
			SwapchainImages[i].ImageView,
			DepthBufferImageView
		};

		//  create info
		vk::FramebufferCreateInfo framebuffer_create_info {};
		framebuffer_create_info.renderPass = RenderPass;
		framebuffer_create_info.attachmentCount = (uint32_t)attachments.size();
		framebuffer_create_info.pAttachments = attachments.data();
		framebuffer_create_info.width = SwapchainExtent.width;
		framebuffer_create_info.height = SwapchainExtent.height;
		framebuffer_create_info.layers = 1;

		SwapchainFrameBuffers[i] = MainDevices.Logical.createFramebuffer( framebuffer_create_info );
	}
}

void VulkanRenderer::create_graphics_command_pool()
{
	VulkanQueueFamilyIndices indices = get_queue_families( MainDevices.Physical );

	vk::CommandPoolCreateInfo create_info {};
	create_info.queueFamilyIndex = indices.GraphicsFamily;
	create_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	create_info.sType = vk::StructureType::eCommandPoolCreateInfo;

	GraphicsCommandPool = MainDevices.Logical.createCommandPool( create_info );
}

void VulkanRenderer::create_graphics_command_buffers()
{
	CommandBuffers.resize( SwapchainFrameBuffers.size() );

	vk::CommandBufferAllocateInfo alloc_info {};
	alloc_info.commandPool = GraphicsCommandPool;
	alloc_info.commandBufferCount = (uint32_t)CommandBuffers.size();
	// Primary means the command buffer will submit directly to a queue.
	// Secondary cannot be called by a queue, but by an other primary command
	// buffer, via vkCmdExecuteCommands.
	alloc_info.level = vk::CommandBufferLevel::ePrimary;

	CommandBuffers = MainDevices.Logical.allocateCommandBuffers( alloc_info );
}

void VulkanRenderer::create_synchronisation()
{
	ImageAvailableSemaphores.resize( MAX_FRAME_DRAWS );
	RenderFinishedSemaphores.resize( MAX_FRAME_DRAWS );
	DrawFences.resize( MAX_FRAME_DRAWS );

	vk::SemaphoreCreateInfo semaphore_create_info {};

	vk::FenceCreateInfo fence_create_info {};
	fence_create_info.flags = vk::FenceCreateFlagBits::eSignaled;  //  starts open

	for ( int i = 0; i < MAX_FRAME_DRAWS; i++ )
	{
		ImageAvailableSemaphores[i] = MainDevices.Logical.createSemaphore( semaphore_create_info );
		RenderFinishedSemaphores[i] = MainDevices.Logical.createSemaphore( semaphore_create_info );
		DrawFences[i] = MainDevices.Logical.createFence( fence_create_info );
	}
}

void VulkanRenderer::create_descriptor_pool()
{
	vk::DescriptorPoolSize vp_pool_size {};
	vp_pool_size.descriptorCount = (uint32_t)ViewProjUniformBuffers.size();

	/*vk::DescriptorPoolSize model_pool_size {};
	model_pool_size.descriptorCount = (uint32_t)ModelUniformDynBuffers.size();*/

	std::vector<vk::DescriptorPoolSize> pool_sizes
	{
		vp_pool_size,
		//model_pool_size
	};

	vk::DescriptorPoolCreateInfo pool_create_info {};
	pool_create_info.maxSets = (uint32_t)SwapchainImages.size();
	pool_create_info.poolSizeCount = (uint32_t)pool_sizes.size();
	pool_create_info.pPoolSizes = pool_sizes.data();

	DescriptorPool = MainDevices.Logical.createDescriptorPool( pool_create_info );
}

void VulkanRenderer::create_descriptor_set_layout()
{
	//  view projection
	vk::DescriptorSetLayoutBinding vp_layout_binding;
	// Binding number in shader
	vp_layout_binding.binding = 0;
	// Type of descriptor (uniform, dynamic uniform, samples...)
	vp_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
	// Number of descriptors for binding
	vp_layout_binding.descriptorCount = 1;
	// Shader stage to bind to (here: vertex shader)
	vp_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;
	// For textures : can make sample data un changeable
	vp_layout_binding.pImmutableSamplers = nullptr;

	//  model
	/*vk::DescriptorSetLayoutBinding model_layout_binding;
	model_layout_binding.binding = 1;
	model_layout_binding.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
	model_layout_binding.descriptorCount = 1;
	model_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;
	model_layout_binding.pImmutableSamplers = nullptr;*/

	std::vector<vk::DescriptorSetLayoutBinding> layout_bindings
	{
		vp_layout_binding,
		//model_layout_binding
	};

	// Descriptor set layout with given binding
	vk::DescriptorSetLayoutCreateInfo layout_create_info {};
	layout_create_info.bindingCount = (uint32_t)layout_bindings.size();
	layout_create_info.pBindings = layout_bindings.data();

	// Create descriptor set layout
	DescriptorSetLayout = MainDevices.Logical.createDescriptorSetLayout( layout_create_info );
}

void VulkanRenderer::create_descriptor_sets()
{
	DescriptorSets.resize( SwapchainImages.size() );

	//  populate from set layouts
	std::vector<vk::DescriptorSetLayout> layouts( SwapchainImages.size(), DescriptorSetLayout );

	//  allocate descriptor sets
	vk::DescriptorSetAllocateInfo set_alloc_info {};
	set_alloc_info.descriptorPool = DescriptorPool;
	set_alloc_info.descriptorSetCount = (uint32_t)SwapchainImages.size();
	set_alloc_info.pSetLayouts = layouts.data();
	
	vk::Result result = MainDevices.Logical.allocateDescriptorSets( &set_alloc_info, DescriptorSets.data() );
	if ( result != vk::Result::eSuccess )
	{
		throw std::runtime_error( "Failed to allocate descriptor sets!" );
	}

	for ( int i = 0; i < ViewProjUniformBuffers.size(); i++ )
	{
		//  view proj descriptor
		vk::DescriptorBufferInfo vp_buffer_info {};
		vp_buffer_info.buffer = ViewProjUniformBuffers[i];
		vp_buffer_info.offset = 0;
		vp_buffer_info.range = sizeof( ViewProjection );

		vk::WriteDescriptorSet vp_set_write {};
		// Descriptor sets to update
		vp_set_write.dstSet = DescriptorSets[i];
		// Binding to update (matches with shader binding)
		vp_set_write.dstBinding = 0;
		// Index in array to update
		vp_set_write.dstArrayElement = 0;
		vp_set_write.descriptorType = vk::DescriptorType::eUniformBuffer;
		// Amount of descriptor sets to update
		vp_set_write.descriptorCount = 1;
		// Information about buffer data to bind
		vp_set_write.pBufferInfo = &vp_buffer_info;

		//  model descriptor
		/*vk::DescriptorBufferInfo model_buffer_info {};
		model_buffer_info.buffer = ModelUniformDynBuffers[i];
		model_buffer_info.offset = 0;
		model_buffer_info.range = ModelUniformAlignement;

		vk::WriteDescriptorSet model_set_write {};
		model_set_write.dstSet = DescriptorSets[i];
		model_set_write.dstBinding = 1;
		model_set_write.dstArrayElement = 0;
		model_set_write.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
		model_set_write.descriptorCount = 1;
		model_set_write.pBufferInfo = &model_buffer_info;*/

		std::vector<vk::WriteDescriptorSet> set_writes
		{
			vp_set_write,
			//model_set_write
		};

		// Update descriptor set with new buffer/binding info
		MainDevices.Logical.updateDescriptorSets( 
			(uint32_t)set_writes.size(), 
			set_writes.data(), 
			0, 
			nullptr
		);
	}
}

void VulkanRenderer::create_uniform_buffers()
{
	vk::DeviceSize vp_buffer_size = sizeof( ViewProjection );
	//vk::DeviceSize model_buffer_size = ModelUniformAlignement * MAX_OBJECTS;

	size_t size = SwapchainImages.size();
	ViewProjUniformBuffers.resize( size );
	ViewProjUniformBuffersMemory.resize( size );
	/*ModelUniformDynBuffers.resize( size );
	ModelUniformDynBuffersMemory.resize( size );*/

	for ( int i = 0; i < size; i++ )
	{
		//  view proj buffers
		create_buffer( 
			MainDevices.Physical, 
			MainDevices.Logical, 
			vp_buffer_size, 
			vk::BufferUsageFlagBits::eUniformBuffer, 
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
			&ViewProjUniformBuffers[i],
			&ViewProjUniformBuffersMemory[i]
		);

		//  model buffers
		/*create_buffer(
			MainDevices.Physical,
			MainDevices.Logical,
			model_buffer_size,
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			&ModelUniformDynBuffers[i],
			&ModelUniformDynBuffersMemory[i]
		);*/
	}
}

void VulkanRenderer::create_push_constant_range()
{
	PushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
	PushConstantRange.offset = 0;
	PushConstantRange.size = sizeof( MeshData );
}

void VulkanRenderer::create_depth_buffer_image()
{
	std::vector<vk::Format> formats
	{
		vk::Format::eD32SfloatS8Uint,
		vk::Format::eD32Sfloat,
		vk::Format::eD24UnormS8Uint,
	};

	vk::Format format = select_supported_format( 
		formats, 
		vk::ImageTiling::eOptimal,
		vk::FormatFeatureFlagBits::eDepthStencilAttachment
	);

	DepthBufferImage = create_image(
		SwapchainExtent.width,
		SwapchainExtent.height,
		format,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		&DepthBufferImageMemory
	);
	DepthBufferImageView = create_image_view(
		DepthBufferImage,
		format,
		vk::ImageAspectFlagBits::eDepth
	);
}

vk::ShaderModule VulkanRenderer::create_shader_module( const std::vector<char>& code )
{
	vk::ShaderModuleCreateInfo create_info {};
	create_info.codeSize = code.size();
	create_info.pCode = reinterpret_cast<const uint32_t*>( code.data() );

	return MainDevices.Logical.createShaderModule( create_info );
}

vk::Image VulkanRenderer::create_image(
	uint32_t width, 
	uint32_t height, 
	vk::Format format,
	vk::ImageTiling tiling,
	vk::ImageUsageFlags use_flags, 
	vk::MemoryPropertyFlags prop_flags,
	vk::DeviceMemory* image_memory 
)
{
	vk::ImageCreateInfo image_create_info {};
	image_create_info.imageType = vk::ImageType::e2D;
	image_create_info.extent.width = width;
	image_create_info.extent.height = height;
	// Depth is 1, no 3D aspect
	image_create_info.extent.depth = 1;
	image_create_info.mipLevels = 1;
	// Number of levels in image array
	image_create_info.arrayLayers = 1;
	image_create_info.format = format;
	// How image data should be "tiled" (arranged for optimal reading)
	image_create_info.tiling = tiling;
	// Initial layout in the render pass
	image_create_info.initialLayout = vk::ImageLayout::eUndefined;
	// Bit flags defining what image will be used for
	image_create_info.usage = use_flags;
	// Number of samples for multi sampling
	image_create_info.samples = vk::SampleCountFlagBits::e1;
	// Whether image can be shared between queues (no)
	image_create_info.sharingMode = vk::SharingMode::eExclusive;

	// Create the header of the image
	vk::Image image = MainDevices.Logical.createImage( image_create_info );

	// Now we need to setup and allocate memory for the image
	vk::MemoryRequirements memory_requirements = MainDevices.Logical.getImageMemoryRequirements( image );

	vk::MemoryAllocateInfo memoryAllocInfo {};
	memoryAllocInfo.allocationSize = memory_requirements.size;
	memoryAllocInfo.memoryTypeIndex = find_memory_type_index( 
		MainDevices.Physical,
		memory_requirements.memoryTypeBits, 
		prop_flags 
	);

	auto result = MainDevices.Logical.allocateMemory(
		&memoryAllocInfo, 
		nullptr, 
		image_memory 
	);
	if ( result != vk::Result::eSuccess )
	{
		throw std::runtime_error( "Failed to allocate memory for an image." );
	}

	// Connect memory to image
	MainDevices.Logical.bindImageMemory( image, *image_memory, 0 );

	return image;
}

void VulkanRenderer::record_commands( uint32_t image_idx )
{
	// How to begin each command buffer
	vk::CommandBufferBeginInfo buffer_begin_info {};
	// Buffer can be resubmited when it has already been submited
	//buffer_begin_info.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

	// Information about how to being a render pass (only for graphical apps)
	vk::RenderPassBeginInfo render_pass_begin_info {};
	// Render pass to begin
	render_pass_begin_info.renderPass = RenderPass;
	// Start point of render pass in pixel
	render_pass_begin_info.renderArea.offset = vk::Offset2D { 0, 0 };
	// Size of region to run render pass on
	render_pass_begin_info.renderArea.extent = SwapchainExtent;

	std::array<vk::ClearValue, 2> clear_values {};
	std::array<float, 4> colors { 0.6f, 0.65f, 0.4f, 1.0f };
	clear_values[0].color = vk::ClearColorValue { colors };
	clear_values[1].depthStencil.depth = 1.0f;

	render_pass_begin_info.clearValueCount = (uint32_t)clear_values.size();
	render_pass_begin_info.pClearValues = clear_values.data();

	// Because 1-to-1 relationship
	render_pass_begin_info.framebuffer = SwapchainFrameBuffers[image_idx];

	auto& buffer = CommandBuffers[image_idx];
	// Start recording commands to command buffer
	buffer.begin( buffer_begin_info );
	// Begin render pass
	// All draw commands inline (no secondary command buffers)
	buffer.beginRenderPass( render_pass_begin_info, vk::SubpassContents::eInline );
	// Bind pipeline to be used in render pass,
	// you could switch pipelines for different subpasses
	buffer.bindPipeline( vk::PipelineBindPoint::eGraphics, GraphicsPipeline );
	
	//  draw meshes
	for ( size_t mesh_id = 0; mesh_id < Meshes.size(); mesh_id++ )
	{
		const auto& mesh = Meshes[mesh_id];

		//  bind vertex buffer
		vk::Buffer vertex_buffers[] = { mesh.get_vertex_buffer() };
		vk::DeviceSize offsets[] = { 0 };
		buffer.bindVertexBuffers( 0, vertex_buffers, offsets );

		//  bind index buffer
		buffer.bindIndexBuffer( mesh.get_index_buffer(), 0, vk::IndexType::eUint32 );
			
		//  dynamic offset amount
		//uint32_t dynamic_offset = (uint32_t)ModelUniformAlignement * mesh_id;

		////  bind descriptor sets
		//buffer.bindDescriptorSets(
		//	vk::PipelineBindPoint::eGraphics,
		//	PipelineLayout,
		//	0,
		//	1,
		//	&DescriptorSets[image_idx],
		//	1,
		//	&dynamic_offset
		//);

		//  push constants
		MeshData model = mesh.get_mesh_data();
		buffer.pushConstants( 
			PipelineLayout, 
			vk::ShaderStageFlagBits::eVertex, 
			0, 
			sizeof( MeshData ), 
			&model 
		);

		//  bind descriptor sets
		buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			PipelineLayout,
			0,
			1,
			&DescriptorSets[image_idx],
			0,
			nullptr
		);

		// Execute pipeline
		buffer.drawIndexed( (uint32_t)mesh.get_index_count(), 1, 0, 0, 0 );

		printf( "mesh_id: %d\n", mesh_id );
	}

	// Draw 3 vertices, 1 instance, with no offset. Instance allow you
	// to draw several instances with one draw call.
	//buffer.draw( 3, 1, 0, 0 );

	// End render pass
	buffer.endRenderPass();
	// Stop recordind to command buffer
	buffer.end();
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

bool VulkanRenderer::check_validation_layer_support()
{
	std::vector<vk::LayerProperties> available_layers = vk::enumerateInstanceLayerProperties();

	for ( const char* layer_name : VulkanValidationLayers )
	{
		bool is_layer_found = false;

		for ( const auto& properties : available_layers )
		{
			if ( strcmp( layer_name, properties.layerName ) == 0 )
			{
				is_layer_found = true;
				break;
			}
		}

		if ( !is_layer_found ) return false;
	}

	return true;
}

void VulkanRenderer::retrieve_physical_device()
{
	std::vector<vk::PhysicalDevice> devices = Instance.enumeratePhysicalDevices();
	if ( devices.size() == 0 ) throw std::runtime_error( "Can't find any GPU that supports Vulkan" );

	for ( const auto& device : devices )
	{
		if ( check_device_suitable( device ) )
		{
			MainDevices.Physical = device;

			//  store properties
			vk::PhysicalDeviceProperties properties = device.getProperties();
			MinUniformBufferOffset = properties.limits.minUniformBufferOffsetAlignment;

			break;
		}
	}
}

bool VulkanRenderer::check_device_suitable( const vk::PhysicalDevice& device )
{
	vk::PhysicalDeviceProperties properties = device.getProperties();
	vk::PhysicalDeviceFeatures features = device.getFeatures();

	if ( !check_device_extension_support( device ) ) return false;

	VulkanSwapchainDetails details = get_swapchain_details( device );
	if ( !details.is_valid() ) return false;

	VulkanQueueFamilyIndices indices = get_queue_families( device );
	return indices.is_valid();
}

bool VulkanRenderer::check_device_extension_support( const vk::PhysicalDevice& device )
{
	std::vector<vk::ExtensionProperties> properties = device.enumerateDeviceExtensionProperties();

	for ( const auto& extension : VulkanDeviceExtensions )
	{
		bool has_extension = false;
		for ( const auto& prop : properties )
		{
			if ( strcmp( extension, prop.extensionName ) == 0 )
			{
				has_extension = true;
				break;
			}
		}

		if ( !has_extension ) return false;
	}

	return true;
}

void VulkanRenderer::update_uniform_buffers( uint32_t image_idx )
{
	//  copy view proj data
	void* data;
	MainDevices.Logical.mapMemory( 
		ViewProjUniformBuffersMemory[image_idx], 
		{}, 
		sizeof( ViewProjection ), 
		{}, 
		&data 
	);
	memcpy( data, &Matrices, sizeof( ViewProjection ) );
	MainDevices.Logical.unmapMemory( ViewProjUniformBuffersMemory[image_idx] );

	//  copy model data
	/*for ( size_t i = 0; i < Meshes.size(); i++ )
	{
		MeshData* model = (MeshData*)( (uint64_t)ModelTransferSpace + i * ModelUniformAlignement );
		*model = Meshes[i].get_mesh_data();
	}
	MainDevices.Logical.mapMemory(
		ModelUniformDynBuffersMemory[image_idx],
		{},
		ModelUniformAlignement * Meshes.size(),
		{},
		&data
	);
	memcpy( data, &ModelTransferSpace, ModelUniformAlignement * Meshes.size() );
	MainDevices.Logical.unmapMemory( ModelUniformDynBuffersMemory[image_idx] );*/
}

void VulkanRenderer::allocate_dynamic_buffer_transfer_space()
{
	// modelUniformAlignement = sizeof(Model) & ~(minUniformBufferOffet - 1);

	// We take the size of Model and we compare its size to a mask.
	// ~(minUniformBufferOffet - 1) is the inverse of minUniformBufferOffet
	// Example with a 16bits alignment coded on 8 bits:
	//   00010000 - 1  == 00001111
	// ~(00010000 - 1) == 11110000 which is our mask.
	// If we imagine our UboModel is 64 bits (01000000)
	// and the minUniformBufferOffet 16 bits (00010000),
	// (01000000) & ~(00010000 - 1) == 01000000 & 11110000 == 01000000
	// Our alignment will need to be 64 bits.

	// However this calculation is not perfect.

	// Let's now imagine our UboModel is 66 bits : 01000010.
	// The above calculation would give us a 64 bits alignment,
	// whereas we would need a 80 bits (01010000 = 64 + 16) alignment.

	// We need to add to the size minUniformBufferOffet - 1 to shield against this effect
	ModelUniformAlignement =
		( sizeof( MeshData ) + MinUniformBufferOffset - 1 ) 
		& ~( MinUniformBufferOffset - 1 );

	// We will now allocate memory for models.
	ModelTransferSpace = (MeshData*)_aligned_malloc(
		ModelUniformAlignement * MAX_OBJECTS, 
		ModelUniformAlignement
	);
}

VulkanSwapchainDetails VulkanRenderer::get_swapchain_details( const vk::PhysicalDevice& device )
{
	VulkanSwapchainDetails details;
	details.SurfaceCapabilities = device.getSurfaceCapabilitiesKHR( Surface );
	details.Formats = device.getSurfaceFormatsKHR( Surface );
	details.PresentationModes = device.getSurfacePresentModesKHR( Surface );

	return details;
}

vk::SurfaceFormatKHR VulkanRenderer::get_best_surface_format( const std::vector<vk::SurfaceFormatKHR>& formats )
{
	if ( formats.size() == 1 && formats[0].format == vk::Format::eUndefined )
	{
		return {
			vk::Format::eR8G8B8A8Unorm,
			vk::ColorSpaceKHR::eSrgbNonlinear
		};
	}

	for ( const auto& format : formats )
	{
		if ( format.format == vk::Format::eR8G8B8A8Unorm
			&& format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear )
		{
			return format;
		}
	}

	return formats[0];
}

vk::PresentModeKHR VulkanRenderer::get_best_presentation_mode( const std::vector<vk::PresentModeKHR>& modes )
{
	for ( const auto& mode : modes )
	{
		if ( mode == vk::PresentModeKHR::eMailbox )
		{
			return mode;
		}
	}

	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D VulkanRenderer::get_swap_extent( const vk::SurfaceCapabilitiesKHR& capabilities )
{
	if ( capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() ) return capabilities.currentExtent;

	//  retrieve window size
	int width, height;
	glfwGetFramebufferSize( Window, &width, &height );

	//  constrain extent size
	vk::Extent2D extent {};
	extent.width = math::clamp( (uint32_t)width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width );
	extent.height = math::clamp( (uint32_t)height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height );

	return extent;
}

vk::Format VulkanRenderer::select_supported_format( const std::vector<vk::Format>& formats, vk::ImageTiling tiling, vk::FormatFeatureFlags feature_flags )
{
	// Loop through the options and find a compatible format
	for ( vk::Format format : formats )
	{
		// Get properties for a given format on this device
		vk::FormatProperties properties =
			MainDevices.Physical.getFormatProperties( format );

		// If the tiling is linear and all feature flags match
		if ( tiling == vk::ImageTiling::eLinear
			&& ( properties.linearTilingFeatures & feature_flags ) == feature_flags )
		{
			return format;
		}
		// If the tiling is optimal and all feature flags match
		else if ( tiling == vk::ImageTiling::eOptimal &&
			( properties.optimalTilingFeatures & feature_flags ) == feature_flags )
		{
			return format;
		}
	}

	throw std::runtime_error( "Failed to find a matching format." );
}

VulkanQueueFamilyIndices VulkanRenderer::get_queue_families( const vk::PhysicalDevice& device )
{
	VulkanQueueFamilyIndices indices;
	std::vector<vk::QueueFamilyProperties> queue_families = device.getQueueFamilyProperties();

	for ( int i = 0; i < queue_families.size(); i++ )
	{
		const auto& queue_family = queue_families[i];
		if ( queue_family.queueCount == 0 ) continue;

		//  check graphics queue
		if ( queue_family.queueFlags & vk::QueueFlagBits::eGraphics )
		{
			indices.GraphicsFamily = i;
		}

		//  check presentation queue
		VkBool32 has_presentation_support = device.getSurfaceSupportKHR(
			(uint32_t)indices.GraphicsFamily,
			Surface
		);
		if ( has_presentation_support )
		{
			indices.PresentationFamily = i;
		}

		if ( indices.is_valid() ) break;
	}

	return indices;
}