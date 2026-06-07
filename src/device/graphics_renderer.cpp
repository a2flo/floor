/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#include <floor/device/graphics_renderer.hpp>
#include <floor/device/device_image.hpp>
#include <floor/device/device.hpp>
#include <floor/device/device_context.hpp>
#include <floor/device/generic_indirect_command.hpp>
#include <floor/core/logger.hpp>
#include <floor/floor.hpp>
#include <unordered_set>

namespace fl {

bool graphics_renderer::is_software_indirect(const device_context& ctx) {
	switch (ctx.get_platform_type()) {
		case PLATFORM_TYPE::METAL:
			return floor::get_metal_soft_indirect();
		case PLATFORM_TYPE::VULKAN:
			return floor::get_vulkan_soft_indirect();
		default:
			break;
	}
	return false;
}

graphics_renderer::graphics_renderer(const device_queue& cqueue_, const graphics_pass& pass_, const graphics_pipeline& pipeline, const bool multi_view_) :
cqueue(cqueue_), ctx(cqueue.get_mutable_context()), pass(pass_), cur_pipeline(&pipeline), multi_view(multi_view_),
is_indirect(pipeline.get_description(multi_view_).options.support_indirect_rendering && !is_software_indirect(ctx)),
is_dynamic_cull_state(pipeline.get_description(multi_view_).options.dynamic_cull_state) {
	// TODO: check validity, check compat between pass <-> pipeline, check if cqueue dev is in pass+pipeline
	
	// all successful, mark as valid for now (can be changed to false again by derived implementations)
	valid = true;
}

graphics_renderer::drawable_t::~drawable_t() {
	// nop
}

bool graphics_renderer::switch_pipeline(const graphics_pipeline& pipeline) {
	// sanity checks
	if (!pipeline.is_valid()) {
		return false;
	}
	
	const auto cur_cull_mode = cur_pipeline->get_description(multi_view).cull_mode;
	const auto new_cull_mode = pipeline.get_description(multi_view).cull_mode;
	if (cur_cull_mode != new_cull_mode) {
		if (!is_dynamic_cull_state || !pipeline.get_description(multi_view).options.dynamic_cull_state) {
			log_error("switched-to pipeline has a different pipeline than the current pipeline, but dynamic cull state is not enabled");
			return false;
		}
		switch_cull_mode(new_cull_mode);
	}
	
	// -> all valid
	cur_pipeline = &pipeline;
	return true;
}

bool graphics_renderer::set_attachments(std::vector<attachment_t>& attachments) {
	if (attachments.empty()) {
		log_error("no attachments set");
		return false;
	}
#if defined(FLOOR_DEBUG)
	const auto attachment_dim = attachments[0].image->get_image_dim().xy;
	for (uint32_t att_idx = 1, att_count = (uint32_t)attachments.size(); att_idx < att_count; ++att_idx) {
		if (attachments[att_idx].image->get_image_dim().xy != attachment_dim) {
			log_error("attachment dim mismatch @$: got $, expected $",
					  att_idx, attachments[att_idx].image->get_image_dim(), attachment_dim);
			return false;
		}
		if (attachments[att_idx].resolve_image &&
			attachments[att_idx].resolve_image->get_image_dim().xy != attachment_dim) {
			log_error("attachment dim mismatch @$ (resolve): got $, expected $",
					  att_idx, attachments[att_idx].resolve_image->get_image_dim(), attachment_dim);
			return false;
		}
	}
#endif
	
	// determine all fixed attachment indices
	std::unordered_set<uint32_t> occupied_att_indices;
	for (const auto& att : attachments) {
		if (att.index != ~0u) {
			if (has_flag<IMAGE_TYPE::FLAG_DEPTH>(att.image->get_image_type())) {
				continue; // depth attachment is not assigned to an index
			}
			if (const auto [iter, success] = occupied_att_indices.emplace(att.index); !success) {
				log_error("attachment index $ is specified multiple times!", att.index);
				return false;
			}
		}
	}
	
	// clear old + prepare for new
	attachments_map.clear();
	
	// set each attachment
	uint32_t running_idx = 0;
	for (auto& att : attachments) {
		if (has_flag<IMAGE_TYPE::FLAG_DEPTH>(att.image->get_image_type())) {
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
	if (has_flag<IMAGE_TYPE::FLAG_DEPTH>(attachment.image->get_image_type())) {
		return set_depth_attachment(attachment);
	}
	attachments_map.insert_or_assign(index, attachment);
	return true;
}

bool graphics_renderer::set_depth_attachment(attachment_t& attachment) {
	depth_attachment = attachment;
	return true;
}

bool graphics_renderer::set_tessellation_factors(const device_buffer& tess_factors_buffer floor_unused) {
	return true;
}

// NOTE: this is the generic implementation (using generic_indirect_command_pipeline) and can only be active when determined by the *_context,
//       e.g. the resp. context must have created a generic_indirect_command_pipeline rather than an impl specific one for this to work
void graphics_renderer::execute_indirect(const indirect_command_pipeline& indirect_cmd) {
	const auto command_count = indirect_cmd.get_command_count();
	if (command_count == 0) {
		return;
	}
	
#if defined(FLOOR_DEBUG)
	if (const auto cmd_type = indirect_cmd.get_description().command_type;
		cmd_type != indirect_command_description::COMMAND_TYPE::RENDER &&
		cmd_type != indirect_command_description::COMMAND_TYPE::RENDER_MESH) {
		log_error("specified indirect command pipeline \"$\" must be a render pipeline",
				  indirect_cmd.get_description().debug_label);
		return;
	}
#endif
	
	const auto& dev = cqueue.get_device();
	const auto& generic_indirect_cmd = (const generic_indirect_command_pipeline&)indirect_cmd;
	const auto generic_indirect_pipeline_entry = generic_indirect_cmd.get_pipeline_entry(dev);
	if (!generic_indirect_pipeline_entry) {
		log_error("no indirect command pipeline state for device \"$\" in indirect command pipeline \"$\"",
				  dev.name, indirect_cmd.get_description().debug_label);
		return;
	}
	
	for (uint32_t cmd_idx = 0u; cmd_idx < command_count; ++cmd_idx) {
		const auto& cmd = generic_indirect_pipeline_entry->commands[cmd_idx];
		if (cmd.render.ms_ptr) {
			const mesh_draw_entry draw_entry {
				.work_group_count = cmd.render.mesh.work_group_count,
				.local_work_size_task = cmd.render.mesh.local_work_size_task,
				.local_work_size_mesh = cmd.render.mesh.local_work_size_mesh,
			};
			draw_mesh_internal(draw_entry, cmd.args);
		} else if (!cmd.render.index_buffer) {
			std::array<multi_draw_entry, 1> draw_entry { multi_draw_entry {
				.vertex_count = cmd.render.vertex.vertex_count,
				.instance_count = cmd.render.vertex.instance_count,
				.first_vertex = cmd.render.vertex.first_vertex,
				.first_instance = cmd.render.vertex.first_instance,
			}};
			draw_internal(draw_entry, {}, cmd.args);
		} else {
			std::array<multi_draw_indexed_entry, 1> draw_entry { multi_draw_indexed_entry {
				.index_buffer = cmd.render.index_buffer,
				.index_count = cmd.render.indexed.index_count,
				.instance_count = cmd.render.indexed.instance_count,
				.first_index = cmd.render.indexed.first_index,
				.vertex_offset = cmd.render.indexed.vertex_offset,
				.first_instance = cmd.render.indexed.first_instance,
				.index_type = cmd.render.indexed.index_type,
			}};
			draw_internal({}, draw_entry, cmd.args);
		}
	}
}

#if defined(FLOOR_DEBUG)
bool graphics_renderer::is_valid_mesh_draw(const mesh_draw_entry& draw_entry,
										   const toolchain::function_info* ts_info,
										   const toolchain::function_info& ms_info) {
	const auto mesh_kernel_dim = ms_info.get_kernel_dim();
	const auto first_kernel_dim = (!ts_info ? mesh_kernel_dim : ts_info->get_kernel_dim());
	if ((first_kernel_dim == 1 && draw_entry.work_group_count.y > 1) ||
		(first_kernel_dim <= 2 && draw_entry.work_group_count.z > 1)) {
		log_error("invalid work-group count for $D task/mesh shader: $", first_kernel_dim, draw_entry.work_group_count);
		return false;
	}
	
	if (ts_info) {
		if ((first_kernel_dim == 1 && draw_entry.local_work_size_task.y > 1) ||
			(first_kernel_dim <= 2 && draw_entry.local_work_size_task.z > 1)) {
			log_error("invalid $D task shader local work size: $", first_kernel_dim, draw_entry.local_work_size_task);
			return false;
		}
		if (ts_info->has_valid_required_local_size() &&
			(draw_entry.local_work_size_task != ts_info->required_local_size).any()) {
			log_error("invalid task shader local work size: $ (required: $)", draw_entry.local_work_size_task,
					  ts_info->required_local_size);
			return false;
		}
	}
	
	if ((mesh_kernel_dim == 1 && draw_entry.local_work_size_mesh.y > 1) ||
		(mesh_kernel_dim <= 2 && draw_entry.local_work_size_mesh.z > 1)) {
		log_error("invalid $D mesh shader local work size: $", mesh_kernel_dim, draw_entry.local_work_size_mesh);
		return false;
	}
	
	if (ms_info.has_valid_required_local_size() &&
		(draw_entry.local_work_size_mesh != ms_info.required_local_size).any()) {
		log_error("invalid mesh shader local work size: $ (required: $)", draw_entry.local_work_size_mesh,
				  ms_info.required_local_size);
		return false;
	}
	return true;
}
#endif

} // namespace fl
