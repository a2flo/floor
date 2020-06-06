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

#ifndef __FLOOR_GRAPHICS_GRAPHICS_RENDERER_HPP__
#define __FLOOR_GRAPHICS_GRAPHICS_RENDERER_HPP__

#include <floor/compute/compute_queue.hpp>
#include <floor/graphics/graphics_pass.hpp>
#include <floor/graphics/graphics_pipeline.hpp>
#include <floor/core/logger.hpp>

//! renderer object for a specific pass and one or more pipelines
//! NOTE: create this every time something should be rendered (this doesn't/shouldn't be a static object)
class graphics_renderer {
public:
	//////////////////////////////////////////
	// renderer construction and handling
	//! NOTE: if "multi_view" is true, this will create a multi-view/VR renderer, otherwise a single-view renderer will be created
	graphics_renderer(const compute_queue& cqueue_, const graphics_pass& pass_, const graphics_pipeline& pipeline, const bool multi_view_ = false);
	virtual ~graphics_renderer() = default;
	
	//! certain render settings can be modified dynamically at run-time, overwriting the values specified in the graphics_pass/graphics_pipeline
	struct dynamic_render_state_t {
		//! if set, overwrites the graphics_pipeline viewport
		optional<decltype(render_pipeline_description::viewport)> viewport;
		//! if set, overwrites the graphics_pipeline scissor rectangle
		optional<render_pipeline_description::scissor_t> scissor;
		//! if set, overwrites the per-attachment clear value
		//! NOTE: if set, clear values for all attachments must be set
		optional<vector<clear_value_t>> clear_values;
	};
	
	//! begins drawing with the specified pass and pipeline
	virtual bool begin(const dynamic_render_state_t dynamic_render_state floor_unused = {}) {
		return true;
	}
	
	//! ends drawing with the specified pass and pipeline
	virtual bool end() {
		return true;
	}
	
	//! commits all currently queued work to the queue
	virtual bool commit() {
		return true;
	}

	//! returns true if this is a multi-view/VR renderer
	bool is_multi_view() const {
		return multi_view;
	}
	
	//////////////////////////////////////////
	// screen presentation and drawable
	
	//! drawable screen surface/texture/image used to actually draw something on the screen,
	//! to be implemented by each backend
	struct drawable_t {
		virtual ~drawable_t();
		compute_image* image { nullptr };
		
		//! returns true if this drawable is in a valid state
		bool is_valid() const {
			return valid;
		}
		
	protected:
		bool valid { false };
		
	};
	
	//! retrieves the next drawable screen surface,
	//! returns nullptr if there is none (mostly due to the screen being in an invalid/non-renderable state)
	virtual drawable_t* get_next_drawable(const bool get_multi_view_drawable = false) = 0;
	
	//! present the current drawable to the screen
	virtual void present() = 0;
	
	//////////////////////////////////////////
	// attachments
	
	//! identifies an attachment at an specific index in the pass/pipeline
	struct attachment_t {
		//! index of the attachment in graphics_pipeline/graphics_pass,
		//! if ~0u -> determine index automatically
		uint32_t index { ~0u };
		//! reference to the backing image
		compute_image& image;
		
#if !defined(FLOOR_DEBUG)
		attachment_t(compute_image& image_) noexcept : image(image_) {}
		attachment_t(compute_image* image_) noexcept : image(*image_) {}
		attachment_t(shared_ptr<compute_image>& image_) noexcept : image(*image_) {}
		attachment_t(unique_ptr<compute_image>& image_) noexcept : image(*image_) {}
		attachment_t(drawable_t& drawable) noexcept : image(*drawable.image) {}
		attachment_t(drawable_t* drawable) noexcept : image(*drawable->image) {}
		
		attachment_t(const uint32_t& index_, compute_image& image_) : index(index_), image(image_) {}
		attachment_t(const uint32_t& index_, drawable_t& drawable) : index(index_), image(*drawable.image) {}
#else
		compute_image& image_sanity_check(compute_image* image_) {
			if (image_ == nullptr) {
				log_error("attachment image is nullptr!");
			}
			return *image_;
		}
		compute_image& drawable_sanity_check(drawable_t* drawable_) {
			if (drawable_ == nullptr) {
				log_error("drawable is nullptr!");
			}
			return image_sanity_check(drawable_->image);
		}
		
