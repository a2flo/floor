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

#ifndef __FLOOR_VR_OPENVR_CONTEXT_HPP__
#define __FLOOR_VR_OPENVR_CONTEXT_HPP__

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VR)
#include <floor/vr/vr_context.hpp>

// forward decls so that we don't need to include OpenVR headers
namespace vr {
	class IVRSystem;
	class IVRCompositor;
	class IVRInput;
	class COpenVRContext;
	struct TrackedDevicePose_t;
	typedef uint64_t VRActionSetHandle_t;
	typedef uint64_t VRActionHandle_t;
}

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

class openvr_context : public vr_context {
public:
	openvr_context();
	~openvr_context() override;
	
	string get_vulkan_instance_extensions() const override;
	string get_vulkan_device_extensions(VkPhysicalDevice_T* physical_device) const override;
	
	bool update() override;
	vector<shared_ptr<event_object>> update_input() override;
	
	bool present(const compute_queue& cqueue, const compute_image& image) override;
	
	frame_matrices_t get_frame_matrices(const float& z_near, const float& z_far, const bool with_position = true) const override;
	matrix4f get_projection_matrix(const VR_EYE& eye, const float& z_near, const float& z_far) const override;
	float4 get_projection_raw(const VR_EYE& eye) const override;
	matrix4f get_eye_matrix(const VR_EYE& eye) const override;
	const matrix4f& get_hmd_matrix() const override;
	
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
	unique_ptr<vr::COpenVRContext> ctx;
	vr::IVRCompositor* compositor { nullptr };
	vr::IVRInput* input { nullptr };
	
	//! stores the currently active tracked device state/poses
	vector<tracked_device_pose_t> poses;
	matrix4f hmd_mat;
	
	// input handling
	vr::VRActionSetHandle_t main_action_set{};
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
