/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#if !defined(FLOOR_NO_OPENVR)
#include <floor/vr/vr_context.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <bitset>

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

class openvr_context : public vr_context {
public:
	openvr_context();
	~openvr_context() override;
	
	string get_vulkan_instance_extensions() const override;
	string get_vulkan_device_extensions(VkPhysicalDevice_T* physical_device) const override;
	
	vector<shared_ptr<event_object>> handle_input() override;
	
	bool present(const compute_queue& cqueue, const compute_image* image) override;
	
	frame_view_state_t get_frame_view_state(const float& z_near, const float& z_far,
											const bool with_position_in_mvm) const override;

	vector<pose_t> get_pose_state() const override;

	//! OpenVR only supports a fixed amount of devices (trackers, controllers, ...)
	static constexpr const uint32_t max_tracked_devices { 64u };
	//! SteamVR/OpenVR should always report 31 bones per hand
	static constexpr const uint32_t expected_bone_count { 31u };
	//! we will only handle the first 26 bones that match the OpenXR bones (-> ignore aux bones)
	static constexpr const uint32_t handled_bone_count { 26u };
	
protected:
	vr::IVRSystem* system { nullptr };
	unique_ptr<vr::COpenVRContext> ctx;
	vr::IVRCompositor* compositor { nullptr };
	vr::IVRInput* input { nullptr };

	//! access to "pose_state" must be thread-safe
	mutable atomic_spin_lock pose_state_lock;
	//! current pose state
	vector<pose_t> pose_state GUARDED_BY(pose_state_lock);
	//! size(pose_state) of the last update (helps with allocation)
	size_t prev_pose_state_size { 0u };

	//! converts the specified device index to a POSE_TYPE
	POSE_TYPE device_index_to_pose_type(const uint32_t idx);
	//! access to "device_type_map" must be thread-safe
	mutable atomic_spin_lock device_type_map_lock;
	//! device index -> POSE_TYPE mapping
	array<POSE_TYPE, max_tracked_devices> device_type_map GUARDED_BY(device_type_map_lock) {};
	//! can use an optimized bit set for device activity tracking
	bitset<max_tracked_devices> device_active { 0u };

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

	//! tracked device index for each hand (0 signals no controller is connected)
	array<uint32_t, 2> hand_device_indices { 0u, 0u };
	//! currently active controller type for each hand
	array<CONTROLLER_TYPE, 2> hand_controller_types {
		CONTROLLER_TYPE::NONE,
		CONTROLLER_TYPE::NONE
	};
	//! set when update_hand_controller_types() should be called next time input is handled
	atomic<bool> force_update_controller_types { false };
	//! called on setup and controller connect/disconnect/update
	void update_hand_controller_types();

	// hand-tracking
	//! this is supported by default, but will be disabled if the config does so or if there is an error
	bool has_hand_tracking_support { true };

	//! computes the current projection matrix for the specified eye and near/far plane
	matrix4f get_projection_matrix(const VR_EYE& eye, const float& z_near, const float& z_far) const;

	//! returns the eye to head matrix for the specified eye
	matrix4f get_eye_matrix(const VR_EYE& eye) const;

	//! returns the current HMD view matrix
	const matrix4f& get_hmd_matrix() const;

	//! returns the raw FOV { -left, right, top, -bottom } tangents of half-angles in radian
	float4 get_projection_raw(const VR_EYE& eye) const;
	
};

#endif
