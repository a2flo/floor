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

#ifndef __FLOOR_VR_VR_CONTEXT_HPP__
#define __FLOOR_VR_VR_CONTEXT_HPP__

#include <floor/core/essentials.hpp>
#include <floor/compute/compute_context.hpp>
#include <floor/math/matrix4.hpp>
#include <floor/core/event_objects.hpp>

//! used to differentiate between the different VR implementations/backends
enum class VR_BACKEND : uint32_t {
	NONE,
	OPENVR,
	OPENXR,
};

//! returns the string representation of the enum VR_BACKEND
floor_inline_always static constexpr const char* vr_backend_to_string(const VR_BACKEND& backend) {
	switch (backend) {
		case VR_BACKEND::OPENVR: return "OpenVR";
		case VR_BACKEND::OPENXR: return "OpenXR";
		default: return "NONE";
	}
}

//! VR eye enum used to specify eye-dependent things
enum class VR_EYE : uint32_t {
	LEFT,
	RIGHT
};

//! forward decls so that we don't need to include Vulkan headers
struct VkPhysicalDevice_T;

class vr_context {
public:
	//! constructs a VR context
	explicit vr_context();
	virtual ~vr_context();

	//! returns true if this VR context is valid / can be used
	bool is_valid() const {
		return valid;
	}
	
	//! returns the VR context backend type
	const VR_BACKEND& get_backend() const {
		return backend;
	}

	//! returns the HMD name
	//! NOTE: empty if unknown
	const string& get_hmd_name() const {
		return hmd_name;
	}

	//! returns the HMD vendor name
	//! NOTE: empty if unknown
	const string& get_vendor_name() const {
		return vendor_name;
	}

	//! returns the HMD display frequency in Hz
	//! NOTE: -1.0 if unknown
	const float& get_display_frequency() const {
		return display_frequency;
	}

	//! returns the HMD recommended render size
	//! NOTE: (0, 0) if unknown
	const uint2& get_recommended_render_size() const {
		return recommended_render_size;
	}

	//! returns the required Vulkan instance extensions needed for VR
	virtual string get_vulkan_instance_extensions() const {
		return {};
	}

	//! returns the required Vulkan device extensions needed for VR
	virtual string get_vulkan_device_extensions(VkPhysicalDevice_T* /* physical_device */) const {
		return {};
	}

	//! input update/event handling
	//! NOTE: this is called automatically by the event handler
	virtual vector<shared_ptr<event_object>> handle_input() {
		return {};
	}

	//! returns true if the VR backend provides its own swapchain
	virtual bool has_swapchain() const {
		return false;
	}

	struct swapchain_info_t {
		//! number of images in the swapchain
		uint32_t image_count { 0 };
		//! image type/format
		COMPUTE_IMAGE_TYPE image_type { COMPUTE_IMAGE_TYPE::NONE };
	};

	//! if has_swapchain() is true, this returns info about the swapchain
	virtual swapchain_info_t get_swapchain_info() const {
		return {};
	}

	//! if has_swapchain() is true, this returns the next swapchain image that can be rendered to
	//! NOTE: the returned image must provided to present() *as the next image*
	virtual compute_image* acquire_next_image() {
		return nullptr;
	}

	//! returns true if the VR backend generates Vulkan validation errors -> will ignore these in certain places
	//! NOTE: since the situation is generally bad, this defaults to true and is only set to false for known good backends
	virtual bool ignore_vulkan_validation() const {
		return true;
	}

	//! presents the images of both eyes to the HMD/compositor
	//! NOTE: image must be a 2D array with 2 layers (first is the left eye, second is the right eye)
	virtual bool present(const compute_queue& /* cqueue */, const compute_image* /* image */) {
		return false;
	}
	
	//! the per-eye modelview and projection matrices for a particular frame
	//! NOTE: this contains all necessary per-eye transformations
	struct frame_view_state_t {
		//! global HMD position
		float3 hmd_position;
		//! eye distance / IPD
		float eye_distance { 0.0f };
		//! left eye modelview matrix
		matrix4f mvm_left;
		//! right eye modelview matrix
		matrix4f mvm_right;
		//! left eye projection matrix
		matrix4f pm_left;
		//! right eye projection matrix
		matrix4f pm_right;
	};
	
	//! returns/computes the modelview and projection matrices for this frame,
	//! as well as the global HMD position and current eye distance (IPD)
	//! NOTE: if "with_position_in_mvm" is true, then the MVMs will also contain the current position
	virtual frame_view_state_t get_frame_view_state(const float& z_near, const float& z_far,
													const bool with_position_in_mvm = true) const = 0;

protected:
	bool valid { false };

	// HMD/system properties
	VR_BACKEND backend { VR_BACKEND::NONE };
	string hmd_name;
	string vendor_name;
	float display_frequency { -1.0f };
	uint2 recommended_render_size;

};

#endif
