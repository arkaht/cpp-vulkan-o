#pragma once

struct VulkanQueueFamilyIndices
{
	int graphics_family_location = -1;

	bool is_valid() { return graphics_family_location >= 0; }
};