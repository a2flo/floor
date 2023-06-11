/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __FLOOR_VR_OPENXR_CONTEXT_HPP__
#define __FLOOR_VR_OPENXR_CONTEXT_HPP__

#include <floor/core/essentials.hpp>

// NOTE: this requires both VR and Vulkan support (no point in anything else)
#if !defined(FLOOR_NO_OPENXR) && !defined(FLOOR_NO_VULKAN)
#include <floor/vr/vr_context.hpp>
#include <floor/compute/vulkan/vulkan_common.hpp>
#include <floor/math/quaternion.hpp>
#include <floor/threading/atomic_spin_lock.hpp>

#define XR_USE_GRAPHICS_API_VULKAN 1
#if defined(__linux__)
#define XR_USE_TIMESPEC 1
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

class vulkan_compute;
class vulkan_device;
class vulkan_queue;

class openxr_context : public vr_context {
public:
	openxr_context();
	~openxr_context() override;

	//! Vulkan instance creation must be done via the OpenXR wrapper
	VkResult create_vulkan_instance(const VkInstanceCreateInfo& vk_create_info, VkInstance& vk_instance);

	//! Vulkan device creation must be done via the OpenXR wrapper
	VkResult create_vulkan_device(const VkDeviceCreateInfo& vk_create_info, VkDevice& vk_dev,
								  const VkPhysicalDevice& vk_phys_dev, VkInstance& vk_instance);

	//! creates the OpenXR session, using the specified Vulkan context
	bool create_session(vulkan_compute& vk_ctx, const vulkan_device& vk_dev, const vulkan_queue& vk_queue);
	
	string get_vulkan_instance_extensions() const override;
	string get_vulkan_device_extensions(VkPhysicalDevice_T* physical_device) const override;
	
	bool update() override;
	vector<shared_ptr<event_object>> update_input() override;
	
	bool present(const compute_queue& cqueue, const compute_image& image) override;
	
	frame_view_state_t get_frame_view_state(const float& z_near, const float& z_far,
											const bool with_position_in_mvm) const override;
	
protected:
	XrInstance instance { nullptr };
	XrSystemId system_id {};
	XrSession session { nullptr };

	vulkan_compute* vk_ctx { nullptr };
	const vulkan_device* vk_dev { nullptr };

	uint32_t swapchain_layer_count { 0u };
	vector<XrReferenceSpaceType> spaces;
	XrSpace scene_space { nullptr };
	XrSpace view_space { nullptr };
	bool mutable_fov { false };
	vector<XrViewConfigurationView> view_configs;
	vector<XrView> views;

	struct multi_layer_swapchaint_t {
		XrSwapchain swapchain{ nullptr };
		vector<unique_ptr<compute_image>> swapchain_images;
		vector<XrSwapchainImageVulkan2KHR> swapchain_vk_images;
		vector<VkImageView> swapchain_vk_image_views;
	};
	struct multi_image_swapchaint_t {
		vector<XrSwapchain> swapchains;
		vector<vector<unique_ptr<compute_image>>> swapchain_images;
		vector<vector<XrSwapchainImageVulkan2KHR>> swapchain_vk_images;
		vector<vector<VkImageView>> swapchain_vk_image_views;
	};
	variant<multi_layer_swapchaint_t, multi_image_swapchaint_t> swapchain;

	struct view_state_t {
		float3 position;
		quaternionf orientation;
		float4 fov;
	};
	struct hmd_view_state_t {
		float3 position;
		quaternionf orientation;
	};
	mutable atomic_spin_lock view_states_lock;
	vector<view_state_t> view_states GUARDED_BY(view_states_lock);
	hmd_view_state_t hmd_view_state GUARDED_BY(view_states_lock);

	PFN_xrCreateVulkanInstanceKHR CreateVulkanInstanceKHR { nullptr };
	PFN_xrCreateVulkanDeviceKHR CreateVulkanDeviceKHR { nullptr };
	PFN_xrGetVulkanGraphicsDevice2KHR GetVulkanGraphicsDevice2KHR { nullptr };
	PFN_xrGetVulkanGraphicsRequirements2KHR GetVulkanGraphicsRequirements2KHR { nullptr };
	
};

#endif

#endif
