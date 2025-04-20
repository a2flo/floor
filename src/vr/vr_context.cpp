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

#include <floor/vr/vr_context.hpp>

#include <floor/floor.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>

namespace fl {

vr_context::vr_context() {
}

vr_context::~vr_context() {
}

std::string_view vr_context::pose_type_to_string(const POSE_TYPE type) {
	switch (type) {
		case POSE_TYPE::UNKNOWN:
			return "unknown";
		case POSE_TYPE::HEAD:
			return "head";
		case POSE_TYPE::HAND_LEFT:
			return "hand-left";
		case POSE_TYPE::HAND_RIGHT:
			return "hand-right";
		case POSE_TYPE::HAND_LEFT_AIM:
			return "hand-left-aim";
		case POSE_TYPE::HAND_RIGHT_AIM:
			return "hand-right-aim";
		case POSE_TYPE::REFERENCE:
			return "reference";
		case POSE_TYPE::SPECIAL:
			return "special";
		case POSE_TYPE::TRACKER:
			return "tracker";
		case POSE_TYPE::TRACKER_HANDHELD_OBJECT:
			return "tracker-handheld-object";
		case POSE_TYPE::TRACKER_FOOT_LEFT:
			return "tracker-foot-left";
		case POSE_TYPE::TRACKER_FOOT_RIGHT:
			return "tracker-foot-right";
		case POSE_TYPE::TRACKER_SHOULDER_LEFT:
			return "tracker-shoulder-left";
		case POSE_TYPE::TRACKER_SHOULDER_RIGHT:
			return "tracker-shoulder-right";
		case POSE_TYPE::TRACKER_ELBOW_LEFT:
			return "tracker-elbow-left";
		case POSE_TYPE::TRACKER_ELBOW_RIGHT:
			return "tracker-elbow-right";
		case POSE_TYPE::TRACKER_KNEE_LEFT:
			return "tracker-knee-left";
		case POSE_TYPE::TRACKER_KNEE_RIGHT:
			return "tracker-knee-right";
		case POSE_TYPE::TRACKER_WAIST:
			return "tracker-waist";
		case POSE_TYPE::TRACKER_CHEST:
			return "tracker-chest";
		case POSE_TYPE::TRACKER_CAMERA:
			return "tracker-camera";
		case POSE_TYPE::TRACKER_KEYBOARD:
			return "tracker-keyboard";
		case POSE_TYPE::TRACKER_WRIST_LEFT:
			return "tracker-wrist-left";
		case POSE_TYPE::TRACKER_WRIST_RIGHT:
			return "tracker-wrist-right";
		case POSE_TYPE::TRACKER_ANKLE_LEFT:
			return "tracker-ankle-left";
		case POSE_TYPE::TRACKER_ANKLE_RIGHT:
			return "tracker-ankle-right";
		case POSE_TYPE::HAND_JOINT_PALM_LEFT:
			return "hand-joint-palm-left";
		case POSE_TYPE::HAND_JOINT_WRIST_LEFT:
			return "hand-joint-wrist-left";
		case POSE_TYPE::HAND_JOINT_THUMB_METACARPAL_LEFT:
			return "hand-joint-thumb-metacarpal-left";
		case POSE_TYPE::HAND_JOINT_THUMB_PROXIMAL_LEFT:
			return "hand-joint-thumb-proximal-left";
		case POSE_TYPE::HAND_JOINT_THUMB_DISTAL_LEFT:
			return "hand-joint-thumb-distal-left";
		case POSE_TYPE::HAND_JOINT_THUMB_TIP_LEFT:
			return "hand-joint-thumb-tip-left";
		case POSE_TYPE::HAND_JOINT_INDEX_METACARPAL_LEFT:
			return "hand-joint-index-metacarpal-left";
		case POSE_TYPE::HAND_JOINT_INDEX_PROXIMAL_LEFT:
			return "hand-joint-index-proximal-left";
		case POSE_TYPE::HAND_JOINT_INDEX_INTERMEDIATE_LEFT:
			return "hand-joint-index-intermediate-left";
		case POSE_TYPE::HAND_JOINT_INDEX_DISTAL_LEFT:
			return "hand-joint-index-distal-left";
		case POSE_TYPE::HAND_JOINT_INDEX_TIP_LEFT:
			return "hand-joint-index-tip-left";
		case POSE_TYPE::HAND_JOINT_MIDDLE_METACARPAL_LEFT:
			return "hand-joint-middle-metacarpal-left";
		case POSE_TYPE::HAND_JOINT_MIDDLE_PROXIMAL_LEFT:
			return "hand-joint-middle-proximal-left";
		case POSE_TYPE::HAND_JOINT_MIDDLE_INTERMEDIATE_LEFT:
			return "hand-joint-middle-intermediate-left";
		case POSE_TYPE::HAND_JOINT_MIDDLE_DISTAL_LEFT:
			return "hand-joint-middle-distal-left";
		case POSE_TYPE::HAND_JOINT_MIDDLE_TIP_LEFT:
			return "hand-joint-middle-tip-left";
		case POSE_TYPE::HAND_JOINT_RING_METACARPAL_LEFT:
			return "hand-joint-ring-metacarpal-left";
		case POSE_TYPE::HAND_JOINT_RING_PROXIMAL_LEFT:
			return "hand-joint-ring-proximal-left";
		case POSE_TYPE::HAND_JOINT_RING_INTERMEDIATE_LEFT:
			return "hand-joint-ring-intermediate-left";
		case POSE_TYPE::HAND_JOINT_RING_DISTAL_LEFT:
			return "hand-joint-ring-distal-left";
		case POSE_TYPE::HAND_JOINT_RING_TIP_LEFT:
			return "hand-joint-ring-tip-left";
		case POSE_TYPE::HAND_JOINT_LITTLE_METACARPAL_LEFT:
			return "hand-joint-little-metacarpal-left";
		case POSE_TYPE::HAND_JOINT_LITTLE_PROXIMAL_LEFT:
			return "hand-joint-little-proximal-left";
		case POSE_TYPE::HAND_JOINT_LITTLE_INTERMEDIATE_LEFT:
			return "hand-joint-little-intermediate-left";
		case POSE_TYPE::HAND_JOINT_LITTLE_DISTAL_LEFT:
			return "hand-joint-little-distal-left";
		case POSE_TYPE::HAND_JOINT_LITTLE_TIP_LEFT:
			return "hand-joint-little-tip-left";
		case POSE_TYPE::HAND_FOREARM_JOINT_ELBOW_LEFT:
			return "hand-forearm-joint-elbow-left";
		case POSE_TYPE::HAND_JOINT_PALM_RIGHT:
			return "hand-joint-palm-right";
		case POSE_TYPE::HAND_JOINT_WRIST_RIGHT:
			return "hand-joint-wrist-right";
		case POSE_TYPE::HAND_JOINT_THUMB_METACARPAL_RIGHT:
			return "hand-joint-thumb-metacarpal-right";
		case POSE_TYPE::HAND_JOINT_THUMB_PROXIMAL_RIGHT:
			return "hand-joint-thumb-proximal-right";
		case POSE_TYPE::HAND_JOINT_THUMB_DISTAL_RIGHT:
			return "hand-joint-thumb-distal-right";
		case POSE_TYPE::HAND_JOINT_THUMB_TIP_RIGHT:
			return "hand-joint-thumb-tip-right";
		case POSE_TYPE::HAND_JOINT_INDEX_METACARPAL_RIGHT:
			return "hand-joint-index-metacarpal-right";
		case POSE_TYPE::HAND_JOINT_INDEX_PROXIMAL_RIGHT:
			return "hand-joint-index-proximal-right";
		case POSE_TYPE::HAND_JOINT_INDEX_INTERMEDIATE_RIGHT:
			return "hand-joint-index-intermediate-right";
		case POSE_TYPE::HAND_JOINT_INDEX_DISTAL_RIGHT:
			return "hand-joint-index-distal-right";
		case POSE_TYPE::HAND_JOINT_INDEX_TIP_RIGHT:
			return "hand-joint-index-tip-right";
		case POSE_TYPE::HAND_JOINT_MIDDLE_METACARPAL_RIGHT:
			return "hand-joint-middle-metacarpal-right";
		case POSE_TYPE::HAND_JOINT_MIDDLE_PROXIMAL_RIGHT:
			return "hand-joint-middle-proximal-right";
		case POSE_TYPE::HAND_JOINT_MIDDLE_INTERMEDIATE_RIGHT:
			return "hand-joint-middle-intermediate-right";
		case POSE_TYPE::HAND_JOINT_MIDDLE_DISTAL_RIGHT:
			return "hand-joint-middle-distal-right";
		case POSE_TYPE::HAND_JOINT_MIDDLE_TIP_RIGHT:
			return "hand-joint-middle-tip-right";
		case POSE_TYPE::HAND_JOINT_RING_METACARPAL_RIGHT:
			return "hand-joint-ring-metacarpal-right";
		case POSE_TYPE::HAND_JOINT_RING_PROXIMAL_RIGHT:
			return "hand-joint-ring-proximal-right";
		case POSE_TYPE::HAND_JOINT_RING_INTERMEDIATE_RIGHT:
			return "hand-joint-ring-intermediate-right";
		case POSE_TYPE::HAND_JOINT_RING_DISTAL_RIGHT:
			return "hand-joint-ring-distal-right";
		case POSE_TYPE::HAND_JOINT_RING_TIP_RIGHT:
			return "hand-joint-ring-tip-right";
		case POSE_TYPE::HAND_JOINT_LITTLE_METACARPAL_RIGHT:
			return "hand-joint-little-metacarpal-right";
		case POSE_TYPE::HAND_JOINT_LITTLE_PROXIMAL_RIGHT:
			return "hand-joint-little-proximal-right";
		case POSE_TYPE::HAND_JOINT_LITTLE_INTERMEDIATE_RIGHT:
			return "hand-joint-little-intermediate-right";
		case POSE_TYPE::HAND_JOINT_LITTLE_DISTAL_RIGHT:
			return "hand-joint-little-distal-right";
		case POSE_TYPE::HAND_JOINT_LITTLE_TIP_RIGHT:
			return "hand-joint-little-tip-right";
		case POSE_TYPE::HAND_FOREARM_JOINT_ELBOW_RIGHT:
			return "hand-forearm-joint-elbow-right";
	}
	return "<unhandled>";
}

std::string_view vr_context::controller_type_to_string(const CONTROLLER_TYPE type) {
	switch (type) {
		case CONTROLLER_TYPE::NONE:
			return "<none>";
		case CONTROLLER_TYPE::KHRONOS_SIMPLE:
			return "Khronos simple controller";
		case CONTROLLER_TYPE::INDEX:
			return "Valve Index controller";
		case CONTROLLER_TYPE::HTC_VIVE:
			return "HTC Vive controller";
		case CONTROLLER_TYPE::GOOGLE_DAYDREAM:
			return "Google Daydream controller";
		case CONTROLLER_TYPE::MICROSOFT_MIXED_REALITY:
			return "Microsoft Mixed Reality Motion controller";
		case CONTROLLER_TYPE::OCULUS_GO:
			return "Oculus Go controller";
		case CONTROLLER_TYPE::OCULUS_TOUCH:
			return "Oculus Touch controller";
		case CONTROLLER_TYPE::HP_MIXED_REALITY:
			return "HP Mixed Reality controller";
		case CONTROLLER_TYPE::HTC_VIVE_COSMOS:
			return "HTC Vive Cosmos controller";
		case CONTROLLER_TYPE::HTC_VIVE_FOCUS3:
			return "HTC Vive Focus 3 controller";
		case CONTROLLER_TYPE::HUAWEI:
			return "Huawei controller";
		case CONTROLLER_TYPE::SAMSUNG_ODYSSEY:
			return "Samsung Odyssey controller";
		case CONTROLLER_TYPE::MAGIC_LEAP2:
			return "Magic Leap 2 controller";
		case CONTROLLER_TYPE::OCULUS_TOUCH_PRO:
			return "Meta Quest Touch Pro controller";
		case CONTROLLER_TYPE::PICO_NEO3:
			return "PICO Neo3 controller";
		case CONTROLLER_TYPE::PICO4:
			return "PICO 4 controller";
		case CONTROLLER_TYPE::__MAX_CONTROLLER_TYPE:
			break;
	}
	return "<unhandled>";
}

} // namespace fl
