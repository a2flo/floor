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

#include <floor/vr/openvr_context.hpp>

#if !defined(FLOOR_NO_OPENVR)
#include <floor/floor/floor.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/compute/compute_queue.hpp>

#if !defined(MINGW)
#include <openvr.h>
#else
#include <openvr_mingw.hpp>
#endif

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#endif

#if !defined(FLOOR_NO_METAL)
#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/metal/metal_image.hpp>
#endif

static constexpr inline vr::EVREye eye_to_eye(const VR_EYE& eye) {
	return (eye == VR_EYE::LEFT ? vr::EVREye::Eye_Left : vr::EVREye::Eye_Right);
}

template <typename property_type>
static property_type get_vr_property(vr::IVRSystem& system,
									 const vr::ETrackedDeviceProperty& prop,
									 const vr::TrackedDeviceIndex_t idx = 0) {
	if constexpr (is_same_v<property_type, string>) {
		const auto len = system.GetStringTrackedDeviceProperty(idx, prop, nullptr, 0, nullptr);
		if (len == 0) {
			return {};
		}
		string str(len, '\0');
		system.GetStringTrackedDeviceProperty(idx, prop, str.data(), len, nullptr);
		return core::trim(str);
	} else if constexpr (is_same_v<property_type, bool>) {
		return system.GetBoolTrackedDeviceProperty(idx, prop, nullptr);
	} else if constexpr (is_same_v<property_type, float>) {
		return system.GetFloatTrackedDeviceProperty(idx, prop, nullptr);
	} else if constexpr (is_same_v<property_type, int32_t>) {
		return system.GetInt32TrackedDeviceProperty(idx, prop, nullptr);
	} else if constexpr (is_same_v<property_type, uint64_t>) {
		return system.GetUint64TrackedDeviceProperty(idx, prop, nullptr);
	} else {
		instantiation_trap_dependent_type(property_type, "not implemented yet or invalid type");
	}
}

