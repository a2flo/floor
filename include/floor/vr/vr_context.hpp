/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#pragma once

#include <floor/core/essentials.hpp>
#include <floor/device/device_context.hpp>
#include <floor/math/matrix4.hpp>
#include <floor/math/quaternion.hpp>
#include <floor/core/event_objects.hpp>

//! forward decls so that we don't need to include Vulkan headers
struct VkPhysicalDevice_T;

namespace fl {

//! used to differentiate between the different VR platforms/backends
enum class VR_PLATFORM : uint32_t {
	NONE,
	OPENVR,
	OPENXR,
};

//! returns the string representation of the enum VR_PLATFORM
floor_inline_always static constexpr const char* vr_platform_to_string(const VR_PLATFORM& platform) {
	switch (platform) {
		case VR_PLATFORM::OPENVR: return "OpenVR";
		case VR_PLATFORM::OPENXR: return "OpenXR";
		default: return "NONE";
	}
}

//! VR eye enum used to specify eye-dependent things
enum class VR_EYE : uint32_t {
	LEFT,
	RIGHT
};

class vr_context {
public:
	//! constructs a VR context
	explicit vr_context();
	virtual ~vr_context();
	
	//! returns true if this VR context is valid / can be used
	bool is_valid() const {
		return valid;
	}
	
	//! returns the VR context platform type
	const VR_PLATFORM& get_platform_type() const {
		return platform;
	}
	
	//! returns the HMD name
	//! NOTE: empty if unknown
	const std::string& get_hmd_name() const {
		return hmd_name;
	}
	
