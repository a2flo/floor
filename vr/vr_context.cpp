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

#include <floor/vr/vr_context.hpp>

#include <floor/floor/floor.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>

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
		case POSE_TYPE::TRACKER:
			return "tracker";
		case POSE_TYPE::REFERENCE:
			return "reference";
		case POSE_TYPE::SPECIAL:
			return "special";
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