openvr_context::openvr_context() : vr_context() {
	backend = VR_BACKEND::OPENVR;
	
	// check pre-conditions
	if (!vr::VR_IsHmdPresent()) {
		log_error("no HMD present");
		return;
	}
	if (!vr::VR_IsRuntimeInstalled()) {
		log_error("no VR runtime installed");
		return;
	}

	// init VR
	vr::EVRInitError err { vr::EVRInitError::VRInitError_None };
	system = vr::VR_Init(&err, vr::EVRApplicationType::VRApplication_Scene, nullptr);
	if (system == nullptr) {
		log_error("failed to initialize VR: $", VR_GetVRInitErrorAsEnglishDescription(err));
		return;
	}

	ctx = make_unique<vr::COpenVRContext>();
	compositor = ctx->VRCompositor();
	if (compositor == nullptr) {
		log_error("no VR compositor");
		return;
	}
	input = ctx->VRInput();
	if (input == nullptr) {
		log_error("no VR input");
		return;
	}

	hmd_name = get_vr_property<string>(*system, vr::Prop_ModelNumber_String);
	vendor_name = get_vr_property<string>(*system, vr::Prop_ManufacturerName_String);
	display_frequency = get_vr_property<float>(*system, vr::Prop_DisplayFrequency_Float);
	if (display_frequency <= 0.0f) {
		display_frequency = -1.0f;
	}
	system->GetRecommendedRenderTargetSize(&recommended_render_size.x, &recommended_render_size.y);
	log_debug("VR HMD: $ ($) -> $*$ @$Hz", hmd_name, vendor_name,
			  recommended_render_size.x, recommended_render_size.y, display_frequency);

	// input setup
	const auto vr_action_manifest_file = floor::data_path("vr_action_manifest.json");
	if (file_io::is_file(vr_action_manifest_file)) {
		const auto input_err = input->SetActionManifestPath(vr_action_manifest_file.c_str());
		if (input_err != vr::EVRInputError::VRInputError_None) {
			log_error("VR: failed to set action manifest: $", input_err);
			return;
		}

		const auto action_set_err = input->GetActionSetHandle("/actions/main", &main_action_set);
		if (action_set_err != vr::EVRInputError::VRInputError_None || main_action_set == vr::k_ulInvalidActionSetHandle) {
			log_error("VR: failed to get main action set: $", action_set_err);
			return;
		}

		actions = {
			{ "/actions/main/in/left_applicationmenu_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_APP_MENU_PRESS, 0 } },
			{ "/actions/main/in/left_applicationmenu_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_APP_MENU_TOUCH, 0 } },
			{ "/actions/main/in/left_main_button_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_MAIN_PRESS, 0 } },
			{ "/actions/main/in/left_main_button_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_MAIN_TOUCH, 0 } },
			{ "/actions/main/in/left_system_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_SYSTEM_PRESS, 0 } },
			{ "/actions/main/in/left_system_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_SYSTEM_TOUCH, 0 } },
			{ "/actions/main/in/left_trackpad_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_TRACKPAD_PRESS, 0 } },
			{ "/actions/main/in/left_trackpad_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_TRACKPAD_TOUCH, 0 } },
			{ "/actions/main/in/left_trackpad_value", { ACTION_TYPE::ANALOG, false, EVENT_TYPE::VR_TRACKPAD_MOVE, 0 } },
			{ "/actions/main/in/left_trackpad_force", { ACTION_TYPE::ANALOG, false, EVENT_TYPE::VR_TRACKPAD_FORCE, 0 } },
			{ "/actions/main/in/left_trigger_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_TRIGGER_PRESS, 0 } },
			{ "/actions/main/in/left_trigger_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_TRIGGER_TOUCH, 0 } },
			{ "/actions/main/in/left_trigger_pull", { ACTION_TYPE::ANALOG, false, EVENT_TYPE::VR_TRIGGER_PULL, 0 } },
			{ "/actions/main/in/left_thumbstick_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_THUMBSTICK_PRESS, 0 } },
			{ "/actions/main/in/left_thumbstick_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_THUMBSTICK_TOUCH, 0 } },
			{ "/actions/main/in/left_thumbstick_value", { ACTION_TYPE::ANALOG, false, EVENT_TYPE::VR_THUMBSTICK_MOVE, 0 } },
			{ "/actions/main/in/left_grip_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_GRIP_PRESS, 0 } },
			{ "/actions/main/in/left_grip_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_GRIP_TOUCH, 0 } },
			{ "/actions/main/in/left_grip_force", { ACTION_TYPE::ANALOG, false, EVENT_TYPE::VR_GRIP_FORCE, 0 } },
			{ "/actions/main/in/left_grip_pull", { ACTION_TYPE::ANALOG, false, EVENT_TYPE::VR_GRIP_PULL, 0 } },
			{ "/actions/main/in/left_pose", { ACTION_TYPE::POSE, false, EVENT_TYPE::__VR_CONTROLLER_EVENT, 0 } }, // TODO
			{ "/actions/main/out/left_haptic", { ACTION_TYPE::HAPTIC, false, EVENT_TYPE::__VR_CONTROLLER_EVENT, 0 } }, // TODO
			{ "/actions/main/in/right_applicationmenu_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_APP_MENU_PRESS, 0 } },
			{ "/actions/main/in/right_applicationmenu_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_APP_MENU_TOUCH, 0 } },
			{ "/actions/main/in/right_main_button_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_MAIN_PRESS, 0 } },
			{ "/actions/main/in/right_main_button_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_MAIN_TOUCH, 0 } },
			{ "/actions/main/in/right_system_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_SYSTEM_PRESS, 0 } },
			{ "/actions/main/in/right_system_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_SYSTEM_TOUCH, 0 } },
			{ "/actions/main/in/right_trackpad_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_TRACKPAD_PRESS, 0 } },
			{ "/actions/main/in/right_trackpad_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_TRACKPAD_TOUCH, 0 } },
			{ "/actions/main/in/right_trackpad_value", { ACTION_TYPE::ANALOG, true, EVENT_TYPE::VR_TRACKPAD_MOVE, 0 } },
			{ "/actions/main/in/right_trackpad_force", { ACTION_TYPE::ANALOG, true, EVENT_TYPE::VR_TRACKPAD_FORCE, 0 } },
			{ "/actions/main/in/right_trigger_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_TRIGGER_PRESS, 0 } },
			{ "/actions/main/in/right_trigger_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_TRIGGER_TOUCH, 0 } },
			{ "/actions/main/in/right_trigger_pull", { ACTION_TYPE::ANALOG, true, EVENT_TYPE::VR_TRIGGER_PULL, 0 } },
			{ "/actions/main/in/right_thumbstick_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_THUMBSTICK_PRESS, 0 } },
			{ "/actions/main/in/right_thumbstick_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_THUMBSTICK_TOUCH, 0 } },
			{ "/actions/main/in/right_thumbstick_value", { ACTION_TYPE::ANALOG, true, EVENT_TYPE::VR_THUMBSTICK_MOVE, 0 } },
			{ "/actions/main/in/right_grip_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_GRIP_PRESS, 0 } },
			{ "/actions/main/in/right_grip_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_GRIP_TOUCH, 0 } },
			{ "/actions/main/in/right_grip_force", { ACTION_TYPE::ANALOG, true, EVENT_TYPE::VR_GRIP_FORCE, 0 } },
			{ "/actions/main/in/right_grip_pull", { ACTION_TYPE::ANALOG, true, EVENT_TYPE::VR_GRIP_PULL, 0 } },
			{ "/actions/main/in/right_pose", { ACTION_TYPE::POSE, true, EVENT_TYPE::__VR_CONTROLLER_EVENT, 0 } }, // TODO
			{ "/actions/main/out/right_haptic", { ACTION_TYPE::HAPTIC, true, EVENT_TYPE::__VR_CONTROLLER_EVENT, 0 } }, // TODO
		};
		for (auto& action : actions) {
			const auto handle_err = input->GetActionHandle(action.first.c_str(), &action.second.handle);
			if (handle_err != vr::EVRInputError::VRInputError_None || action.second.handle == vr::k_ulInvalidActionHandle) {
				log_error("VR: failed to get action handle $: $", action.first, handle_err);
				return;
			}
		}
	}

	// initial setup / get initial poses / tracked devices
	openvr_context::update();

	// all done
	valid = true;
}

openvr_context::~openvr_context() {
	if (system == nullptr) {
		return;
	}
	vr::VR_Shutdown();
}

string openvr_context::get_vulkan_instance_extensions() const {
	if (compositor == nullptr) {
		return {};
	}

	const auto vk_instance_exts_len = compositor->GetVulkanInstanceExtensionsRequired(nullptr, 0);
	if (vk_instance_exts_len == 0) {
		return {};
	}

	string vk_instance_exts(vk_instance_exts_len, '\0');
	compositor->GetVulkanInstanceExtensionsRequired(vk_instance_exts.data(), vk_instance_exts_len);
	return vk_instance_exts;
}

string openvr_context::get_vulkan_device_extensions(VkPhysicalDevice_T* physical_device) const {
	if (compositor == nullptr) {
		return {};
	}

	const auto vk_dev_exts_len = compositor->GetVulkanDeviceExtensionsRequired(physical_device, nullptr, 0);
	if (vk_dev_exts_len == 0) {
		return {};
	}

	string vk_dev_exts(vk_dev_exts_len, '\0');
	compositor->GetVulkanDeviceExtensionsRequired(physical_device, vk_dev_exts.data(), vk_dev_exts_len);
	return vk_dev_exts;
}

bool openvr_context::update() {
	// poses / tracked device handling
	array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> vr_poses {};
	const auto err = compositor->WaitGetPoses(&vr_poses[0], vr::k_unMaxTrackedDeviceCount, nullptr, 0);
	if (err != vr::EVRCompositorError::VRCompositorError_None) {
		log_error("failed to update VR poses: $", err);
		return false;
	}

	poses.clear();
	for (uint32_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i) {
		const auto& vr_pose = vr_poses[i];
		if (!vr_pose.bPoseIsValid) {
			// valid poses are contiguous, no need to iterate any further
			break;
		}

		const auto& vr_pose_mat = vr_pose.mDeviceToAbsoluteTracking.m;
		tracked_device_pose_t pose {
			.device_to_absolute_tracking = {
				vr_pose_mat[0][0], vr_pose_mat[1][0], vr_pose_mat[2][0], 0.0f,
				vr_pose_mat[0][1], vr_pose_mat[1][1], vr_pose_mat[2][1], 0.0f,
				vr_pose_mat[0][2], vr_pose_mat[1][2], vr_pose_mat[2][2], 0.0f,
				vr_pose_mat[0][3], vr_pose_mat[1][3], vr_pose_mat[2][3], 1.0f
			},
			.velocity = { vr_pose.vVelocity.v[0], vr_pose.vVelocity.v[1], vr_pose.vVelocity.v[2] },
			.angular_velocity = { vr_pose.vAngularVelocity.v[0], vr_pose.vAngularVelocity.v[1], vr_pose.vAngularVelocity.v[2] },
			.status = (VR_TRACKING_STATUS)vr_pose.eTrackingResult,
			.device_is_connected = vr_pose.bDeviceIsConnected
		};
		poses.emplace_back(pose);
	}

	const auto& hmd_pose = vr_poses[vr::k_unTrackedDeviceIndex_Hmd];
	if (hmd_pose.bPoseIsValid) {
		const auto& vr_mat = hmd_pose.mDeviceToAbsoluteTracking;
		hmd_mat = {
			vr_mat.m[0][0], vr_mat.m[1][0], vr_mat.m[2][0], 0.0f,
			vr_mat.m[0][1], vr_mat.m[1][1], vr_mat.m[2][1], 0.0f,
			vr_mat.m[0][2], vr_mat.m[1][2], vr_mat.m[2][2], 0.0f,
			vr_mat.m[0][3], vr_mat.m[1][3], vr_mat.m[2][3], 1.0f
		};
		hmd_mat.invert();
	} else {
		hmd_mat.identity();
	}

	return true;
}

vector<shared_ptr<event_object>> openvr_context::update_input() {
	vr::VRActiveActionSet_t active_action_set {
		.ulActionSet = main_action_set,
		.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle,
		.ulSecondaryActionSet = vr::k_ulInvalidActionSetHandle,
		.unPadding = 0,
		.nPriority = 0,
	};
	const auto update_act_err = input->UpdateActionState(&active_action_set, sizeof(vr::VRActiveActionSet_t), 1);
	if (update_act_err != vr::EVRInputError::VRInputError_None) {
		log_error("failed to update action state: $", update_act_err);
		return {};
	}

	vector<shared_ptr<event_object>> events;
	const auto cur_time = SDL_GetTicks();
	for (auto& action : actions) {
		vr::EVRInputError err = vr::EVRInputError::VRInputError_None;
		switch (action.second.type) {
			case ACTION_TYPE::DIGITAL: {
				vr::InputDigitalActionData_t data {};
				err = input->GetDigitalActionData(action.second.handle, &data, sizeof(vr::InputDigitalActionData_t),
												  vr::k_ulInvalidInputValueHandle);
				if (err != vr::EVRInputError::VRInputError_None) {
					break;
				}
				if (data.bActive && data.bChanged) {
					switch (action.second.event_type) {
						case EVENT_TYPE::VR_APP_MENU_PRESS:
							events.emplace_back(make_shared<vr_app_menu_press_event>(cur_time, action.second.side, data.bState));
							break;
						case EVENT_TYPE::VR_APP_MENU_TOUCH:
							events.emplace_back(make_shared<vr_app_menu_touch_event>(cur_time, action.second.side, data.bState));
							break;
						case EVENT_TYPE::VR_MAIN_PRESS:
							events.emplace_back(make_shared<vr_main_press_event>(cur_time, action.second.side, data.bState));
							break;
						case EVENT_TYPE::VR_MAIN_TOUCH:
							events.emplace_back(make_shared<vr_main_touch_event>(cur_time, action.second.side, data.bState));
							break;
						case EVENT_TYPE::VR_SYSTEM_PRESS:
							events.emplace_back(make_shared<vr_system_press_event>(cur_time, action.second.side, data.bState));
							break;
						case EVENT_TYPE::VR_SYSTEM_TOUCH:
							events.emplace_back(make_shared<vr_system_touch_event>(cur_time, action.second.side, data.bState));
							break;
						case EVENT_TYPE::VR_TRACKPAD_PRESS:
							events.emplace_back(make_shared<vr_trackpad_press_event>(cur_time, action.second.side, data.bState));
							break;
						case EVENT_TYPE::VR_TRACKPAD_TOUCH:
							events.emplace_back(make_shared<vr_trackpad_touch_event>(cur_time, action.second.side, data.bState));
							break;
						case EVENT_TYPE::VR_THUMBSTICK_PRESS:
							events.emplace_back(make_shared<vr_thumbstick_press_event>(cur_time, action.second.side, data.bState));
							break;
						case EVENT_TYPE::VR_THUMBSTICK_TOUCH:
							events.emplace_back(make_shared<vr_thumbstick_touch_event>(cur_time, action.second.side, data.bState));
							break;
						case EVENT_TYPE::VR_TRIGGER_PRESS:
							events.emplace_back(make_shared<vr_trigger_press_event>(cur_time, action.second.side, data.bState));
							break;
						case EVENT_TYPE::VR_TRIGGER_TOUCH:
							events.emplace_back(make_shared<vr_trigger_touch_event>(cur_time, action.second.side, data.bState));
							break;
						case EVENT_TYPE::VR_GRIP_PRESS:
							events.emplace_back(make_shared<vr_grip_press_event>(cur_time, action.second.side, data.bState));
							break;
						case EVENT_TYPE::VR_GRIP_TOUCH:
							events.emplace_back(make_shared<vr_grip_touch_event>(cur_time, action.second.side, data.bState));
							break;
						default:
							log_error("unknown/unhandled VR event: $", action.first);
							break;
					}
				}
				break;
			}
			case ACTION_TYPE::ANALOG: {
				vr::InputAnalogActionData_t data {};
				err = input->GetAnalogActionData(action.second.handle, &data, sizeof(vr::InputAnalogActionData_t),
												 vr::k_ulInvalidInputValueHandle);
				if (err != vr::EVRInputError::VRInputError_None) {
					break;
				}
				if (data.bActive && (data.deltaX != 0.0f || data.deltaY != 0.0f || data.deltaZ != 0.0f)) {
					switch (action.second.event_type) {
						case EVENT_TYPE::VR_TRACKPAD_MOVE:
							events.emplace_back(make_shared<vr_trackpad_move_event>(cur_time, action.second.side,
																					float2 { data.x, data.y }, float2 { data.deltaX, data.deltaY }));
							break;
						case EVENT_TYPE::VR_THUMBSTICK_MOVE:
							events.emplace_back(make_shared<vr_thumbstick_move_event>(cur_time, action.second.side,
																					  float2 { data.x, data.y }, float2 { data.deltaX, data.deltaY }));
							break;
						case EVENT_TYPE::VR_TRIGGER_PULL:
							events.emplace_back(make_shared<vr_trigger_pull_event>(cur_time, action.second.side, data.x, data.deltaX));
							break;
						case EVENT_TYPE::VR_GRIP_PULL:
							events.emplace_back(make_shared<vr_grip_pull_event>(cur_time, action.second.side, data.x, data.deltaX));
							break;
						case EVENT_TYPE::VR_TRACKPAD_FORCE:
							events.emplace_back(make_shared<vr_trackpad_force_event>(cur_time, action.second.side, data.x, data.deltaX));
							break;
						case EVENT_TYPE::VR_GRIP_FORCE:
							events.emplace_back(make_shared<vr_grip_force_event>(cur_time, action.second.side, data.x, data.deltaX));
							break;
						default:
							log_error("unknown/unhandled VR event: $", action.first);
							break;
					}
				}
				break;
			}
			default:
				// not handled yet
				break;
		}
		if (err != vr::EVRInputError::VRInputError_None) {
			log_error("failed to update action $: $", action.first, err);
			continue;
		}
	}

	return events;
}

bool openvr_context::present(const compute_queue& cqueue, const compute_image& image) {
#if !defined(FLOOR_NO_VULKAN) && !defined(__APPLE__)
	// check if specified queue and image are actually from Vulkan
	if (const auto vk_queue_ptr = dynamic_cast<const vulkan_queue*>(&cqueue); !vk_queue_ptr) {
		log_error("specified queue is not a Vulkan queue");
		return false;
	}
	if (const auto vk_image_ptr = dynamic_cast<const vulkan_image*>(&image); !vk_image_ptr) {
		log_error("specified queue is not a Vulkan image");
		return false;
	}

	const auto& vk_queue = (const vulkan_queue&)cqueue;
	const auto& vk_dev = (const vulkan_device&)vk_queue.get_device();
	const auto& vk_image = (const vulkan_image&)image;

	const auto left_eye_image = vk_image.get_vulkan_aliased_layer_image(0);
	const auto right_eye_image = vk_image.get_vulkan_aliased_layer_image(1);
	if (left_eye_image == nullptr || right_eye_image == nullptr) {
		log_error("failed to retrieve aliased Vulkan layer image");
		return false;
	}

	// present VR images
	vr::VRVulkanTextureData_t vr_vk_image {
		.m_nImage = uint64_t(left_eye_image),
		.m_pDevice = vk_dev.device,
		.m_pPhysicalDevice = vk_dev.physical_device,
		.m_pInstance = ((const vulkan_compute*)vk_dev.context)->get_vulkan_context(),
		.m_pQueue = (VkQueue)const_cast<void*>(vk_queue.get_queue_ptr()),
		.m_nQueueFamilyIndex = vk_queue.get_family_index(),
		.m_nWidth = image.get_image_dim().x,
		.m_nHeight = image.get_image_dim().y,
		.m_nFormat = uint32_t(vk_image.get_vulkan_format()),
		.m_nSampleCount = 1,
	};
	vr::Texture_t vr_image {
		.handle = &vr_vk_image,
		.eType = vr::ETextureType::TextureType_Vulkan,
		// TODO: properly determine color space we're rendering in and how it is presented to the HMD
		.eColorSpace = vr::EColorSpace::ColorSpace_Gamma,
	};

	auto err = compositor->Submit(vr::EVREye::Eye_Left, &vr_image, nullptr, vr::EVRSubmitFlags::Submit_Default);
	if (err != vr::EVRCompositorError::VRCompositorError_None) {
		log_error("failed to submit left VR eye image: $", err);
		return false;
	}

	vr_vk_image.m_nImage = uint64_t(right_eye_image);
	err = compositor->Submit(vr::EVREye::Eye_Right, &vr_image, nullptr, vr::EVRSubmitFlags::Submit_Default);
	if (err != vr::EVRCompositorError::VRCompositorError_None) {
		log_error("failed to submit right VR eye image: $", err);
		return false;
	}

	return true;
#elif !defined(FLOOR_NO_METAL)
	(void)cqueue; // unused
	
	// check if specified image is actually from Metal
	if (const auto mtl_image_ptr = dynamic_cast<const metal_image*>(&image); !mtl_image_ptr) {
		log_error("specified queue is not a Metal image");
		return false;
	}
	const auto& mtl_image = (const metal_image&)image;

	// present VR image
	// NOTE: with TextureType_Metal we can directly present a layered 2D image
	vr::Texture_t vr_image {
		.handle = mtl_image.get_metal_image_void_ptr(),
		.eType = vr::ETextureType::TextureType_Metal,
		.eColorSpace = vr::EColorSpace::ColorSpace_Gamma,
	};

	auto err = compositor->Submit(vr::EVREye::Eye_Left, &vr_image, nullptr, vr::EVRSubmitFlags::Submit_Default);
	if (err != vr::EVRCompositorError::VRCompositorError_None) {
		log_error("failed to submit left VR eye image: $", err);
		return false;
	}

	err = compositor->Submit(vr::EVREye::Eye_Right, &vr_image, nullptr, vr::EVRSubmitFlags::Submit_Default);
	if (err != vr::EVRCompositorError::VRCompositorError_None) {
		log_error("failed to submit right VR eye image: $", err);
		return false;
	}
	
	return true;
#else
	log_error("non-Vulkan present path not implemented");
	return false;
#endif
}

vr_context::frame_view_state_t openvr_context::get_frame_view_state(const float& z_near, const float& z_far,
																	const bool with_position_in_mvm) const {
	auto mview_hmd = get_hmd_matrix();
	if (!with_position_in_mvm) {
		mview_hmd.set_translation(0.0f, 0.0f, 0.0f);
	}
	const auto eye_mat_left = get_eye_matrix(VR_EYE::LEFT);
	const auto eye_mat_right = get_eye_matrix(VR_EYE::RIGHT);
	const float eye_distance {
		(float3 { eye_mat_left.data[12], eye_mat_left.data[13], eye_mat_left.data[14] } -
		 float3 { eye_mat_right.data[12], eye_mat_right.data[13], eye_mat_right.data[14] }).length()
	};
	const auto hmd_inv_mat = get_hmd_matrix().inverted();
	const float3 hmd_position { -hmd_inv_mat.data[12], -hmd_inv_mat.data[13], -hmd_inv_mat.data[14] };

	return {
		hmd_position,
		eye_distance,
		mview_hmd * eye_mat_left,
		mview_hmd * eye_mat_right,
		get_projection_matrix(VR_EYE::LEFT, z_near, z_far),
		get_projection_matrix(VR_EYE::RIGHT, z_near, z_far)
	};
}

matrix4f openvr_context::get_projection_matrix(const VR_EYE& eye, const float& z_near, const float& z_far) const {
	// build our own projection matrix
	// NOTE: raw projection values are already pre-adjusted -> https://github.com/ValveSoftware/openvr/wiki/IVRSystem::GetProjectionRaw
	const auto fov_lrtb = get_projection_raw(eye);
	return matrix4f::perspective<true /* pre_adjusted_fov */, true /* is_right_handed */, true /* is_only_positive_z */>
								(fov_lrtb.x, fov_lrtb.y, fov_lrtb.z, fov_lrtb.w, z_near, z_far);
}

float4 openvr_context::get_projection_raw(const VR_EYE& eye) const {
	float4 ret;
	system->GetProjectionRaw(eye_to_eye(eye), &ret.x, &ret.y, &ret.z, &ret.w);
	return ret;
}

matrix4f openvr_context::get_eye_matrix(const VR_EYE& eye) const {
	const auto mat = system->GetEyeToHeadTransform(eye_to_eye(eye));
	return matrix4f(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], 0.0f,
		mat.m[0][1], mat.m[1][1], mat.m[2][1], 0.0f,
		mat.m[0][2], mat.m[1][2], mat.m[2][2], 0.0f,
		mat.m[0][3], mat.m[1][3], mat.m[2][3], 1.0f
	).inverted();
}

const matrix4f& openvr_context::get_hmd_matrix() const {
	return hmd_mat;
}

#endif
