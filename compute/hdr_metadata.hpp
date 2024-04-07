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

#include <array>
#include <floor/math/vector_lib.hpp>

//! HDR metadata of the mastering display
struct hdr_metadata_t {
	//! RGB color primaries (in CIE 1931)
	//! NOTE: defaults to BT.2020 color space
	array<float2, 3> primaries {{
		{ 0.708f, 0.292f },
		{ 0.170f, 0.797f },
		{ 0.131f, 0.046f },
	}};
	//! white point (in CIE 1931)
	//! NOTE: defaults to BT.2020 color space
	float2 white_point { 0.3127f, 0.3290f };
	//! nominal min/max luminance (in cd/m^2)
	//! NOTE: with SMPTE ST.2086, min must be a multiple of 0.0001, max must be a multiple of 1
	float2 luminance { 0.01f, 1000.0f };
	//! upper bound on the maximum light level among all RGB samples (in cd/m^2),
	//! i.e. the brightest pixel does not exceed this light level
	float max_content_light_level { 1000.0f };
	//! upper bound on the maximum average light level among RGB samples (in cd/m^2),
	//! i.e. the average brightness of all pixels does not exceed this light level
	float max_average_light_level { 500.0f };
};
