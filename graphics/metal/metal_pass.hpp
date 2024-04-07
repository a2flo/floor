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

#include <floor/compute/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/graphics/graphics_pass.hpp>
#include <Metal/Metal.h>

class metal_pass final : public graphics_pass {
public:
	metal_pass(const render_pass_description& pass_desc, const bool with_multi_view_support = false);
	~metal_pass() override;
	
	//! returns the corresponding MTLLoadAction for the specified LOAD_OP
	static MTLLoadAction metal_load_action_from_load_op(const LOAD_OP& load_op);
	
	//! returns the corresponding MTLStoreAction for the specified STORE_OP
	static MTLStoreAction metal_store_action_from_store_op(const STORE_OP& store_op);
	
	//! creates an encoder for this render pass description
	id <MTLRenderCommandEncoder> create_encoder(id <MTLCommandBuffer> cmd_buffer,
												const bool create_multi_view) const;
	
	//! returns the Metal render pass descriptor
	const MTLRenderPassDescriptor* get_metal_pass_desc(const bool get_multi_view) const {
		return (!get_multi_view ? sv_mtl_pass_desc : mv_mtl_pass_desc);
	}
	
protected:
	MTLRenderPassDescriptor* sv_mtl_pass_desc { nil };
	MTLRenderPassDescriptor* mv_mtl_pass_desc { nil };
	
};

#endif