		attachment_t(compute_image& image_) noexcept : image(image_) {}
		attachment_t(compute_image* image_) noexcept : image(image_sanity_check(image_)) {}
		attachment_t(shared_ptr<compute_image>& image_) noexcept : image(image_sanity_check(image_.get())) {}
		attachment_t(unique_ptr<compute_image>& image_) noexcept : image(image_sanity_check(image_.get())) {}
		attachment_t(drawable_t& drawable) noexcept : image(image_sanity_check(drawable.image)) {}
		attachment_t(drawable_t* drawable) noexcept : image(drawable_sanity_check(drawable)) {}
		
		attachment_t(const uint32_t& index_, compute_image& image_) : index(index_), image(image_) {}
		attachment_t(const uint32_t& index_, drawable_t& drawable) : index(index_), image(image_sanity_check(drawable.image)) {}
#endif
	};
	
	//! set all pass/pipeline attachments
	//! NOTE: depth attachments are automatically detected
	//! NOTE: resets all previously set attachments
	template <typename... Args>
	bool set_attachments(Args&&... attachments) {
		vector<attachment_t> atts { forward<Args>(attachments)... };
		return set_attachments(atts);
	}
	
	//! set all pass/pipeline attachments
	//! NOTE: depth attachments are automatically detected
	//! NOTE: resets all previously set attachments
	virtual bool set_attachments(vector<attachment_t>& attachments);
	
	//! manually set or update/replace an attachment at a specific index
	//! NOTE: depth attachments are automatically detected
	virtual bool set_attachment(const uint32_t& index, attachment_t& attachment);

	//////////////////////////////////////////
	// pipeline functions
	
	//! switches this renderer/pass to a different pipeline
	//! NOTE: must only be called before begin() or after end(), not when actively rendering
	virtual bool switch_pipeline(const graphics_pipeline& pipeline);

	//////////////////////////////////////////
	// draw calls
	
	//! simple draw info with contiguous vertices creating a primitive
	struct multi_draw_entry {
		uint32_t vertex_count;
		uint32_t instance_count { 1u };
		uint32_t first_vertex { 0u };
		uint32_t first_instance { 0u };
	};
	
	//! draw info with primitives created via indices into the vertex buffer
	struct multi_draw_indexed_entry {
		compute_buffer* index_buffer;
		uint32_t index_count;
		uint32_t instance_count { 1u };
		uint32_t first_index { 0u };
		int32_t vertex_offset { 0u };
		uint32_t first_instance { 0u };
	};
	
	//! emit simple draw calls with the per-draw-call information stored in "draw_entries"
	//! NOTE: vertex shader arguments are specified first, fragment shader arguments after
	template <typename... Args> void multi_draw(const vector<multi_draw_entry>& draw_entries, const Args&... args) const {
		draw_internal(&draw_entries, nullptr, { args... });
	}
	
	//! emit indexed draw calls with the per-draw-call information stored in "draw_entries"
	//! NOTE: vertex shader arguments are specified first, fragment shader arguments after
	template <typename... Args> void multi_draw_indexed(const vector<multi_draw_indexed_entry>& draw_entries, const Args&... args) const {
		draw_internal(nullptr, &draw_entries, { args... });
	}
	
	//////////////////////////////////////////
	// misc
	
	//! returns true if this renderer is in a valid state
	bool is_valid() const {
		return valid;
	}
	
protected:
	const compute_queue& cqueue;
	const compute_context& ctx;
	const graphics_pass& pass;
	const graphics_pipeline* cur_pipeline { nullptr };
	flat_map<uint32_t, compute_image*> attachments_map;
	compute_image* depth_attachment { nullptr };
	bool valid { false };
	const bool multi_view { false };
	
	//! internal draw call dispatcher for the respective backend
	virtual void draw_internal(const vector<multi_draw_entry>* draw_entries,
							   const vector<multi_draw_indexed_entry>* draw_indexed_entries,
							   const vector<compute_kernel_arg>& args) const = 0;
	
	//! sets the depth attachment
	virtual bool set_depth_attachment(attachment_t& attachment);
	
};

#endif