	//! returns the HMD vendor name
	//! NOTE: empty if unknown
	const std::string& get_vendor_name() const {
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
	virtual std::string get_vulkan_instance_extensions() const {
		return {};
	}
	
	//! returns the required Vulkan device extensions needed for VR
	virtual std::string get_vulkan_device_extensions(VkPhysicalDevice_T* /* physical_device */) const {
		return {};
	}
	
	//! input update/event handling
	//! NOTE: this is called automatically by the event handler
	virtual std::vector<std::shared_ptr<event_object>> handle_input() {
		return {};
	}
	
	//! returns true if the VR platform provides its own swapchain
	virtual bool has_swapchain() const {
		return false;
	}
	
	struct swapchain_info_t {
		//! number of images in the swapchain
		uint32_t image_count { 0 };
		//! image type/format
		IMAGE_TYPE image_type { IMAGE_TYPE::NONE };
	};
	
	//! if has_swapchain() is true, this returns info about the swapchain
	virtual swapchain_info_t get_swapchain_info() const {
		return {};
	}
	
	//! if has_swapchain() is true, this returns the next swapchain image that can be rendered to
	//! NOTE: the returned image must provided to present() *as the next image*
	virtual device_image* acquire_next_image() {
		return nullptr;
	}
	
	//! returns true if the VR platform generates Vulkan validation errors -> will ignore these in certain places
	//! NOTE: since the situation is generally bad, this defaults to true and is only set to false for known good backends
	virtual bool ignore_vulkan_validation() const {
		return true;
	}
	
	//! presents the images of both eyes to the HMD/compositor
	//! NOTE: image must be a 2D array with 2 layers (first is the left eye, second is the right eye)
	virtual bool present(const device_queue& /* cqueue */, const device_image* /* image */) {
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
	
	//! known pose types
	enum class POSE_TYPE {
		//! invalid/unknown/none state
		UNKNOWN,
		//! head or HMD
		HEAD,
		//! left hand or controller
		HAND_LEFT,
		//! right hand or controller
		HAND_RIGHT,
		//! aim/target of the left hand/controller
		HAND_LEFT_AIM,
		//! aim/target of the right hand/controller
		HAND_RIGHT_AIM,
		//! reference point
		REFERENCE,
		//! special/internal type that generally doesn't need to be handled
		SPECIAL,
		
		//! generic + specific trackers
		TRACKER,
		TRACKER_HANDHELD_OBJECT,
		TRACKER_FOOT_LEFT,
		TRACKER_FOOT_RIGHT,
		TRACKER_SHOULDER_LEFT,
		TRACKER_SHOULDER_RIGHT,
		TRACKER_ELBOW_LEFT,
		TRACKER_ELBOW_RIGHT,
		TRACKER_KNEE_LEFT,
		TRACKER_KNEE_RIGHT,
		TRACKER_WAIST,
		TRACKER_CHEST,
		TRACKER_CAMERA,
		TRACKER_KEYBOARD,
		TRACKER_WRIST_LEFT,
		TRACKER_WRIST_RIGHT,
		TRACKER_ANKLE_LEFT,
		TRACKER_ANKLE_RIGHT,
		
		//! hand + forearm joints for each hand/arm
		//! NOTE: these match the OpenXR order
		HAND_JOINT_PALM_LEFT,
		HAND_JOINT_WRIST_LEFT,
		HAND_JOINT_THUMB_METACARPAL_LEFT,
		HAND_JOINT_THUMB_PROXIMAL_LEFT,
		HAND_JOINT_THUMB_DISTAL_LEFT,
		HAND_JOINT_THUMB_TIP_LEFT,
		HAND_JOINT_INDEX_METACARPAL_LEFT,
		HAND_JOINT_INDEX_PROXIMAL_LEFT,
		HAND_JOINT_INDEX_INTERMEDIATE_LEFT,
		HAND_JOINT_INDEX_DISTAL_LEFT,
		HAND_JOINT_INDEX_TIP_LEFT,
		HAND_JOINT_MIDDLE_METACARPAL_LEFT,
		HAND_JOINT_MIDDLE_PROXIMAL_LEFT,
		HAND_JOINT_MIDDLE_INTERMEDIATE_LEFT,
		HAND_JOINT_MIDDLE_DISTAL_LEFT,
		HAND_JOINT_MIDDLE_TIP_LEFT,
		HAND_JOINT_RING_METACARPAL_LEFT,
		HAND_JOINT_RING_PROXIMAL_LEFT,
		HAND_JOINT_RING_INTERMEDIATE_LEFT,
		HAND_JOINT_RING_DISTAL_LEFT,
		HAND_JOINT_RING_TIP_LEFT,
		HAND_JOINT_LITTLE_METACARPAL_LEFT,
		HAND_JOINT_LITTLE_PROXIMAL_LEFT,
		HAND_JOINT_LITTLE_INTERMEDIATE_LEFT,
		HAND_JOINT_LITTLE_DISTAL_LEFT,
		HAND_JOINT_LITTLE_TIP_LEFT,
		HAND_FOREARM_JOINT_ELBOW_LEFT,
		
		HAND_JOINT_PALM_RIGHT,
		HAND_JOINT_WRIST_RIGHT,
		HAND_JOINT_THUMB_METACARPAL_RIGHT,
		HAND_JOINT_THUMB_PROXIMAL_RIGHT,
		HAND_JOINT_THUMB_DISTAL_RIGHT,
		HAND_JOINT_THUMB_TIP_RIGHT,
		HAND_JOINT_INDEX_METACARPAL_RIGHT,
		HAND_JOINT_INDEX_PROXIMAL_RIGHT,
		HAND_JOINT_INDEX_INTERMEDIATE_RIGHT,
		HAND_JOINT_INDEX_DISTAL_RIGHT,
		HAND_JOINT_INDEX_TIP_RIGHT,
		HAND_JOINT_MIDDLE_METACARPAL_RIGHT,
		HAND_JOINT_MIDDLE_PROXIMAL_RIGHT,
		HAND_JOINT_MIDDLE_INTERMEDIATE_RIGHT,
		HAND_JOINT_MIDDLE_DISTAL_RIGHT,
		HAND_JOINT_MIDDLE_TIP_RIGHT,
		HAND_JOINT_RING_METACARPAL_RIGHT,
		HAND_JOINT_RING_PROXIMAL_RIGHT,
		HAND_JOINT_RING_INTERMEDIATE_RIGHT,
		HAND_JOINT_RING_DISTAL_RIGHT,
		HAND_JOINT_RING_TIP_RIGHT,
		HAND_JOINT_LITTLE_METACARPAL_RIGHT,
		HAND_JOINT_LITTLE_PROXIMAL_RIGHT,
		HAND_JOINT_LITTLE_INTERMEDIATE_RIGHT,
		HAND_JOINT_LITTLE_DISTAL_RIGHT,
		HAND_JOINT_LITTLE_TIP_RIGHT,
		HAND_FOREARM_JOINT_ELBOW_RIGHT,
	};
	
	//! returns a human-readable string of the specified pose type
	static std::string_view pose_type_to_string(const POSE_TYPE type);
	
	//! tracked pose (tracker device, hand tracking, ...)
	struct pose_t {
		POSE_TYPE type { POSE_TYPE::UNKNOWN };
		
		float3 position;
		float radius { 0.0f };
		quaternionf orientation;
		float3 linear_velocity;
		float3 angular_velocity;
		
		//! validity flags
		struct flags_t {
			union {
				struct {
					uint32_t is_active : 1;
					
					uint32_t position_valid : 1;
					uint32_t orientation_valid : 1;
					uint32_t linear_velocity_valid : 1;
					uint32_t angular_velocity_valid : 1;
					
					uint32_t position_tracked : 1;
					uint32_t orientation_tracked : 1;
					uint32_t linear_velocity_tracked : 1;
					uint32_t angular_velocity_tracked : 1;
					
					uint32_t radius_valid : 1;
					
					uint32_t unused : 22;
				};
				uint32_t all { 0u };
			};
			
			constexpr std::strong_ordering operator<=>(const flags_t& rhs) const noexcept {
				if (all < rhs.all) {
					return std::strong_ordering::less;
				} else if (all > rhs.all) {
					return std::strong_ordering::greater;
				}
				return std::strong_ordering::equal;
			}
			
			constexpr bool operator==(const flags_t& rhs) const noexcept {
				return (all == rhs.all);
			}
			
			constexpr bool operator!=(const flags_t& rhs) const noexcept {
				return (all != rhs.all);
			}
		} flags {};
		
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(float-equal)
		constexpr auto operator<=>(const pose_t&) const noexcept = default;
FLOOR_POP_WARNINGS()
	};
	
	//! retrieve the current pose state
	virtual std::vector<pose_t> get_pose_state() const = 0;
	
	//! supported/known controller types (including OpenXR extensions)
	enum class CONTROLLER_TYPE {
		NONE,
		KHRONOS_SIMPLE,
		INDEX,
		HTC_VIVE,
		GOOGLE_DAYDREAM,
		MICROSOFT_MIXED_REALITY,
		OCULUS_GO,
		OCULUS_TOUCH,
		HP_MIXED_REALITY,
		HTC_VIVE_COSMOS,
		HTC_VIVE_FOCUS3,
		HUAWEI,
		SAMSUNG_ODYSSEY,
		MAGIC_LEAP2,
		OCULUS_TOUCH_PRO,
		PICO_NEO3,
		PICO4,
		__MAX_CONTROLLER_TYPE
	};
	
	//! returns a human-readable string of the specified controller type
	static std::string_view controller_type_to_string(const CONTROLLER_TYPE type);
	
protected:
	bool valid { false };
	
	// HMD/system properties
	VR_PLATFORM platform { VR_PLATFORM::NONE };
	std::string hmd_name;
	std::string vendor_name;
	float display_frequency { -1.0f };
	uint2 recommended_render_size;
	
};

} // namespace fl
