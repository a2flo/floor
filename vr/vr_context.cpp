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
