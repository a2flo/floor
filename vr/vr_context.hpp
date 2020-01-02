/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#if !defined(FLOOR_NO_VR)
#include <floor/compute/compute_context.hpp>
#include <floor/math/matrix4.hpp>
#include <floor/core/event_objects.hpp>

// forward decls so that we don't need to include OpenVR or Vulkan headers
struct VkPhysicalDevice_T;
namespace vr {
	class IVRSystem;
	class IVRCompositor;
	class IVRInput;
	class COpenVRContext;
	struct TrackedDevicePose_t;
	typedef uint64_t VRActionSetHandle_t;
	typedef uint64_t VRActionHandle_t;
}

//! VR eye enum used to specify eye-dependent things
enum class VR_EYE : uint32_t {
	LEFT,
	RIGHT
};

//! current status of a tracked VR device
//! NOTE: corresponds to vr::ETrackingResult
enum class VR_TRACKING_STATUS : int {
	UNINITIALIZED = 1,
	CALIBRATING_IN_PROGRESS = 100,
	CALIBRATING_OUF_OF_RANGE = 101,
	RUNNING_OK = 200,
	RUNNING_OUT_OF_RANGE = 201,
	FALLBACK_ROTATION_ONLY = 300,
};

class vr_context {
public:
	//! constructs a VR context
	explicit vr_context();
	~vr_context();

	//! returns true if this VR context is valid / can be used
	bool is_valid() const {
		return valid;
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
	unique_ptr<char[]> get_vulkan_instance_extensions() const;

	//! returns the required Vulkan device extensions needed for VR
	unique_ptr<char[]> get_vulkan_device_extensions(VkPhysicalDevice_T* physical_device) const;

	//! updates the state of all currently tracked devices
	//! NOTE: must be called once each frame (done automatically through the graphics context)
	bool update();

	//! input update handling
	//! NOTE: this is called automatically by the event handler
	vector<shared_ptr<event_object>> update_input();

	//! presents the images of both eyes to the HMD/compositor
	//! NOTE: image must be a 2D array with 2 layers (first is the left eye, second is the right eye)
	bool present(const compute_queue& cqueue, const compute_image& image);
	
	//! the per-eye modelview and projection matrices for a particular frame
	//! NOTE: this contains all necessary per-eye transformations
	struct frame_matrices_t {
		//! left eye modelview matrix
		matrix4f mvm_left;
		//! right eye modelview matrix
		matrix4f mvm_right;
		//! left eye projection matrix
		matrix4f pm_left;
		//! right eye projection matrix
		matrix4f pm_right;
	};
	
	//! returns/computes the modelview and projection matrices for this frame
	//! NOTE: if "with_position" is true, then the modelview will also contain the current position
	frame_matrices_t get_frame_matrices(const float& z_near, const float& z_far,
										const bool with_position = true) const;

	//! computes the current projection matrix for the specified eye and near/far plane
	matrix4f get_projection_matrix(const VR_EYE& eye, const float& z_near, const float& z_far) const;

	//! returns the raw FOV { -left, right, top, -bottom } tangents of half-angles in radian
	float4 get_projection_raw(const VR_EYE& eye) const;

	//! returns the eye to head matrix for the specified eye
	matrix4f get_eye_matrix(const VR_EYE& eye) const;

	//! returns the current HMD view matrix
	const matrix4f& get_hmd_matrix() const;

	//! contains the current state of a tracked device
	//! NOTE: corresponds to vr::TrackedDevicePose_t
	struct tracked_device_pose_t {
		matrix4f device_to_absolute_tracking;
		float3 velocity;
		float3 angular_velocity;
		VR_TRACKING_STATUS status { VR_TRACKING_STATUS::UNINITIALIZED };
		bool device_is_connected { false };
	};

	//! returns the currently active/tracked device poses
	const vector<tracked_device_pose_t>& get_poses() const {
		return poses;
	}

protected:
	vr::IVRSystem* system { nullptr };
	bool valid { false };

	unique_ptr<vr::COpenVRContext> ctx;
	vr::IVRCompositor* compositor { nullptr };
	vr::IVRInput* input { nullptr };

	// HMD/system properties
	string hmd_name;
	string vendor_name;
	float display_frequency { -1.0f };
	uint2 recommended_render_size;

	//! stores the currently active tracked device state/poses
	vector<tracked_device_pose_t> poses;
	matrix4f hmd_mat;

	// input handling
	vr::VRActionSetHandle_t main_action_set;
	enum class ACTION_TYPE : uint32_t {
		//! on / off
		DIGITAL,
		//! 1D, 2D, 3D, ...
		ANALOG,
		//! pose
		POSE,
		//! skeletal
		SKELETAL,
		//! haptic
		HAPTIC,
	};
	struct action_t {
		ACTION_TYPE type;
		bool side; // false: left, true: right
		EVENT_TYPE event_type;
		vr::VRActionHandle_t handle;
	};
	unordered_map<string, action_t> actions;

};

#endif

#endif
