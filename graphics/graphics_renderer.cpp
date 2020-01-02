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

#include <floor/graphics/graphics_renderer.hpp>
#include <floor/compute/compute_image.hpp>
#include <floor/core/logger.hpp>
#include <unordered_set>

graphics_renderer::graphics_renderer(const compute_queue& cqueue_, const graphics_pass& pass_, const graphics_pipeline& pipeline, const bool multi_view_) :
cqueue(cqueue_), ctx(*cqueue.get_device().context), pass(pass_), cur_pipeline(&pipeline), multi_view(multi_view_) {
	// TODO: check validity, check compat between pass <-> pipeline, check if cqueue dev is in pass+pipeline
	
	// all successful, mark as valid for now (can be changed to false again by derived implementations)
	valid = true;
}

graphics_renderer::drawable_t::~drawable_t() {
	// nop
}

bool graphics_renderer::switch_pipeline(const graphics_pipeline& pipeline) {
	// TODO: sanity checks
	if (!pipeline.is_valid()) {
		return false;
	}
	cur_pipeline = &pipeline;
	return true;
}

bool graphics_renderer::set_attachments(vector<attachment_t>& attachments) {
	// TODO: sanity check
	
	// determine all fixed attachment indices
	unordered_set<uint32_t> occupied_att_indices;
	for (const auto& att : attachments) {
		if (att.index != ~0u) {
			if (has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(att.image.get_image_type())) {
				continue; // depth attachment is not assigned to an index
			}
			if (const auto [iter, success] = occupied_att_indices.emplace(att.index); !success) {
				log_error("attachment index %u is specified multiple times!", att.index);
				return false;
			}
		}
	}
	
	// clear old + prepare for new
	attachments_map.clear();
	attachments_map.reserve(attachments.size());
	
	// set each attachment
	uint32_t running_idx = 0;
	for (auto& att : attachments) {
		if (has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(att.image.get_image_type())) {
			set_depth_attachment(att);
			continue;
		}
		
		if (att.index != ~0u) {
			set_attachment(att.index, att);
		} else {
			// get the next non-occupied index
			while (occupied_att_indices.count(running_idx) > 0) {
				++running_idx;
			}
			set_attachment(running_idx, att);
			++running_idx;
		}
	}
	
	return true;
}

bool graphics_renderer::set_attachment(const uint32_t& index, attachment_t& attachment) {
	if (has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(attachment.image.get_image_type())) {
		return set_depth_attachment(attachment);
	}
	attachments_map.insert_or_assign(index, &attachment.image);
	return true;
}

bool graphics_renderer::set_depth_attachment(attachment_t& attachment) {
	depth_attachment = &attachment.image;
	return true;
}
