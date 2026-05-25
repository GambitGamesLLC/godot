/**************************************************************************/
/*  renderer_canvas_render_rd.cpp                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "renderer_canvas_render_rd.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/math/geometry_2d.h"
#include "core/math/math_defs.h"
#include "core/math/math_funcs.h"
#include "core/math/transform_interpolator.h"
#include "core/os/os.h"
#include "core/templates/fixed_vector.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_server_default.h"

void gdgs_first_l88_render_pipeline_create_trace_arm(uint32_t p_pipeline_hash, RID p_shader_rid, RD::FramebufferFormatID p_framebuffer_format_id, RD::VertexFormatID p_vertex_format_id, RD::RenderPrimitive p_render_primitive, uint32_t p_render_pass, uint32_t p_specialization_constant_0, int p_batch_index, int p_match_ordinal);
void gdgs_first_l88_render_pipeline_create_trace_disarm();
void gdgs_first_l88_post_create_trace_disarm();
void gdgs_first_l88_execution_packet_trace_disarm();

namespace {
bool gdgs_debug_ui_pass_origin_enabled() {
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
	if (!OS::get_singleton()->has_environment("GODOT_GDGS_DEBUG_UI_PASS_ORIGIN")) {
		return false;
	}
	const String value = OS::get_singleton()->get_environment("GODOT_GDGS_DEBUG_UI_PASS_ORIGIN").strip_edges().to_lower();
	return !(value.is_empty() || value == "0" || value == "false" || value == "off" || value == "no");
#else
	return false;
#endif
}

const char *gdgs_canvas_blend_mode_name(int p_blend_mode) {
	switch (RendererRD::MaterialStorage::ShaderData::BlendMode(p_blend_mode)) {
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_MIX:
			return "mix";
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_ADD:
			return "add";
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_SUB:
			return "sub";
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_MUL:
			return "mul";
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_ALPHA_TO_COVERAGE:
			return "alpha_to_coverage";
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_PREMULTIPLIED_ALPHA:
			return "premul_alpha";
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_DISABLED:
			return "disabled";
		default:
			return "unknown";
	}
}

bool gdgs_canvas_blend_mode_uses_prior_color(int p_blend_mode) {
	switch (RendererRD::MaterialStorage::ShaderData::BlendMode(p_blend_mode)) {
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_MIX:
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_ADD:
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_SUB:
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_MUL:
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_ALPHA_TO_COVERAGE:
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_PREMULTIPLIED_ALPHA:
			return true;
		case RendererRD::MaterialStorage::ShaderData::BLEND_MODE_DISABLED:
		default:
			return false;
	}
}

const char *gdgs_canvas_blend_factor_name(RD::BlendFactor p_factor) {
	switch (p_factor) {
		case RD::BLEND_FACTOR_ZERO:
			return "zero";
		case RD::BLEND_FACTOR_ONE:
			return "one";
		case RD::BLEND_FACTOR_SRC_COLOR:
			return "src_color";
		case RD::BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
			return "one_minus_src_color";
		case RD::BLEND_FACTOR_DST_COLOR:
			return "dst_color";
		case RD::BLEND_FACTOR_ONE_MINUS_DST_COLOR:
			return "one_minus_dst_color";
		case RD::BLEND_FACTOR_SRC_ALPHA:
			return "src_alpha";
		case RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
			return "one_minus_src_alpha";
		case RD::BLEND_FACTOR_DST_ALPHA:
			return "dst_alpha";
		case RD::BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
			return "one_minus_dst_alpha";
		case RD::BLEND_FACTOR_CONSTANT_COLOR:
			return "constant_color";
		case RD::BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
			return "one_minus_constant_color";
		case RD::BLEND_FACTOR_CONSTANT_ALPHA:
			return "constant_alpha";
		case RD::BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
			return "one_minus_constant_alpha";
		case RD::BLEND_FACTOR_SRC_ALPHA_SATURATE:
			return "src_alpha_saturate";
		case RD::BLEND_FACTOR_SRC1_COLOR:
			return "src1_color";
		case RD::BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
			return "one_minus_src1_color";
		case RD::BLEND_FACTOR_SRC1_ALPHA:
			return "src1_alpha";
		case RD::BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
			return "one_minus_src1_alpha";
		default:
			return "unknown";
	}
}

const char *gdgs_canvas_blend_op_name(RD::BlendOperation p_op) {
	switch (p_op) {
		case RD::BLEND_OP_ADD:
			return "add";
		case RD::BLEND_OP_SUBTRACT:
			return "subtract";
		case RD::BLEND_OP_REVERSE_SUBTRACT:
			return "reverse_subtract";
		case RD::BLEND_OP_MINIMUM:
			return "minimum";
		case RD::BLEND_OP_MAXIMUM:
			return "maximum";
		default:
			return "unknown";
	}
}

void gdgs_canvas_compute_blend_recipe(RendererRD::MaterialStorage::ShaderData::BlendMode p_blend_mode, bool p_lcd_blend, bool p_force_src_color_premul_experiment, bool p_force_dst_factors_zero_experiment, bool p_force_src_alpha_zero_experiment, bool p_force_enable_blend_false_experiment, bool p_force_blend_ops_non_add_experiment, RD::PipelineColorBlendState::Attachment &r_attachment, uint32_t &r_dynamic_state_flags, const char *&r_recipe_branch, bool &r_src_color_premul_override_applied, bool &r_dst_factors_zero_override_applied, bool &r_src_alpha_zero_override_applied, bool &r_enable_blend_false_override_applied, bool &r_blend_ops_non_add_override_applied) {
	r_attachment = RD::PipelineColorBlendState::Attachment();
	r_dynamic_state_flags = 0;
	r_recipe_branch = p_lcd_blend ? "lcd_blend_override" : "shader_blend_mode_attachment";
	r_src_color_premul_override_applied = false;
	r_dst_factors_zero_override_applied = false;
	r_src_alpha_zero_override_applied = false;
	r_enable_blend_false_override_applied = false;
	r_blend_ops_non_add_override_applied = false;

	if (p_lcd_blend) {
		r_attachment.enable_blend = true;
		r_attachment.alpha_blend_op = RD::BLEND_OP_ADD;
		r_attachment.color_blend_op = RD::BLEND_OP_ADD;
		r_attachment.src_color_blend_factor = RD::BLEND_FACTOR_CONSTANT_COLOR;
		r_attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		r_attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_ONE;
		r_attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		r_dynamic_state_flags = RD::DYNAMIC_STATE_BLEND_CONSTANTS;
	} else {
		r_attachment = RendererRD::MaterialStorage::ShaderData::blend_mode_to_blend_attachment(p_blend_mode);
		const bool matches_mix_preserve_dst_shape = p_blend_mode == RendererRD::MaterialStorage::ShaderData::BLEND_MODE_MIX && r_attachment.enable_blend && r_attachment.color_blend_op == RD::BLEND_OP_ADD && r_attachment.alpha_blend_op == RD::BLEND_OP_ADD && r_attachment.src_color_blend_factor == RD::BLEND_FACTOR_SRC_ALPHA && r_attachment.dst_color_blend_factor == RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA && r_attachment.src_alpha_blend_factor == RD::BLEND_FACTOR_ONE && r_attachment.dst_alpha_blend_factor == RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		if (p_force_src_color_premul_experiment && matches_mix_preserve_dst_shape) {
			r_attachment.src_color_blend_factor = RD::BLEND_FACTOR_ONE;
			r_recipe_branch = "shader_blend_mode_attachment_src_color_premul_experiment";
			r_src_color_premul_override_applied = true;
		}

		const bool matches_src_one_preserve_dst_shape = p_blend_mode == RendererRD::MaterialStorage::ShaderData::BLEND_MODE_MIX && r_attachment.enable_blend && r_attachment.color_blend_op == RD::BLEND_OP_ADD && r_attachment.alpha_blend_op == RD::BLEND_OP_ADD && r_attachment.src_color_blend_factor == RD::BLEND_FACTOR_ONE && r_attachment.dst_color_blend_factor == RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA && r_attachment.src_alpha_blend_factor == RD::BLEND_FACTOR_ONE && r_attachment.dst_alpha_blend_factor == RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		if (p_force_dst_factors_zero_experiment) {
			if (!p_force_src_color_premul_experiment && matches_mix_preserve_dst_shape) {
				r_attachment.src_color_blend_factor = RD::BLEND_FACTOR_SRC_ALPHA;
				r_attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_recipe_branch = "shader_blend_mode_attachment_dst_factors_zero_experiment_src_preserved";
				r_dst_factors_zero_override_applied = true;
			} else if (matches_src_one_preserve_dst_shape) {
				r_attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_recipe_branch = "shader_blend_mode_attachment_dst_factors_zero_experiment_with_src_color_premul";
				r_dst_factors_zero_override_applied = true;
			}
		}

		const bool matches_src_one_dst_zero_shape = p_blend_mode == RendererRD::MaterialStorage::ShaderData::BLEND_MODE_MIX && r_attachment.enable_blend && r_attachment.color_blend_op == RD::BLEND_OP_ADD && r_attachment.alpha_blend_op == RD::BLEND_OP_ADD && r_attachment.src_color_blend_factor == RD::BLEND_FACTOR_ONE && r_attachment.dst_color_blend_factor == RD::BLEND_FACTOR_ZERO && r_attachment.src_alpha_blend_factor == RD::BLEND_FACTOR_ONE && r_attachment.dst_alpha_blend_factor == RD::BLEND_FACTOR_ZERO;
		if (p_force_src_alpha_zero_experiment) {
			if (matches_mix_preserve_dst_shape) {
				r_attachment.src_color_blend_factor = RD::BLEND_FACTOR_ONE;
				r_attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_recipe_branch = "shader_blend_mode_attachment_src_alpha_zero_experiment_on_demoted_blend_shape";
				r_src_alpha_zero_override_applied = true;
			} else if (matches_src_one_preserve_dst_shape) {
				r_attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_recipe_branch = "shader_blend_mode_attachment_src_alpha_zero_experiment_with_dst_factors_zero";
				r_src_alpha_zero_override_applied = true;
			} else if (matches_src_one_dst_zero_shape) {
				r_attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_recipe_branch = "shader_blend_mode_attachment_src_alpha_zero_experiment_src_alpha_only";
				r_src_alpha_zero_override_applied = true;
			}
		}

		const bool matches_src_one_dst_zero_src_alpha_zero_shape = p_blend_mode == RendererRD::MaterialStorage::ShaderData::BLEND_MODE_MIX && r_attachment.enable_blend && r_attachment.color_blend_op == RD::BLEND_OP_ADD && r_attachment.alpha_blend_op == RD::BLEND_OP_ADD && r_attachment.src_color_blend_factor == RD::BLEND_FACTOR_ONE && r_attachment.dst_color_blend_factor == RD::BLEND_FACTOR_ZERO && r_attachment.src_alpha_blend_factor == RD::BLEND_FACTOR_ZERO && r_attachment.dst_alpha_blend_factor == RD::BLEND_FACTOR_ZERO;
		if (p_force_enable_blend_false_experiment) {
			if (matches_mix_preserve_dst_shape) {
				r_attachment.src_color_blend_factor = RD::BLEND_FACTOR_ONE;
				r_attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_attachment.enable_blend = false;
				r_recipe_branch = "shader_blend_mode_attachment_enable_blend_false_experiment_from_mix_baseline";
				r_enable_blend_false_override_applied = true;
			} else if (matches_src_one_dst_zero_src_alpha_zero_shape) {
				r_attachment.enable_blend = false;
				r_recipe_branch = "shader_blend_mode_attachment_enable_blend_false_experiment_on_demoted_blend_shape";
				r_enable_blend_false_override_applied = true;
			}
		}

		const bool matches_enable_blend_false_demoted_additive_shape = p_blend_mode == RendererRD::MaterialStorage::ShaderData::BLEND_MODE_MIX && !r_attachment.enable_blend && r_attachment.color_blend_op == RD::BLEND_OP_ADD && r_attachment.alpha_blend_op == RD::BLEND_OP_ADD && r_attachment.src_color_blend_factor == RD::BLEND_FACTOR_ONE && r_attachment.dst_color_blend_factor == RD::BLEND_FACTOR_ZERO && r_attachment.src_alpha_blend_factor == RD::BLEND_FACTOR_ZERO && r_attachment.dst_alpha_blend_factor == RD::BLEND_FACTOR_ZERO;
		if (p_force_blend_ops_non_add_experiment) {
			if (matches_mix_preserve_dst_shape) {
				r_attachment.enable_blend = false;
				r_attachment.src_color_blend_factor = RD::BLEND_FACTOR_ONE;
				r_attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
				r_attachment.color_blend_op = RD::BLEND_OP_REVERSE_SUBTRACT;
				r_attachment.alpha_blend_op = RD::BLEND_OP_REVERSE_SUBTRACT;
				r_recipe_branch = "shader_blend_mode_attachment_blend_ops_non_add_experiment_from_mix_baseline";
				r_blend_ops_non_add_override_applied = true;
			} else if (matches_enable_blend_false_demoted_additive_shape) {
				r_attachment.color_blend_op = RD::BLEND_OP_REVERSE_SUBTRACT;
				r_attachment.alpha_blend_op = RD::BLEND_OP_REVERSE_SUBTRACT;
				r_recipe_branch = "shader_blend_mode_attachment_blend_ops_non_add_experiment_on_demoted_blend_shape";
				r_blend_ops_non_add_override_applied = true;
			}
		}
	}
}

bool gdgs_debug_env_bool_enabled(const char *p_name) {
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
	if (!OS::get_singleton()->has_environment(p_name)) {
		return false;
	}
	const String value = OS::get_singleton()->get_environment(p_name).strip_edges().to_lower();
	return !(value.is_empty() || value == "0" || value == "false" || value == "off" || value == "no");
#else
	return false;
#endif
}

int gdgs_debug_env_int_or(const char *p_name, int p_default) {
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
	if (!OS::get_singleton()->has_environment(p_name)) {
		return p_default;
	}
	return OS::get_singleton()->get_environment(p_name).strip_edges().to_int();
#else
	return p_default;
#endif
}

const char *gdgs_canvas_command_type_name(RendererCanvasRender::Item::Command::Type p_type) {
	switch (p_type) {
		case RendererCanvasRender::Item::Command::TYPE_RECT:
			return "rect";
		case RendererCanvasRender::Item::Command::TYPE_NINEPATCH:
			return "ninepatch";
		case RendererCanvasRender::Item::Command::TYPE_POLYGON:
			return "polygon";
		case RendererCanvasRender::Item::Command::TYPE_PRIMITIVE:
			return "primitive";
		case RendererCanvasRender::Item::Command::TYPE_MESH:
			return "mesh";
		case RendererCanvasRender::Item::Command::TYPE_MULTIMESH:
			return "multimesh";
		case RendererCanvasRender::Item::Command::TYPE_PARTICLES:
			return "particles";
		case RendererCanvasRender::Item::Command::TYPE_TRANSFORM:
			return "transform";
		case RendererCanvasRender::Item::Command::TYPE_CLIP_IGNORE:
			return "clip_ignore";
		case RendererCanvasRender::Item::Command::TYPE_ANIMATION_SLICE:
			return "animation_slice";
		default:
			return "unknown";
	}
}

const char *gdgs_canvas_shader_variant_name(int p_variant) {
	switch (p_variant) {
		case 0:
			return "quad";
		case 1:
			return "ninepatch";
		case 2:
			return "primitive";
		case 3:
			return "primitive_points";
		case 4:
			return "attributes";
		case 5:
			return "attributes_points";
		default:
			return "unknown";
	}
}

const char *gdgs_canvas_render_primitive_name(RD::RenderPrimitive p_primitive) {
	switch (p_primitive) {
		case RD::RENDER_PRIMITIVE_POINTS:
			return "points";
		case RD::RENDER_PRIMITIVE_LINES:
			return "lines";
		case RD::RENDER_PRIMITIVE_LINESTRIPS:
			return "line_strip";
		case RD::RENDER_PRIMITIVE_TRIANGLES:
			return "triangles";
		case RD::RENDER_PRIMITIVE_TRIANGLE_STRIPS:
			return "triangle_strip";
		default:
			return "unknown";
	}
}

String gdgs_rect2_to_string(const Rect2 &p_rect) {
	return vformat("{x=%f,y=%f,w=%f,h=%f}", p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y);
}

String gdgs_rid_to_string(RID p_rid) {
	return p_rid.is_valid() ? itos(p_rid.get_id()) : String("null");
}

String gdgs_u64_hex_string(uint64_t p_value) {
	return String("0x") + String::num_uint64(p_value, 16);
}

const char *gdgs_canvas_data_format_name(RD::DataFormat p_format) {
	switch (p_format) {
		case RD::DATA_FORMAT_R32G32B32A32_SFLOAT:
			return "R32G32B32A32_SFLOAT";
		case RD::DATA_FORMAT_R32G32B32A32_UINT:
			return "R32G32B32A32_UINT";
		default:
			return "other";
	}
}

const char *gdgs_canvas_vertex_frequency_name(RD::VertexFrequency p_frequency) {
	switch (p_frequency) {
		case RD::VERTEX_FREQUENCY_VERTEX:
			return "vertex";
		case RD::VERTEX_FREQUENCY_INSTANCE:
			return "instance";
		default:
			return "unknown";
	}
}

const char *gdgs_canvas_quad_vertex_attribute_semantic(uint32_t p_location) {
	switch (p_location) {
		case 8:
			return "world_x_world_y";
		case 9:
			return "world_ofs_ninepatch_pixel_size";
		case 10:
			return "modulation";
		case 11:
			return "ninepatch_margins";
		case 12:
			return "dst_rect";
		case 13:
			return "src_rect";
		case 14:
			return "flags_instance_uniforms_ofs";
		case 15:
			return "lights";
		default:
			return "unknown";
	}
}

thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_trace_active = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_combined_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_scissor_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_uniform_pipeline_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_draw_binding_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_uniform_bind_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_bind_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_blend_constants_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_selector_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_attachment_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_dynamic_state_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_src_color_premul_experiment_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_dst_factors_zero_experiment_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_src_alpha_zero_experiment_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_enable_blend_false_experiment_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_blend_ops_non_add_experiment_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_specialization_force_msdf_experiment_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_duplicate_format_experiment_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_vertex_bind_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_index_bind_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_binding_layout_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_attribute_layout_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_provenance_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_selector_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_packing_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_provenance_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_descriptor_shape_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_push_constant_shape_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_trace_enabled = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_trace_enabled = false;
thread_local const void *gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr = nullptr;
thread_local int gdgs_temp_diag_first_clipped_preserve_rect_batch_index = -1;
thread_local int gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal = -1;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_scissor_changed = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_scissor_enabled = false;
thread_local Rect2 gdgs_temp_diag_first_clipped_preserve_rect_scissor_rect;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_scissor_bucket_logged = false;
thread_local uint64_t gdgs_temp_diag_first_clipped_preserve_rect_base_uniform_set = 0;
thread_local uint64_t gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set = 0;
thread_local uint64_t gdgs_temp_diag_first_clipped_preserve_rect_transforms_uniform_set = 0;
thread_local uint64_t gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set = 0;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set_rebound = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_uniform_pipeline_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_draw_binding_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_uniform_bind_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_bind_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_blend_constants_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_selector_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_attachment_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_dynamic_state_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_vertex_bind_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_index_bind_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_binding_layout_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_attribute_layout_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_provenance_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_selector_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_packing_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_provenance_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_descriptor_shape_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_push_constant_shape_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_request_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_result_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_bind_bucket_logged = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_context_reached = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_shader_version_valid = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_shader_rid_valid = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_uses_default_shader = false;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_ubershader = false;
thread_local uint64_t gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_pipeline_hash = 0;

struct GdgsCanvasPipelineRealizationTraceTarget {
	Mutex mutex;
	bool armed = false;
	uint32_t pipeline_hash = 0;
	int batch_index = -1;
	int match_ordinal = -1;
	bool create_observed = false;
	RID created_pipeline;
};

GdgsCanvasPipelineRealizationTraceTarget gdgs_canvas_pipeline_realization_trace_target;

void gdgs_canvas_pipeline_realization_trace_arm(uint32_t p_pipeline_hash, int p_batch_index, int p_match_ordinal) {
	MutexLock lock(gdgs_canvas_pipeline_realization_trace_target.mutex);
	gdgs_canvas_pipeline_realization_trace_target.armed = true;
	gdgs_canvas_pipeline_realization_trace_target.pipeline_hash = p_pipeline_hash;
	gdgs_canvas_pipeline_realization_trace_target.batch_index = p_batch_index;
	gdgs_canvas_pipeline_realization_trace_target.match_ordinal = p_match_ordinal;
	gdgs_canvas_pipeline_realization_trace_target.create_observed = false;
	gdgs_canvas_pipeline_realization_trace_target.created_pipeline = RID();
}

void gdgs_canvas_pipeline_realization_trace_record_create(uint32_t p_pipeline_hash, RID p_created_pipeline) {
	MutexLock lock(gdgs_canvas_pipeline_realization_trace_target.mutex);
	if (!gdgs_canvas_pipeline_realization_trace_target.armed || gdgs_canvas_pipeline_realization_trace_target.pipeline_hash != p_pipeline_hash) {
		return;
	}
	gdgs_canvas_pipeline_realization_trace_target.create_observed = true;
	gdgs_canvas_pipeline_realization_trace_target.created_pipeline = p_created_pipeline;
}

void gdgs_canvas_pipeline_realization_trace_snapshot(uint32_t p_pipeline_hash, bool &r_target_armed, bool &r_create_observed, RID &r_created_pipeline) {
	MutexLock lock(gdgs_canvas_pipeline_realization_trace_target.mutex);
	r_target_armed = gdgs_canvas_pipeline_realization_trace_target.armed && gdgs_canvas_pipeline_realization_trace_target.pipeline_hash == p_pipeline_hash;
	r_create_observed = r_target_armed && gdgs_canvas_pipeline_realization_trace_target.create_observed;
	r_created_pipeline = r_create_observed ? gdgs_canvas_pipeline_realization_trace_target.created_pipeline : RID();
}

void gdgs_canvas_pipeline_realization_trace_disarm(uint32_t p_pipeline_hash) {
	MutexLock lock(gdgs_canvas_pipeline_realization_trace_target.mutex);
	if (!gdgs_canvas_pipeline_realization_trace_target.armed) {
		return;
	}
	if (p_pipeline_hash != 0 && gdgs_canvas_pipeline_realization_trace_target.pipeline_hash != p_pipeline_hash) {
		return;
	}
	gdgs_canvas_pipeline_realization_trace_target.armed = false;
	gdgs_canvas_pipeline_realization_trace_target.pipeline_hash = 0;
	gdgs_canvas_pipeline_realization_trace_target.batch_index = -1;
	gdgs_canvas_pipeline_realization_trace_target.match_ordinal = -1;
	gdgs_canvas_pipeline_realization_trace_target.create_observed = false;
	gdgs_canvas_pipeline_realization_trace_target.created_pipeline = RID();
}

thread_local int gdgs_temp_diag_first_clipped_preserve_rect_shader_blend_mode = RendererRD::MaterialStorage::ShaderData::BLEND_MODE_DISABLED;
thread_local bool gdgs_temp_diag_first_clipped_preserve_rect_prereq_logged = false;

} // namespace

void RendererCanvasRenderRD::_update_transform_2d_to_mat4(const Transform2D &p_transform, float *p_mat4) {
	p_mat4[0] = p_transform.columns[0][0];
	p_mat4[1] = p_transform.columns[0][1];
	p_mat4[2] = 0;
	p_mat4[3] = 0;
	p_mat4[4] = p_transform.columns[1][0];
	p_mat4[5] = p_transform.columns[1][1];
	p_mat4[6] = 0;
	p_mat4[7] = 0;
	p_mat4[8] = 0;
	p_mat4[9] = 0;
	p_mat4[10] = 1;
	p_mat4[11] = 0;
	p_mat4[12] = p_transform.columns[2][0];
	p_mat4[13] = p_transform.columns[2][1];
	p_mat4[14] = 0;
	p_mat4[15] = 1;
}

void RendererCanvasRenderRD::_update_transform_2d_to_mat2x4(const Transform2D &p_transform, float *p_mat2x4) {
	p_mat2x4[0] = p_transform.columns[0][0];
	p_mat2x4[1] = p_transform.columns[1][0];
	p_mat2x4[2] = 0;
	p_mat2x4[3] = p_transform.columns[2][0];

	p_mat2x4[4] = p_transform.columns[0][1];
	p_mat2x4[5] = p_transform.columns[1][1];
	p_mat2x4[6] = 0;
	p_mat2x4[7] = p_transform.columns[2][1];
}

void RendererCanvasRenderRD::_update_transform_2d_to_mat2x3(const Transform2D &p_transform, float *p_mat2x3) {
	p_mat2x3[0] = p_transform.columns[0][0];
	p_mat2x3[1] = p_transform.columns[0][1];
	p_mat2x3[2] = p_transform.columns[1][0];
	p_mat2x3[3] = p_transform.columns[1][1];
	p_mat2x3[4] = p_transform.columns[2][0];
	p_mat2x3[5] = p_transform.columns[2][1];
}

void RendererCanvasRenderRD::_update_transform_to_mat4(const Transform3D &p_transform, float *p_mat4) {
	p_mat4[0] = p_transform.basis.rows[0][0];
	p_mat4[1] = p_transform.basis.rows[1][0];
	p_mat4[2] = p_transform.basis.rows[2][0];
	p_mat4[3] = 0;
	p_mat4[4] = p_transform.basis.rows[0][1];
	p_mat4[5] = p_transform.basis.rows[1][1];
	p_mat4[6] = p_transform.basis.rows[2][1];
	p_mat4[7] = 0;
	p_mat4[8] = p_transform.basis.rows[0][2];
	p_mat4[9] = p_transform.basis.rows[1][2];
	p_mat4[10] = p_transform.basis.rows[2][2];
	p_mat4[11] = 0;
	p_mat4[12] = p_transform.origin.x;
	p_mat4[13] = p_transform.origin.y;
	p_mat4[14] = p_transform.origin.z;
	p_mat4[15] = 1;
}

RendererCanvasRender::PolygonID RendererCanvasRenderRD::request_polygon(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs, const Vector<int> &p_bones, const Vector<float> &p_weights, int p_count) {
	// Care must be taken to generate array formats
	// in ways where they could be reused, so we will
	// put single-occurring elements first, and repeated
	// elements later. This way the generated formats are
	// the same no matter the length of the arrays.
	// This dramatically reduces the amount of pipeline objects
	// that need to be created for these formats.

	RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();

	uint32_t vertex_count = p_points.size();
	uint32_t stride = 2; //vertices always repeat
	if ((uint32_t)p_colors.size() == vertex_count || p_colors.size() == 1) {
		stride += 4;
	}
	if ((uint32_t)p_uvs.size() == vertex_count) {
		stride += 2;
	}
	if ((uint32_t)p_bones.size() == vertex_count * 4 && (uint32_t)p_weights.size() == vertex_count * 4) {
		stride += 4;
	}

	uint32_t buffer_size = stride * p_points.size();

	Vector<uint8_t> polygon_buffer;
	polygon_buffer.resize(buffer_size * sizeof(float));
	Vector<RD::VertexAttribute> descriptions;
	descriptions.resize(5);
	Vector<RID> buffers;
	buffers.resize(5);

	{
		uint8_t *r = polygon_buffer.ptrw();
		float *fptr = reinterpret_cast<float *>(r);
		uint32_t *uptr = reinterpret_cast<uint32_t *>(r);
		uint32_t base_offset = 0;
		{ //vertices
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32_SFLOAT;
			vd.offset = base_offset * sizeof(float);
			vd.location = RSE::ARRAY_VERTEX;
			vd.stride = stride * sizeof(float);

			descriptions.write[0] = vd;

			const Vector2 *points_ptr = p_points.ptr();

			for (uint32_t i = 0; i < vertex_count; i++) {
				fptr[base_offset + i * stride + 0] = points_ptr[i].x;
				fptr[base_offset + i * stride + 1] = points_ptr[i].y;
			}

			base_offset += 2;
		}

		//colors
		if ((uint32_t)p_colors.size() == vertex_count || p_colors.size() == 1) {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
			vd.offset = base_offset * sizeof(float);
			vd.location = RSE::ARRAY_COLOR;
			vd.stride = stride * sizeof(float);

			descriptions.write[1] = vd;

			if (p_colors.size() == 1) {
				Color color = p_colors[0];
				for (uint32_t i = 0; i < vertex_count; i++) {
					fptr[base_offset + i * stride + 0] = color.r;
					fptr[base_offset + i * stride + 1] = color.g;
					fptr[base_offset + i * stride + 2] = color.b;
					fptr[base_offset + i * stride + 3] = color.a;
				}
			} else {
				const Color *color_ptr = p_colors.ptr();

				for (uint32_t i = 0; i < vertex_count; i++) {
					fptr[base_offset + i * stride + 0] = color_ptr[i].r;
					fptr[base_offset + i * stride + 1] = color_ptr[i].g;
					fptr[base_offset + i * stride + 2] = color_ptr[i].b;
					fptr[base_offset + i * stride + 3] = color_ptr[i].a;
				}
			}
			base_offset += 4;
		} else {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
			vd.offset = 0;
			vd.location = RSE::ARRAY_COLOR;
			vd.stride = 0;

			descriptions.write[1] = vd;
			buffers.write[1] = mesh_storage->mesh_get_default_rd_buffer(RendererRD::MeshStorage::DEFAULT_RD_BUFFER_COLOR);
		}

		//uvs
		if ((uint32_t)p_uvs.size() == vertex_count) {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32_SFLOAT;
			vd.offset = base_offset * sizeof(float);
			vd.location = RSE::ARRAY_TEX_UV;
			vd.stride = stride * sizeof(float);

			descriptions.write[2] = vd;

			const Vector2 *uv_ptr = p_uvs.ptr();

			for (uint32_t i = 0; i < vertex_count; i++) {
				fptr[base_offset + i * stride + 0] = uv_ptr[i].x;
				fptr[base_offset + i * stride + 1] = uv_ptr[i].y;
			}
			base_offset += 2;
		} else {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32_SFLOAT;
			vd.offset = 0;
			vd.location = RSE::ARRAY_TEX_UV;
			vd.stride = 0;

			descriptions.write[2] = vd;
			buffers.write[2] = mesh_storage->mesh_get_default_rd_buffer(RendererRD::MeshStorage::DEFAULT_RD_BUFFER_TEX_UV);
		}

		//bones
		if ((uint32_t)p_indices.size() == vertex_count * 4 && (uint32_t)p_weights.size() == vertex_count * 4) {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R16G16B16A16_UINT;
			vd.offset = base_offset * sizeof(float);
			vd.location = RSE::ARRAY_BONES;
			vd.stride = stride * sizeof(float);

			descriptions.write[3] = vd;

			const int *bone_ptr = p_bones.ptr();

			for (uint32_t i = 0; i < vertex_count; i++) {
				uint16_t *bone16w = (uint16_t *)&uptr[base_offset + i * stride];

				bone16w[0] = bone_ptr[i * 4 + 0];
				bone16w[1] = bone_ptr[i * 4 + 1];
				bone16w[2] = bone_ptr[i * 4 + 2];
				bone16w[3] = bone_ptr[i * 4 + 3];
			}

			base_offset += 2;
		} else {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32B32A32_UINT;
			vd.offset = 0;
			vd.location = RSE::ARRAY_BONES;
			vd.stride = 0;

			descriptions.write[3] = vd;
			buffers.write[3] = mesh_storage->mesh_get_default_rd_buffer(RendererRD::MeshStorage::DEFAULT_RD_BUFFER_BONES);
		}

		//weights
		if ((uint32_t)p_weights.size() == vertex_count * 4) {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R16G16B16A16_UNORM;
			vd.offset = base_offset * sizeof(float);
			vd.location = RSE::ARRAY_WEIGHTS;
			vd.stride = stride * sizeof(float);

			descriptions.write[4] = vd;

			const float *weight_ptr = p_weights.ptr();

			for (uint32_t i = 0; i < vertex_count; i++) {
				uint16_t *weight16w = (uint16_t *)&uptr[base_offset + i * stride];

				weight16w[0] = CLAMP(weight_ptr[i * 4 + 0] * 65535, 0, 65535);
				weight16w[1] = CLAMP(weight_ptr[i * 4 + 1] * 65535, 0, 65535);
				weight16w[2] = CLAMP(weight_ptr[i * 4 + 2] * 65535, 0, 65535);
				weight16w[3] = CLAMP(weight_ptr[i * 4 + 3] * 65535, 0, 65535);
			}

			base_offset += 2;
		} else {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
			vd.offset = 0;
			vd.location = RSE::ARRAY_WEIGHTS;
			vd.stride = 0;

			descriptions.write[4] = vd;
			buffers.write[4] = mesh_storage->mesh_get_default_rd_buffer(RendererRD::MeshStorage::DEFAULT_RD_BUFFER_WEIGHTS);
		}

		//check that everything is as it should be
		ERR_FAIL_COND_V(base_offset != stride, 0); //bug
	}

	RD::VertexFormatID vertex_id = RD::get_singleton()->vertex_format_create(descriptions);
	ERR_FAIL_COND_V(vertex_id == RD::INVALID_ID, 0);

	PolygonBuffers pb;
	pb.vertex_buffer = RD::get_singleton()->vertex_buffer_create(polygon_buffer.size(), polygon_buffer);
	for (int i = 0; i < descriptions.size(); i++) {
		if (buffers[i] == RID()) { //if put in vertex, use as vertex
			buffers.write[i] = pb.vertex_buffer;
		}
	}

	pb.vertex_array = RD::get_singleton()->vertex_array_create(p_points.size(), vertex_id, buffers);
	pb.primitive_count = vertex_count;

	if (p_indices.size()) {
		//create indices, as indices were requested
		Vector<uint8_t> index_buffer;
		index_buffer.resize(p_count * sizeof(int32_t));
		{
			uint8_t *w = index_buffer.ptrw();
			memcpy(w, p_indices.ptr(), sizeof(int32_t) * p_indices.size());
		}
		pb.index_buffer = RD::get_singleton()->index_buffer_create(p_count, RD::INDEX_BUFFER_FORMAT_UINT32, index_buffer);
		pb.indices = RD::get_singleton()->index_array_create(pb.index_buffer, 0, p_count);
		pb.primitive_count = p_count;
	}

	pb.vertex_format_id = vertex_id;

	PolygonID id = polygon_buffers.last_id++;

	polygon_buffers.polygons[id] = pb;

	return id;
}

void RendererCanvasRenderRD::free_polygon(PolygonID p_polygon) {
	PolygonBuffers *pb_ptr = polygon_buffers.polygons.getptr(p_polygon);
	ERR_FAIL_NULL(pb_ptr);

	PolygonBuffers &pb = *pb_ptr;

	if (pb.indices.is_valid()) {
		RD::get_singleton()->free_rid(pb.indices);
	}
	if (pb.index_buffer.is_valid()) {
		RD::get_singleton()->free_rid(pb.index_buffer);
	}

	RD::get_singleton()->free_rid(pb.vertex_array);
	RD::get_singleton()->free_rid(pb.vertex_buffer);

	polygon_buffers.polygons.erase(p_polygon);
}

////////////////////

static RD::RenderPrimitive _primitive_type_to_render_primitive(RSE::PrimitiveType p_primitive) {
	switch (p_primitive) {
		case RSE::PRIMITIVE_POINTS:
			return RD::RENDER_PRIMITIVE_POINTS;
		case RSE::PRIMITIVE_LINES:
			return RD::RENDER_PRIMITIVE_LINES;
		case RSE::PRIMITIVE_LINE_STRIP:
			return RD::RENDER_PRIMITIVE_LINESTRIPS;
		case RSE::PRIMITIVE_TRIANGLES:
			return RD::RENDER_PRIMITIVE_TRIANGLES;
		case RSE::PRIMITIVE_TRIANGLE_STRIP:
			return RD::RENDER_PRIMITIVE_TRIANGLE_STRIPS;
		default:
			return RD::RENDER_PRIMITIVE_MAX;
	}
}

_FORCE_INLINE_ static uint32_t _indices_to_primitives(RSE::PrimitiveType p_primitive, uint32_t p_indices) {
	static const uint32_t divisor[RSE::PRIMITIVE_MAX] = { 1, 2, 1, 3, 1 };
	static const uint32_t subtractor[RSE::PRIMITIVE_MAX] = { 0, 0, 1, 0, 2 };
	return (p_indices - subtractor[p_primitive]) / divisor[p_primitive];
}

RID RendererCanvasRenderRD::_create_base_uniform_set(RID p_to_render_target, bool p_backbuffer) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

	//re create canvas state
	thread_local LocalVector<RD::Uniform> uniforms;
	uniforms.clear();

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
		u.binding = 1;
		u.append_id(state.canvas_state_buffer);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
		u.binding = 2;
		u.append_id(state.lights_storage_buffer);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 3;
		u.append_id(RendererRD::TextureStorage::get_singleton()->decal_atlas_get_texture());
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 4;
		u.append_id(state.shadow_texture);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
		u.binding = 5;
		u.append_id(state.shadow_sampler);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 6;
		RID screen;
		if (p_backbuffer) {
			screen = texture_storage->render_target_get_rd_texture(p_to_render_target);
		} else {
			screen = texture_storage->render_target_get_rd_backbuffer(p_to_render_target);
			if (screen.is_null()) { //unallocated backbuffer
				screen = RendererRD::TextureStorage::get_singleton()->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_WHITE);
			}
		}
		u.append_id(screen);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 7;
		RID sdf = texture_storage->render_target_get_sdf_texture(p_to_render_target);
		u.append_id(sdf);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
		u.binding = 9;
		u.append_id(RendererRD::MaterialStorage::get_singleton()->global_shader_uniforms_get_storage_buffer());
		uniforms.push_back(u);
	}

	material_storage->samplers_rd_get_default().append_uniforms(uniforms, SAMPLERS_BINDING_FIRST_INDEX);

	RID uniform_set = RD::get_singleton()->uniform_set_create(uniforms, shader.default_version_rd_shader, BASE_UNIFORM_SET);
	if (p_backbuffer) {
		texture_storage->render_target_set_backbuffer_uniform_set(p_to_render_target, uniform_set);
	} else {
		texture_storage->render_target_set_framebuffer_uniform_set(p_to_render_target, uniform_set);
	}

	return uniform_set;
}

RID RendererCanvasRenderRD::_get_pipeline_specialization_or_ubershader(CanvasShaderData *p_shader_data, PipelineKey &r_pipeline_key, PushConstant &r_push_constant, RID p_mesh_instance, void *p_surface, uint32_t p_surface_index, RID *r_vertex_array) {
	r_pipeline_key.ubershader = 0;

	const uint32_t ubershader_iterations = 1;
	while (r_pipeline_key.ubershader < ubershader_iterations) {
		if (r_vertex_array != nullptr) {
			RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();
			uint64_t input_mask = p_shader_data->get_vertex_input_mask(r_pipeline_key.variant, r_pipeline_key.ubershader);
			if (p_mesh_instance.is_valid()) {
				mesh_storage->mesh_instance_surface_get_vertex_arrays_and_format(p_mesh_instance, p_surface_index, input_mask, false, false, *r_vertex_array, r_pipeline_key.vertex_format_id);
			} else {
				mesh_storage->mesh_surface_get_vertex_arrays_and_format(p_surface, input_mask, false, false, *r_vertex_array, r_pipeline_key.vertex_format_id);
			}
		}

		if (r_pipeline_key.ubershader) {
			r_push_constant.shader_specialization = r_pipeline_key.shader_specialization;
			r_pipeline_key.shader_specialization = {};
		} else {
			r_push_constant.shader_specialization = {};
		}

		bool wait_for_compilation = r_pipeline_key.ubershader || ubershader_iterations == 1;
		RSE::PipelineSource source = RSE::PIPELINE_SOURCE_CANVAS;
		RID pipeline = p_shader_data->pipeline_hash_map.get_pipeline(r_pipeline_key, r_pipeline_key.hash(), wait_for_compilation, source);
		if (pipeline.is_valid()) {
			return pipeline;
		}

		r_pipeline_key.ubershader++;
	}

	// This case should never be reached unless the shader wasn't available.
	return RID();
}

void RendererCanvasRenderRD::canvas_render_items(RID p_to_render_target, Item *p_item_list, const Color &p_modulate, Light *p_light_list, Light *p_directional_light_list, const Transform2D &p_canvas_transform, RSE::CanvasItemTextureFilter p_default_filter, RSE::CanvasItemTextureRepeat p_default_repeat, bool p_snap_2d_vertices_to_pixel, bool &r_sdf_used, RenderingServerTypes::RenderInfo *r_render_info) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();

	r_sdf_used = false;
	int item_count = 0;

	//setup canvas state uniforms if needed

	Transform2D canvas_transform_inverse = p_canvas_transform.affine_inverse();

	//setup directional lights if exist

	uint32_t light_count = 0;
	uint32_t directional_light_count = 0;
	{
		Light *l = p_directional_light_list;
		uint32_t index = 0;

		while (l) {
			if (index == MAX_LIGHTS_PER_RENDER) {
				l->render_index_cache = -1;
				l = l->next_ptr;
				continue;
			}

			CanvasLight *clight = canvas_light_owner.get_or_null(l->light_internal);
			if (!clight) { //unused or invalid texture
				l->render_index_cache = -1;
				l = l->next_ptr;
				ERR_CONTINUE(!clight);
			}

			Vector2 canvas_light_dir = l->xform_cache.columns[1].normalized();

			state.light_uniforms[index].position[0] = -canvas_light_dir.x;
			state.light_uniforms[index].position[1] = -canvas_light_dir.y;

			_update_transform_2d_to_mat2x4(clight->shadow.directional_xform, state.light_uniforms[index].shadow_matrix);

			state.light_uniforms[index].height = l->height; //0..1 here

			for (int i = 0; i < 4; i++) {
				state.light_uniforms[index].shadow_color[i] = uint8_t(CLAMP(int32_t(l->shadow_color[i] * 255.0), 0, 255));
				state.light_uniforms[index].color[i] = l->color[i];
			}

			state.light_uniforms[index].color[3] *= l->energy; //use alpha for energy, so base color can go separate

			if (state.shadow_fb.is_valid()) {
				state.light_uniforms[index].shadow_pixel_size = (1.0 / state.shadow_texture_size) * (1.0 + l->shadow_smooth);
				state.light_uniforms[index].shadow_z_far_inv = 1.0 / clight->shadow.z_far;
				state.light_uniforms[index].shadow_y_ofs = clight->shadow.y_offset;
			} else {
				state.light_uniforms[index].shadow_pixel_size = 1.0;
				state.light_uniforms[index].shadow_z_far_inv = 1.0;
				state.light_uniforms[index].shadow_y_ofs = 0;
			}

			state.light_uniforms[index].flags = l->blend_mode << LIGHT_FLAGS_BLEND_SHIFT;
			state.light_uniforms[index].flags |= l->shadow_filter << LIGHT_FLAGS_FILTER_SHIFT;
			if (clight->shadow.enabled) {
				state.light_uniforms[index].flags |= LIGHT_FLAGS_HAS_SHADOW;
			}

			l->render_index_cache = index;

			index++;
			l = l->next_ptr;
		}

		light_count = index;
		directional_light_count = light_count;
		using_directional_lights = directional_light_count > 0;
	}

	//setup lights if exist

	{
		Light *l = p_light_list;
		uint32_t index = light_count;

		while (l) {
			if (index == MAX_LIGHTS_PER_RENDER) {
				l->render_index_cache = -1;
				l = l->next_ptr;
				continue;
			}

			CanvasLight *clight = canvas_light_owner.get_or_null(l->light_internal);
			if (!clight) { //unused or invalid texture
				l->render_index_cache = -1;
				l = l->next_ptr;
				ERR_CONTINUE(!clight);
			}

			Transform2D final_xform;
			if (!RSG::canvas->_interpolation_data.interpolation_enabled || !l->interpolated || !l->on_interpolate_transform_list) {
				final_xform = l->xform_curr;
			} else {
				real_t f = Engine::get_singleton()->get_physics_interpolation_fraction();
				TransformInterpolator::interpolate_transform_2d(l->xform_prev, l->xform_curr, final_xform, f);
			}
			// Convert light position to canvas coordinates, as all computation is done in canvas coordinates to avoid precision loss.
			Vector2 canvas_light_pos = p_canvas_transform.xform(final_xform.get_origin());
			state.light_uniforms[index].position[0] = canvas_light_pos.x;
			state.light_uniforms[index].position[1] = canvas_light_pos.y;

			_update_transform_2d_to_mat2x4(l->light_shader_xform.affine_inverse(), state.light_uniforms[index].matrix);
			_update_transform_2d_to_mat2x4(l->xform_cache.affine_inverse(), state.light_uniforms[index].shadow_matrix);

			state.light_uniforms[index].height = l->height * (p_canvas_transform.columns[0].length() + p_canvas_transform.columns[1].length()) * 0.5; //approximate height conversion to the canvas size, since all calculations are done in canvas coords to avoid precision loss
			for (int i = 0; i < 4; i++) {
				state.light_uniforms[index].shadow_color[i] = uint8_t(CLAMP(int32_t(l->shadow_color[i] * 255.0), 0, 255));
				state.light_uniforms[index].color[i] = l->color[i];
			}

			state.light_uniforms[index].color[3] *= l->energy; //use alpha for energy, so base color can go separate

			if (state.shadow_fb.is_valid()) {
				state.light_uniforms[index].shadow_pixel_size = (1.0 / state.shadow_texture_size) * (1.0 + l->shadow_smooth);
				state.light_uniforms[index].shadow_z_far_inv = 1.0 / clight->shadow.z_far;
				state.light_uniforms[index].shadow_y_ofs = clight->shadow.y_offset;
			} else {
				state.light_uniforms[index].shadow_pixel_size = 1.0;
				state.light_uniforms[index].shadow_z_far_inv = 1.0;
				state.light_uniforms[index].shadow_y_ofs = 0;
			}

			state.light_uniforms[index].flags = l->blend_mode << LIGHT_FLAGS_BLEND_SHIFT;
			state.light_uniforms[index].flags |= l->shadow_filter << LIGHT_FLAGS_FILTER_SHIFT;
			if (clight->shadow.enabled) {
				state.light_uniforms[index].flags |= LIGHT_FLAGS_HAS_SHADOW;
			}

			if (clight->texture.is_valid()) {
				Rect2 atlas_rect = RendererRD::TextureStorage::get_singleton()->decal_atlas_get_texture_rect(clight->texture);
				state.light_uniforms[index].atlas_rect[0] = atlas_rect.position.x;
				state.light_uniforms[index].atlas_rect[1] = atlas_rect.position.y;
				state.light_uniforms[index].atlas_rect[2] = atlas_rect.size.width;
				state.light_uniforms[index].atlas_rect[3] = atlas_rect.size.height;

			} else {
				state.light_uniforms[index].atlas_rect[0] = 0;
				state.light_uniforms[index].atlas_rect[1] = 0;
				state.light_uniforms[index].atlas_rect[2] = 0;
				state.light_uniforms[index].atlas_rect[3] = 0;
			}

			l->render_index_cache = index;

			index++;
			l = l->next_ptr;
		}

		light_count = index;
	}

	if (light_count > 0) {
		RD::get_singleton()->buffer_update(state.lights_storage_buffer, 0, sizeof(LightUniform) * light_count, &state.light_uniforms[0]);
	}

	bool use_linear_colors = texture_storage->render_target_is_using_hdr(p_to_render_target);

	{
		//update canvas state uniform buffer
		State::Buffer state_buffer;

		Size2i ssize = texture_storage->render_target_get_size(p_to_render_target);

		Transform3D screen_transform;
		screen_transform.translate_local(-(ssize.width / 2.0f), -(ssize.height / 2.0f), 0.0f);
		screen_transform.scale(Vector3(2.0f / ssize.width, 2.0f / ssize.height, 1.0f));
		_update_transform_to_mat4(screen_transform, state_buffer.screen_transform);
		_update_transform_2d_to_mat4(p_canvas_transform, state_buffer.canvas_transform);

		Transform2D normal_transform = p_canvas_transform;
		normal_transform.columns[0].normalize();
		normal_transform.columns[1].normalize();
		normal_transform.columns[2] = Vector2();
		_update_transform_2d_to_mat4(normal_transform, state_buffer.canvas_normal_transform);

		Color modulate = p_modulate;
		if (use_linear_colors) {
			modulate = p_modulate.srgb_to_linear();
		}
		state_buffer.canvas_modulate[0] = modulate.r;
		state_buffer.canvas_modulate[1] = modulate.g;
		state_buffer.canvas_modulate[2] = modulate.b;
		state_buffer.canvas_modulate[3] = modulate.a;

		Size2 render_target_size = texture_storage->render_target_get_size(p_to_render_target);
		state_buffer.screen_pixel_size[0] = 1.0 / render_target_size.x;
		state_buffer.screen_pixel_size[1] = 1.0 / render_target_size.y;

		state_buffer.time = state.time;
		state_buffer.use_pixel_snap = p_snap_2d_vertices_to_pixel;

		state_buffer.directional_light_count = directional_light_count;

		Vector2 canvas_scale = p_canvas_transform.get_scale();

		state_buffer.sdf_to_screen[0] = render_target_size.width / canvas_scale.x;
		state_buffer.sdf_to_screen[1] = render_target_size.height / canvas_scale.y;

		state_buffer.screen_to_sdf[0] = 1.0 / state_buffer.sdf_to_screen[0];
		state_buffer.screen_to_sdf[1] = 1.0 / state_buffer.sdf_to_screen[1];

		Rect2 sdf_rect = texture_storage->render_target_get_sdf_rect(p_to_render_target);
		Rect2 sdf_tex_rect(sdf_rect.position / canvas_scale, sdf_rect.size / canvas_scale);

		state_buffer.sdf_to_tex[0] = 1.0 / sdf_tex_rect.size.width;
		state_buffer.sdf_to_tex[1] = 1.0 / sdf_tex_rect.size.height;
		state_buffer.sdf_to_tex[2] = -sdf_tex_rect.position.x / sdf_tex_rect.size.width;
		state_buffer.sdf_to_tex[3] = -sdf_tex_rect.position.y / sdf_tex_rect.size.height;

		//print_line("w: " + itos(ssize.width) + " s: " + rtos(canvas_scale));
		state_buffer.tex_to_sdf = 1.0 / ((canvas_scale.x + canvas_scale.y) * 0.5);
		state_buffer.shadow_pixel_size = 1.0f / (float)(state.shadow_texture_size);

		state_buffer.flags = use_linear_colors ? CANVAS_FLAGS_CONVERT_ATTRIBUTES_TO_LINEAR : 0;

		RD::get_singleton()->buffer_update(state.canvas_state_buffer, 0, sizeof(State::Buffer), &state_buffer);
	}

	{ //default filter/repeat
		default_filter = p_default_filter;
		default_repeat = p_default_repeat;
	}

	Item *ci = p_item_list;

	//fill the list until rendering is possible.
	bool material_screen_texture_cached = false;
	bool material_screen_texture_mipmaps_cached = false;

	Rect2 back_buffer_rect;
	bool backbuffer_copy = false;
	bool backbuffer_gen_mipmaps = false;

	Item *canvas_group_owner = nullptr;
	bool skip_item = false;

	bool update_skeletons = false;
	bool time_used = false;

	bool backbuffer_cleared = false;

	RenderTarget to_render_target;
	to_render_target.render_target = p_to_render_target;
	to_render_target.use_linear_colors = use_linear_colors;

	while (ci) {
		if (ci->copy_back_buffer && canvas_group_owner == nullptr) {
			backbuffer_copy = true;

			if (ci->copy_back_buffer->full) {
				back_buffer_rect = Rect2();
			} else {
				back_buffer_rect = ci->copy_back_buffer->rect;
			}
		}

		RID material = ci->material_owner == nullptr ? ci->material : ci->material_owner->material;

		if (material.is_valid()) {
			CanvasMaterialData *md = static_cast<CanvasMaterialData *>(material_storage->material_get_data(material, RendererRD::MaterialStorage::SHADER_TYPE_2D));
			if (md && md->shader_data->is_valid()) {
				if (md->shader_data->uses_screen_texture && canvas_group_owner == nullptr) {
					if (!material_screen_texture_cached) {
						backbuffer_copy = true;
						back_buffer_rect = Rect2();
						backbuffer_gen_mipmaps = md->shader_data->uses_screen_texture_mipmaps;
					} else if (!material_screen_texture_mipmaps_cached) {
						backbuffer_gen_mipmaps = md->shader_data->uses_screen_texture_mipmaps;
					}
				}

				if (md->shader_data->uses_sdf) {
					r_sdf_used = true;
				}
				if (md->shader_data->uses_time) {
					time_used = true;
				}
			}
		}

		if (ci->skeleton.is_valid()) {
			const Item::Command *c = ci->commands;

			while (c) {
				if (c->type == Item::Command::TYPE_MESH) {
					const Item::CommandMesh *cm = static_cast<const Item::CommandMesh *>(c);
					if (cm->mesh_instance.is_valid()) {
						mesh_storage->mesh_instance_check_for_update(cm->mesh_instance);
						mesh_storage->mesh_instance_set_canvas_item_transform(cm->mesh_instance, canvas_transform_inverse * ci->final_transform);
						update_skeletons = true;
					}
				}
				c = c->next;
			}
		}

		if (ci->canvas_group_owner != nullptr) {
			if (canvas_group_owner == nullptr) {
				// Canvas group begins here, render until before this item
				if (update_skeletons) {
					mesh_storage->update_mesh_instances();
					update_skeletons = false;
				}
				_render_batch_items(to_render_target, item_count, canvas_transform_inverse, p_light_list, r_sdf_used, false, r_render_info);
				item_count = 0;

				if (ci->canvas_group_owner->canvas_group->mode != RSE::CANVAS_GROUP_MODE_TRANSPARENT) {
					Rect2i group_rect = ci->canvas_group_owner->global_rect_cache;
					texture_storage->render_target_copy_to_back_buffer(p_to_render_target, group_rect, false);
					if (ci->canvas_group_owner->canvas_group->mode == RSE::CANVAS_GROUP_MODE_CLIP_AND_DRAW) {
						ci->canvas_group_owner->use_canvas_group = false;
						items[item_count++] = ci->canvas_group_owner;
					}
				} else if (!backbuffer_cleared) {
					texture_storage->render_target_clear_back_buffer(p_to_render_target, Rect2i(), Color(0, 0, 0, 0));
					backbuffer_cleared = true;
				}

				backbuffer_copy = false;
				canvas_group_owner = ci->canvas_group_owner; //continue until owner found
			}

			ci->canvas_group_owner = nullptr; //must be cleared
		}

		if (canvas_group_owner == nullptr && ci->canvas_group != nullptr && ci->canvas_group->mode != RSE::CANVAS_GROUP_MODE_CLIP_AND_DRAW) {
			skip_item = true;
		}

		if (ci == canvas_group_owner) {
			if (update_skeletons) {
				mesh_storage->update_mesh_instances();
				update_skeletons = false;
			}

			_render_batch_items(to_render_target, item_count, canvas_transform_inverse, p_light_list, r_sdf_used, true, r_render_info);
			item_count = 0;

			if (ci->canvas_group->blur_mipmaps) {
				texture_storage->render_target_gen_back_buffer_mipmaps(p_to_render_target, ci->global_rect_cache);
			}

			canvas_group_owner = nullptr;
			// Backbuffer is dirty now and needs to be re-cleared if another CanvasGroup needs it.
			backbuffer_cleared = false;

			// Tell the renderer to paint this as a canvas group
			ci->use_canvas_group = true;
		} else {
			ci->use_canvas_group = false;
		}

		if (backbuffer_copy) {
			//render anything pending, including clearing if no items
			if (update_skeletons) {
				mesh_storage->update_mesh_instances();
				update_skeletons = false;
			}

			_render_batch_items(to_render_target, item_count, canvas_transform_inverse, p_light_list, r_sdf_used, false, r_render_info);
			item_count = 0;

			texture_storage->render_target_copy_to_back_buffer(p_to_render_target, back_buffer_rect, backbuffer_gen_mipmaps);

			backbuffer_copy = false;
			material_screen_texture_cached = true; // After a backbuffer copy, screen texture makes no further copies.
			material_screen_texture_mipmaps_cached = backbuffer_gen_mipmaps;
			backbuffer_gen_mipmaps = false;
		}

		if (backbuffer_gen_mipmaps) {
			texture_storage->render_target_gen_back_buffer_mipmaps(p_to_render_target, back_buffer_rect);

			backbuffer_gen_mipmaps = false;
			material_screen_texture_mipmaps_cached = true;
		}

		if (skip_item) {
			skip_item = false;
		} else {
			items[item_count++] = ci;
		}

		if (!ci->next || item_count == MAX_RENDER_ITEMS - 1) {
			if (update_skeletons) {
				mesh_storage->update_mesh_instances();
				update_skeletons = false;
			}

			_render_batch_items(to_render_target, item_count, canvas_transform_inverse, p_light_list, r_sdf_used, canvas_group_owner != nullptr, r_render_info);
			//then reset
			item_count = 0;
		}

		ci = ci->next;
	}

	if (time_used) {
		RenderingServerDefault::redraw_request();
	}

	texture_info_map.clear();

	// Save the previous instance data pointer in case more items are rendered in the same frame.
	state.prev_instance_data = state.instance_data;
	state.prev_instance_data_index = state.instance_data_index;

	state.instance_data = nullptr;
	if (state.instance_data_index > 0) {
		// If there was any remaining instance data, it must be flushed.
		RID buf = state.instance_buffers._get(0);
		RD::get_singleton()->buffer_flush(buf);
		state.instance_data_index = 0;
	}
}

RID RendererCanvasRenderRD::light_create() {
	CanvasLight canvas_light;
	return canvas_light_owner.make_rid(canvas_light);
}

void RendererCanvasRenderRD::light_set_texture(RID p_rid, RID p_texture) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	CanvasLight *cl = canvas_light_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(cl);
	if (cl->texture == p_texture) {
		return;
	}

	ERR_FAIL_COND(p_texture.is_valid() && !texture_storage->owns_texture(p_texture));

	if (cl->texture.is_valid()) {
		texture_storage->texture_remove_from_decal_atlas(cl->texture);
	}
	cl->texture = p_texture;

	if (cl->texture.is_valid()) {
		texture_storage->texture_add_to_decal_atlas(cl->texture);
	}
}

void RendererCanvasRenderRD::light_set_use_shadow(RID p_rid, bool p_enable) {
	CanvasLight *cl = canvas_light_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(cl);

	cl->shadow.enabled = p_enable;
}

void RendererCanvasRenderRD::_update_shadow_atlas() {
	if (state.shadow_fb == RID()) {
		//ah, we lack the shadow texture..
		RD::get_singleton()->free_rid(state.shadow_texture); //erase placeholder

		Vector<RID> fb_textures;

		{ //texture
			RD::TextureFormat tf;
			tf.texture_type = RD::TEXTURE_TYPE_2D;
			tf.width = state.shadow_texture_size;
			tf.height = MAX_LIGHTS_PER_RENDER * 2;
			tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
			tf.format = RD::DATA_FORMAT_R32_SFLOAT;

			state.shadow_texture = RD::get_singleton()->texture_create(tf, RD::TextureView());
			fb_textures.push_back(state.shadow_texture);
		}
		{
			RD::TextureFormat tf;
			tf.texture_type = RD::TEXTURE_TYPE_2D;
			tf.width = state.shadow_texture_size;
			tf.height = MAX_LIGHTS_PER_RENDER * 2;
			tf.usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			tf.format = RD::DATA_FORMAT_D32_SFLOAT;
			tf.is_discardable = true;
			//chunks to write
			state.shadow_depth_texture = RD::get_singleton()->texture_create(tf, RD::TextureView());
			fb_textures.push_back(state.shadow_depth_texture);
		}

		state.shadow_fb = RD::get_singleton()->framebuffer_create(fb_textures);
	}
}

void RendererCanvasRenderRD::_update_occluder_buffer(uint32_t p_size) {
	bool needs_update = state.shadow_occluder_buffer.is_null();

	if (p_size > state.shadow_occluder_buffer_size) {
		needs_update = true;
		state.shadow_occluder_buffer_size = Math::next_power_of_2(p_size);
		if (state.shadow_occluder_buffer.is_valid()) {
			RD::get_singleton()->free_rid(state.shadow_occluder_buffer);
		}
	}

	if (needs_update) {
		state.shadow_occluder_buffer = RD::get_singleton()->storage_buffer_create(state.shadow_occluder_buffer_size);

		Vector<RD::Uniform> uniforms;

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 0;
			u.append_id(state.shadow_occluder_buffer);
			uniforms.push_back(u);
		}
		state.shadow_ocluder_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, shadow_render.shader.version_get_shader(shadow_render.shader_version, SHADOW_RENDER_MODE_POSITIONAL_SHADOW), 0);
	}
}

void RendererCanvasRenderRD::light_update_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders, const Rect2 &p_light_rect) {
	CanvasLight *cl = canvas_light_owner.get_or_null(p_rid);
	ERR_FAIL_COND(!cl->shadow.enabled);

	_update_shadow_atlas();

	cl->shadow.z_far = p_far;
	cl->shadow.y_offset = float(p_shadow_index * 2 + 1) / float(MAX_LIGHTS_PER_RENDER * 2);
	Color cc = Color(p_far, p_far, p_far, 1.0);

	// First, do a culling pass and record what occluders need to be drawn for this light.
	static thread_local LocalVector<OccluderPolygon *> occluders;
	static thread_local LocalVector<uint32_t> occluder_indices;
	occluders.clear();
	occluder_indices.clear();

	uint32_t occluder_count = 0;

	LightOccluderInstance *instance = p_occluders;
	while (instance) {
		OccluderPolygon *co = occluder_polygon_owner.get_or_null(instance->occluder);

		occluder_count++;

		if (!co || co->index_array.is_null()) {
			instance = instance->next;
			continue;
		}

		if (!(p_light_mask & instance->light_mask) || !p_light_rect.intersects_transformed(instance->xform_cache, instance->aabb_cache)) {
			instance = instance->next;
			continue;
		}

		occluders.push_back(co);
		occluder_indices.push_back(occluder_count - 1);

		instance = instance->next;
	}

	// Then, upload all the occluder transforms to a shared buffer.
	// We only do this for the first light so we can avoid uploading the same
	// Transforms over and over again.
	if (p_shadow_index == 0 && occluder_count > 0) {
		static thread_local LocalVector<float> transforms;
		transforms.clear();
		transforms.resize(occluder_count * 8);

		instance = p_occluders;
		uint32_t index = 0;
		while (instance) {
			_update_transform_2d_to_mat2x4(instance->xform_cache, &transforms[index * 8]);
			index++;
			instance = instance->next;
		}

		_update_occluder_buffer(occluder_count * 8 * sizeof(float));
		RD::get_singleton()->buffer_update(state.shadow_occluder_buffer, 0, transforms.size() * sizeof(float), transforms.ptr());
	}

	Rect2i rect(0, p_shadow_index * 2, state.shadow_texture_size, 2);
	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(state.shadow_fb, RD::DRAW_CLEAR_ALL, VectorView(&cc, 1), 1.0f, 0, rect);

	if (state.shadow_occluder_buffer.is_valid()) {
		RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, shadow_render.render_pipelines[SHADOW_RENDER_MODE_POSITIONAL_SHADOW]);
		RD::get_singleton()->draw_list_bind_uniform_set(draw_list, state.shadow_ocluder_uniform_set, 0);

		for (int i = 0; i < 4; i++) {
			Rect2i sub_rect((state.shadow_texture_size / 4) * i, p_shadow_index * 2, (state.shadow_texture_size / 4), 2);
			RD::get_singleton()->draw_list_set_viewport(draw_list, sub_rect);

			static const Vector2 directions[4] = { Vector2(1, 0), Vector2(0, 1), Vector2(-1, 0), Vector2(0, -1) };
			static const Vector4 rotations[4] = { Vector4(0, -1, 1, 0), Vector4(-1, 0, 0, -1), Vector4(0, 1, -1, 0), Vector4(1, 0, 0, 1) };

			PositionalShadowRenderPushConstant push_constant;
			_update_transform_2d_to_mat2x4(p_light_xform, push_constant.modelview);
			push_constant.direction[0] = directions[i].x;
			push_constant.direction[1] = directions[i].y;
			push_constant.rotation[0] = rotations[i].x;
			push_constant.rotation[1] = rotations[i].y;
			push_constant.rotation[2] = rotations[i].z;
			push_constant.rotation[3] = rotations[i].w;
			push_constant.z_far = p_far;
			push_constant.z_near = p_near;

			for (uint32_t j = 0; j < occluders.size(); j++) {
				OccluderPolygon *co = occluders[j];

				push_constant.pad = occluder_indices[j];
				push_constant.cull_mode = uint32_t(co->cull_mode);

				// The slowest part about this whole function is that we have to draw the occluders one by one, 4 times.
				// We can optimize this so that all occluders draw at once if we store vertices and indices in a giant
				// SSBO and just save an index into that SSBO for each occluder.
				RD::get_singleton()->draw_list_bind_vertex_array(draw_list, co->vertex_array);
				RD::get_singleton()->draw_list_bind_index_array(draw_list, co->index_array);
				RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, sizeof(PositionalShadowRenderPushConstant));

				RD::get_singleton()->draw_list_draw(draw_list, true);
			}
		}
	}
	RD::get_singleton()->draw_list_end();
}

void RendererCanvasRenderRD::light_update_directional_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_cull_distance, const Rect2 &p_clip_rect, LightOccluderInstance *p_occluders) {
	CanvasLight *cl = canvas_light_owner.get_or_null(p_rid);
	ERR_FAIL_COND(!cl->shadow.enabled);

	_update_shadow_atlas();

	Vector2 light_dir = p_light_xform.columns[1].normalized();

	Vector2 center = p_clip_rect.get_center();

	float to_edge_distance = Math::abs(light_dir.dot(p_clip_rect.get_support(-light_dir)) - light_dir.dot(center));

	Vector2 from_pos = center - light_dir * (to_edge_distance + p_cull_distance);
	float distance = to_edge_distance * 2.0 + p_cull_distance;
	float half_size = p_clip_rect.size.length() * 0.5; //shadow length, must keep this no matter the angle

	cl->shadow.z_far = distance;
	cl->shadow.y_offset = float(p_shadow_index * 2 + 1) / float(MAX_LIGHTS_PER_RENDER * 2);

	Transform2D to_light_xform;

	to_light_xform[2] = from_pos;
	to_light_xform[1] = light_dir;
	to_light_xform[0] = -light_dir.orthogonal();

	to_light_xform.invert();

	Vector<Color> cc;
	cc.push_back(Color(1, 1, 1, 1));

	Rect2i rect(0, p_shadow_index * 2, state.shadow_texture_size, 2);
	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(state.shadow_fb, RD::DRAW_CLEAR_ALL, cc, 1.0f, 0, rect);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, shadow_render.render_pipelines[SHADOW_RENDER_MODE_DIRECTIONAL_SHADOW]);

	Projection projection;
	projection.set_orthogonal(-half_size, half_size, -0.5, 0.5, 0.0, distance);
	projection = projection * Projection(Transform3D().looking_at(Vector3(0, 1, 0), Vector3(0, 0, -1)).affine_inverse());

	ShadowRenderPushConstant push_constant;
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			push_constant.projection[y * 4 + x] = projection.columns[y][x];
		}
	}

	push_constant.direction[0] = 0.0;
	push_constant.direction[1] = 1.0;
	push_constant.z_far = distance;

	LightOccluderInstance *instance = p_occluders;

	while (instance) {
		OccluderPolygon *co = occluder_polygon_owner.get_or_null(instance->occluder);

		if (!co || co->index_array.is_null() || !(p_light_mask & instance->light_mask)) {
			instance = instance->next;
			continue;
		}

		_update_transform_2d_to_mat2x4(to_light_xform * instance->xform_cache, push_constant.modelview);
		push_constant.cull_mode = uint32_t(co->cull_mode);

		RD::get_singleton()->draw_list_bind_vertex_array(draw_list, co->vertex_array);
		RD::get_singleton()->draw_list_bind_index_array(draw_list, co->index_array);
		RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, sizeof(ShadowRenderPushConstant));

		RD::get_singleton()->draw_list_draw(draw_list, true);

		instance = instance->next;
	}

	RD::get_singleton()->draw_list_end();

	Transform2D to_shadow;
	to_shadow.columns[0].x = 1.0 / -(half_size * 2.0);
	to_shadow.columns[2].x = 0.5;

	cl->shadow.directional_xform = to_shadow * to_light_xform;
}

void RendererCanvasRenderRD::render_sdf(RID p_render_target, LightOccluderInstance *p_occluders) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	RID fb = texture_storage->render_target_get_sdf_framebuffer(p_render_target);
	Rect2i rect = texture_storage->render_target_get_sdf_rect(p_render_target);

	Transform2D to_sdf;
	to_sdf.columns[0] *= rect.size.width;
	to_sdf.columns[1] *= rect.size.height;
	to_sdf.columns[2] = rect.position;

	Transform2D to_clip;
	to_clip.columns[0] *= 2.0;
	to_clip.columns[1] *= 2.0;
	to_clip.columns[2] = -Vector2(1.0, 1.0);

	to_clip = to_clip * to_sdf.affine_inverse();

	Vector<Color> cc;
	cc.push_back(Color(0, 0, 0, 0));

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(fb, RD::DRAW_CLEAR_ALL, cc);

	Projection projection;

	ShadowRenderPushConstant push_constant;
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			push_constant.projection[y * 4 + x] = projection.columns[y][x];
		}
	}

	push_constant.direction[0] = 0.0;
	push_constant.direction[1] = 0.0;
	push_constant.z_far = 0;
	push_constant.cull_mode = 0;

	LightOccluderInstance *instance = p_occluders;

	while (instance) {
		OccluderPolygon *co = occluder_polygon_owner.get_or_null(instance->occluder);

		if (!co || co->sdf_index_array.is_null() || !instance->sdf_collision) {
			instance = instance->next;
			continue;
		}

		_update_transform_2d_to_mat2x4(to_clip * instance->xform_cache, push_constant.modelview);

		RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, shadow_render.sdf_render_pipelines[co->sdf_is_lines ? SHADOW_RENDER_SDF_LINES : SHADOW_RENDER_SDF_TRIANGLES]);
		RD::get_singleton()->draw_list_bind_vertex_array(draw_list, co->sdf_vertex_array);
		RD::get_singleton()->draw_list_bind_index_array(draw_list, co->sdf_index_array);
		RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, sizeof(ShadowRenderPushConstant));

		RD::get_singleton()->draw_list_draw(draw_list, true);

		instance = instance->next;
	}

	RD::get_singleton()->draw_list_end();

	texture_storage->render_target_sdf_process(p_render_target); //done rendering, process it
}

RID RendererCanvasRenderRD::occluder_polygon_create() {
	OccluderPolygon occluder;
	occluder.line_point_count = 0;
	occluder.sdf_point_count = 0;
	occluder.sdf_index_count = 0;
	occluder.cull_mode = RSE::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED;
	return occluder_polygon_owner.make_rid(occluder);
}

void RendererCanvasRenderRD::occluder_polygon_set_shape(RID p_occluder, const Vector<Vector2> &p_points, bool p_closed) {
	OccluderPolygon *oc = occluder_polygon_owner.get_or_null(p_occluder);
	ERR_FAIL_NULL(oc);

	Vector<Vector2> lines;

	if (p_points.size()) {
		int lc = p_points.size() * 2;

		lines.resize(lc - (p_closed ? 0 : 2));
		{
			Vector2 *w = lines.ptrw();
			const Vector2 *r = p_points.ptr();

			int max = lc / 2;
			if (!p_closed) {
				max--;
			}
			for (int i = 0; i < max; i++) {
				Vector2 a = r[i];
				Vector2 b = r[(i + 1) % (lc / 2)];
				w[i * 2 + 0] = a;
				w[i * 2 + 1] = b;
			}
		}
	}

	if ((oc->line_point_count != lines.size() || lines.is_empty()) && oc->vertex_array.is_valid()) {
		RD::get_singleton()->free_rid(oc->vertex_array);
		RD::get_singleton()->free_rid(oc->vertex_buffer);
		RD::get_singleton()->free_rid(oc->index_array);
		RD::get_singleton()->free_rid(oc->index_buffer);

		oc->vertex_array = RID();
		oc->vertex_buffer = RID();
		oc->index_array = RID();
		oc->index_buffer = RID();

		oc->line_point_count = lines.size();
	}

	if (lines.size()) {
		oc->line_point_count = lines.size();
		Vector<uint8_t> geometry;
		Vector<uint8_t> indices;
		int lc = lines.size();

		geometry.resize(lc * 6 * sizeof(float));
		indices.resize(lc * 3 * sizeof(uint16_t));

		{
			uint8_t *vw = geometry.ptrw();
			float *vwptr = reinterpret_cast<float *>(vw);
			uint8_t *iw = indices.ptrw();
			uint16_t *iwptr = (uint16_t *)iw;

			const Vector2 *lr = lines.ptr();

			const int POLY_HEIGHT = 16384;

			for (int i = 0; i < lc / 2; i++) {
				vwptr[i * 12 + 0] = lr[i * 2 + 0].x;
				vwptr[i * 12 + 1] = lr[i * 2 + 0].y;
				vwptr[i * 12 + 2] = POLY_HEIGHT;

				vwptr[i * 12 + 3] = lr[i * 2 + 1].x;
				vwptr[i * 12 + 4] = lr[i * 2 + 1].y;
				vwptr[i * 12 + 5] = POLY_HEIGHT;

				vwptr[i * 12 + 6] = lr[i * 2 + 1].x;
				vwptr[i * 12 + 7] = lr[i * 2 + 1].y;
				vwptr[i * 12 + 8] = -POLY_HEIGHT;

				vwptr[i * 12 + 9] = lr[i * 2 + 0].x;
				vwptr[i * 12 + 10] = lr[i * 2 + 0].y;
				vwptr[i * 12 + 11] = -POLY_HEIGHT;

				iwptr[i * 6 + 0] = i * 4 + 0;
				iwptr[i * 6 + 1] = i * 4 + 1;
				iwptr[i * 6 + 2] = i * 4 + 2;

				iwptr[i * 6 + 3] = i * 4 + 2;
				iwptr[i * 6 + 4] = i * 4 + 3;
				iwptr[i * 6 + 5] = i * 4 + 0;
			}
		}

		//if same buffer len is being set, just use buffer_update to avoid a pipeline flush

		if (oc->vertex_array.is_null()) {
			//create from scratch
			//vertices
			oc->vertex_buffer = RD::get_singleton()->vertex_buffer_create(lc * 6 * sizeof(float), geometry);

			Vector<RID> buffer;
			buffer.push_back(oc->vertex_buffer);
			oc->vertex_array = RD::get_singleton()->vertex_array_create(4 * lc / 2, shadow_render.vertex_format, buffer);
			//indices

			oc->index_buffer = RD::get_singleton()->index_buffer_create(3 * lc, RD::INDEX_BUFFER_FORMAT_UINT16, indices);
			oc->index_array = RD::get_singleton()->index_array_create(oc->index_buffer, 0, 3 * lc);

		} else {
			//update existing
			const uint8_t *vr = geometry.ptr();
			RD::get_singleton()->buffer_update(oc->vertex_buffer, 0, geometry.size(), vr);
			const uint8_t *ir = indices.ptr();
			RD::get_singleton()->buffer_update(oc->index_buffer, 0, indices.size(), ir);
		}
	}

	// sdf

	Vector<int> sdf_indices;

	if (p_points.size()) {
		if (p_closed) {
			sdf_indices = Geometry2D::triangulate_polygon(p_points);
			oc->sdf_is_lines = false;
		} else {
			int max = p_points.size();
			sdf_indices.resize(max * 2);

			int *iw = sdf_indices.ptrw();
			for (int i = 0; i < max; i++) {
				iw[i * 2 + 0] = i;
				iw[i * 2 + 1] = (i + 1) % max;
			}
			oc->sdf_is_lines = true;
		}
	}

	if (((oc->sdf_index_count != sdf_indices.size() && oc->sdf_point_count != p_points.size()) || p_points.is_empty()) && oc->sdf_vertex_array.is_valid()) {
		RD::get_singleton()->free_rid(oc->sdf_vertex_array);
		RD::get_singleton()->free_rid(oc->sdf_vertex_buffer);
		RD::get_singleton()->free_rid(oc->sdf_index_array);
		RD::get_singleton()->free_rid(oc->sdf_index_buffer);

		oc->sdf_vertex_array = RID();
		oc->sdf_vertex_buffer = RID();
		oc->sdf_index_array = RID();
		oc->sdf_index_buffer = RID();

		oc->sdf_index_count = sdf_indices.size();
		oc->sdf_point_count = p_points.size();

		oc->sdf_is_lines = false;
	}

	if (sdf_indices.size()) {
		if (oc->sdf_vertex_array.is_null()) {
			//create from scratch
			//vertices
#ifdef REAL_T_IS_DOUBLE
			PackedFloat32Array float_points;
			float_points.resize(p_points.size() * 2);
			float *float_points_ptr = (float *)float_points.ptrw();
			for (int i = 0; i < p_points.size(); i++) {
				float_points_ptr[i * 2] = p_points[i].x;
				float_points_ptr[i * 2 + 1] = p_points[i].y;
			}
			oc->sdf_vertex_buffer = RD::get_singleton()->vertex_buffer_create(p_points.size() * 2 * sizeof(float), float_points.span().reinterpret<uint8_t>());
#else
			oc->sdf_vertex_buffer = RD::get_singleton()->vertex_buffer_create(p_points.size() * 2 * sizeof(float), p_points.span().reinterpret<uint8_t>());
#endif
			oc->sdf_index_buffer = RD::get_singleton()->index_buffer_create(sdf_indices.size(), RD::INDEX_BUFFER_FORMAT_UINT32, sdf_indices.span().reinterpret<uint8_t>());
			oc->sdf_index_array = RD::get_singleton()->index_array_create(oc->sdf_index_buffer, 0, sdf_indices.size());

			Vector<RID> buffer;
			buffer.push_back(oc->sdf_vertex_buffer);
			oc->sdf_vertex_array = RD::get_singleton()->vertex_array_create(p_points.size(), shadow_render.sdf_vertex_format, buffer);
			//indices

		} else {
			//update existing
#ifdef REAL_T_IS_DOUBLE
			PackedFloat32Array float_points;
			float_points.resize(p_points.size() * 2);
			float *float_points_ptr = (float *)float_points.ptrw();
			for (int i = 0; i < p_points.size(); i++) {
				float_points_ptr[i * 2] = p_points[i].x;
				float_points_ptr[i * 2 + 1] = p_points[i].y;
			}
			RD::get_singleton()->buffer_update(oc->sdf_vertex_buffer, 0, sizeof(float) * 2 * p_points.size(), float_points.ptr());
#else
			RD::get_singleton()->buffer_update(oc->sdf_vertex_buffer, 0, sizeof(float) * 2 * p_points.size(), p_points.ptr());
#endif
			RD::get_singleton()->buffer_update(oc->sdf_index_buffer, 0, sdf_indices.size() * sizeof(int32_t), sdf_indices.ptr());
		}
	}
}

void RendererCanvasRenderRD::occluder_polygon_set_cull_mode(RID p_occluder, RSE::CanvasOccluderPolygonCullMode p_mode) {
	OccluderPolygon *oc = occluder_polygon_owner.get_or_null(p_occluder);
	ERR_FAIL_NULL(oc);
	oc->cull_mode = p_mode;
}

void RendererCanvasRenderRD::CanvasShaderData::_clear_vertex_input_mask_cache() {
	for (uint32_t i = 0; i < VERTEX_INPUT_MASKS_SIZE; i++) {
		vertex_input_masks[i].store(0);
	}
}

void RendererCanvasRenderRD::CanvasShaderData::_create_pipeline(PipelineKey p_pipeline_key) {
#if PRINT_PIPELINE_COMPILATION_KEYS
	print_line(
			"HASH:", p_pipeline_key.hash(),
			"VERSION:", version,
			"VARIANT:", p_pipeline_key.variant,
			"FRAMEBUFFER:", p_pipeline_key.framebuffer_format_id,
			"VERTEX:", p_pipeline_key.vertex_format_id,
			"PRIMITIVE:", p_pipeline_key.render_primitive,
			"SPEC PACKED #0:", p_pipeline_key.shader_specialization.packed_0,
			"LCD:", p_pipeline_key.lcd_blend);
#endif

	RendererRD::MaterialStorage::ShaderData::BlendMode blend_mode_rd = RendererRD::MaterialStorage::ShaderData::BlendMode(blend_mode);
	RD::PipelineColorBlendState blend_state;
	RD::PipelineColorBlendState::Attachment attachment;
	uint32_t dynamic_state_flags = 0;
	const char *blend_recipe_branch = nullptr;
	bool src_color_premul_override_applied = false;
	bool dst_factors_zero_override_applied = false;
	bool src_alpha_zero_override_applied = false;
	bool enable_blend_false_override_applied = false;
	bool blend_ops_non_add_override_applied = false;
	gdgs_canvas_compute_blend_recipe(blend_mode_rd, p_pipeline_key.lcd_blend, p_pipeline_key.gdgs_temp_diag_src_color_premul_experiment != 0, p_pipeline_key.gdgs_temp_diag_dst_factors_zero_experiment != 0, p_pipeline_key.gdgs_temp_diag_src_alpha_zero_experiment != 0, p_pipeline_key.gdgs_temp_diag_enable_blend_false_experiment != 0, p_pipeline_key.gdgs_temp_diag_blend_ops_non_add_experiment != 0, attachment, dynamic_state_flags, blend_recipe_branch, src_color_premul_override_applied, dst_factors_zero_override_applied, src_alpha_zero_override_applied, enable_blend_false_override_applied, blend_ops_non_add_override_applied);
	(void)blend_recipe_branch;
	(void)src_color_premul_override_applied;
	(void)dst_factors_zero_override_applied;
	(void)src_alpha_zero_override_applied;
	(void)enable_blend_false_override_applied;
	(void)blend_ops_non_add_override_applied;

	blend_state.attachments.push_back(attachment);

	RD::PipelineMultisampleState multisample_state;
	multisample_state.sample_count = RD::get_singleton()->framebuffer_format_get_texture_samples(p_pipeline_key.framebuffer_format_id, 0);

	// Convert the specialization from the key to pipeline specialization constants.
	Vector<RD::PipelineSpecializationConstant> specialization_constants;
	RD::PipelineSpecializationConstant sc;
	sc.constant_id = 0;
	sc.int_value = p_pipeline_key.shader_specialization.packed_0;
	sc.type = RD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT;
	specialization_constants.push_back(sc);

	RID shader_rid = get_shader(p_pipeline_key.variant, p_pipeline_key.ubershader);
	ERR_FAIL_COND(shader_rid.is_null());

	RID pipeline = RD::get_singleton()->render_pipeline_create(shader_rid, p_pipeline_key.framebuffer_format_id, p_pipeline_key.vertex_format_id, p_pipeline_key.render_primitive, RD::PipelineRasterizationState(), multisample_state, RD::PipelineDepthStencilState(), blend_state, dynamic_state_flags, 0, specialization_constants);
	ERR_FAIL_COND(pipeline.is_null());

	const uint32_t gdgs_pipeline_hash = p_pipeline_key.hash();
	bool gdgs_trace_target_armed = false;
	bool gdgs_trace_create_observed = false;
	RID gdgs_trace_created_pipeline;
	gdgs_canvas_pipeline_realization_trace_snapshot(gdgs_pipeline_hash, gdgs_trace_target_armed, gdgs_trace_create_observed, gdgs_trace_created_pipeline);
	(void)gdgs_trace_create_observed;
	(void)gdgs_trace_created_pipeline;
	if (gdgs_trace_target_armed) {
		gdgs_canvas_pipeline_realization_trace_record_create(gdgs_pipeline_hash, pipeline);
		print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_pipeline_realization_create={pipeline_hash=%s,shader_rid=%s,framebuffer_format_id=%d,vertex_format_id=%d,render_primitive=%s,specialization_packed_0=0x%x,lcd_blend=%s,ubershader=%s,compiled_pipeline_rid=%s}",
				gdgs_u64_hex_string(gdgs_pipeline_hash),
				gdgs_rid_to_string(shader_rid),
				(int)p_pipeline_key.framebuffer_format_id,
				(int)p_pipeline_key.vertex_format_id,
				gdgs_canvas_render_primitive_name(p_pipeline_key.render_primitive),
				p_pipeline_key.shader_specialization.packed_0,
				p_pipeline_key.lcd_blend ? "true" : "false",
				p_pipeline_key.ubershader ? "true" : "false",
				gdgs_rid_to_string(pipeline)));
	}

	pipeline_hash_map.add_compiled_pipeline(gdgs_pipeline_hash, pipeline);
}

void RendererCanvasRenderRD::CanvasShaderData::set_code(const String &p_code) {
	//compile

	code = p_code;
	ubo_size = 0;
	uniforms.clear();
	uses_screen_texture = false;
	uses_screen_texture_mipmaps = false;
	uses_sdf = false;
	uses_time = false;
	_clear_vertex_input_mask_cache();

	if (code.is_empty()) {
		return; //just invalid, but no error
	}

	ShaderCompiler::GeneratedCode gen_code;

	blend_mode = BLEND_MODE_MIX;

	ShaderCompiler::IdentifierActions actions;
	actions.entry_point_stages["vertex"] = ShaderCompiler::STAGE_VERTEX;
	actions.entry_point_stages["fragment"] = ShaderCompiler::STAGE_FRAGMENT;
	actions.entry_point_stages["light"] = ShaderCompiler::STAGE_FRAGMENT;

	actions.render_mode_values["blend_add"] = Pair<int *, int>(&blend_mode, BLEND_MODE_ADD);
	actions.render_mode_values["blend_mix"] = Pair<int *, int>(&blend_mode, BLEND_MODE_MIX);
	actions.render_mode_values["blend_sub"] = Pair<int *, int>(&blend_mode, BLEND_MODE_SUB);
	actions.render_mode_values["blend_mul"] = Pair<int *, int>(&blend_mode, BLEND_MODE_MUL);
	actions.render_mode_values["blend_premul_alpha"] = Pair<int *, int>(&blend_mode, BLEND_MODE_PREMULTIPLIED_ALPHA);
	actions.render_mode_values["blend_disabled"] = Pair<int *, int>(&blend_mode, BLEND_MODE_DISABLED);

	actions.usage_flag_pointers["texture_sdf"] = &uses_sdf;
	actions.usage_flag_pointers["texture_sdf_normal"] = &uses_sdf;
	actions.usage_flag_pointers["TIME"] = &uses_time;

	actions.uniforms = &uniforms;

	RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
	MutexLock lock(canvas_singleton->shader.mutex);

	Error err = canvas_singleton->shader.compiler.compile(RSE::SHADER_CANVAS_ITEM, code, &actions, path, gen_code);
	if (err != OK) {
		if (version.is_valid()) {
			canvas_singleton->shader.canvas_shader.version_free(version);
			version = RID();
		}
		ERR_FAIL_MSG("Shader compilation failed.");
	}

	uses_screen_texture_mipmaps = gen_code.uses_screen_texture_mipmaps;
	uses_screen_texture = gen_code.uses_screen_texture;

	pipeline_hash_map.clear_pipelines();

	if (version.is_null()) {
		version = canvas_singleton->shader.canvas_shader.version_create(false);
	}

#if 0
	print_line("**compiling shader:");
	print_line("**defines:\n");
	for (int i = 0; i < gen_code.defines.size(); i++) {
		print_line(gen_code.defines[i]);
	}

	HashMap<String, String>::Iterator el = gen_code.code.begin();
	while (el) {
		print_line("\n**code " + el->key + ":\n" + el->value);
		++el;
	}

	print_line("\n**uniforms:\n" + gen_code.uniforms);
	print_line("\n**vertex_globals:\n" + gen_code.stage_globals[ShaderCompiler::STAGE_VERTEX]);
	print_line("\n**fragment_globals:\n" + gen_code.stage_globals[ShaderCompiler::STAGE_FRAGMENT]);
#endif
	canvas_singleton->shader.canvas_shader.version_set_code(version, gen_code.code, gen_code.uniforms, gen_code.stage_globals[ShaderCompiler::STAGE_VERTEX], gen_code.stage_globals[ShaderCompiler::STAGE_FRAGMENT], gen_code.defines);

	ubo_size = gen_code.uniform_total_size;
	ubo_offsets = gen_code.uniform_offsets;
	texture_uniforms = gen_code.texture_uniforms;
}

bool RendererCanvasRenderRD::CanvasShaderData::is_animated() const {
	return false;
}

bool RendererCanvasRenderRD::CanvasShaderData::casts_shadows() const {
	return false;
}

RenderingServerTypes::ShaderNativeSourceCode RendererCanvasRenderRD::CanvasShaderData::get_native_source_code() const {
	RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
	MutexLock lock(canvas_singleton->shader.mutex);
	return canvas_singleton->shader.canvas_shader.version_get_native_source_code(version);
}

Pair<ShaderRD *, RID> RendererCanvasRenderRD::CanvasShaderData::get_native_shader_and_version() const {
	RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
	return { &canvas_singleton->shader.canvas_shader, version };
}

RID RendererCanvasRenderRD::CanvasShaderData::get_shader(ShaderVariant p_shader_variant, bool p_ubershader) const {
	if (version.is_valid()) {
		uint32_t variant_index = p_shader_variant + (p_ubershader ? SHADER_VARIANT_MAX : 0);
		RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
		MutexLock lock(canvas_singleton->shader.mutex);
		return canvas_singleton->shader.canvas_shader.version_get_shader(version, variant_index);
	} else {
		return RID();
	}
}

uint64_t RendererCanvasRenderRD::CanvasShaderData::get_vertex_input_mask(ShaderVariant p_shader_variant, bool p_ubershader) {
	// Vertex input masks require knowledge of the shader. Since querying the shader can be expensive due to high contention and the necessary mutex, we cache the result instead.
	uint32_t input_mask_index = p_shader_variant + (p_ubershader ? SHADER_VARIANT_MAX : 0);
	uint64_t input_mask = vertex_input_masks[input_mask_index].load(std::memory_order_relaxed);
	if (input_mask == 0) {
		RID shader_rid = get_shader(p_shader_variant, p_ubershader);
		ERR_FAIL_COND_V(shader_rid.is_null(), 0);

		input_mask = RD::get_singleton()->shader_get_vertex_input_attribute_mask(shader_rid);
		vertex_input_masks[input_mask_index].store(input_mask, std::memory_order_relaxed);
	}

	return input_mask;
}

bool RendererCanvasRenderRD::CanvasShaderData::is_valid() const {
	if (version.is_valid()) {
		RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
		MutexLock lock(canvas_singleton->shader.mutex);
		return canvas_singleton->shader.canvas_shader.version_is_valid(version);
	} else {
		return false;
	}
}

RendererCanvasRenderRD::CanvasShaderData::CanvasShaderData() {
	RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
	pipeline_hash_map.set_creation_object_and_function(this, &CanvasShaderData::_create_pipeline);
	pipeline_hash_map.set_compilations(&canvas_singleton->shader.pipeline_compilations[0], &canvas_singleton->shader.mutex);
}

RendererCanvasRenderRD::CanvasShaderData::~CanvasShaderData() {
	pipeline_hash_map.clear_pipelines();

	if (version.is_valid()) {
		RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
		MutexLock lock(canvas_singleton->shader.mutex);
		canvas_singleton->shader.canvas_shader.version_free(version);
	}
}

RendererRD::MaterialStorage::ShaderData *RendererCanvasRenderRD::_create_shader_func() {
	CanvasShaderData *shader_data = memnew(CanvasShaderData);
	return shader_data;
}

bool RendererCanvasRenderRD::CanvasMaterialData::update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty) {
	RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
	MutexLock lock(canvas_singleton->shader.mutex);
	RID shader_to_update = canvas_singleton->shader.canvas_shader.version_get_shader(shader_data->version, 0);
	bool uniform_set_changed = update_parameters_uniform_set(p_parameters, p_uniform_dirty, p_textures_dirty, shader_data->uniforms, shader_data->ubo_offsets.ptr(), shader_data->texture_uniforms, shader_data->default_texture_params, shader_data->ubo_size, uniform_set, shader_to_update, MATERIAL_UNIFORM_SET, true, false);
	bool uniform_set_srgb_changed = update_parameters_uniform_set(p_parameters, p_uniform_dirty, p_textures_dirty, shader_data->uniforms, shader_data->ubo_offsets.ptr(), shader_data->texture_uniforms, shader_data->default_texture_params, shader_data->ubo_size, uniform_set_srgb, shader_to_update, MATERIAL_UNIFORM_SET, false, false);
	return uniform_set_changed || uniform_set_srgb_changed;
}

RendererCanvasRenderRD::CanvasMaterialData::~CanvasMaterialData() {
	free_parameters_uniform_set(uniform_set);
	free_parameters_uniform_set(uniform_set_srgb);
}

RendererRD::MaterialStorage::MaterialData *RendererCanvasRenderRD::_create_material_func(CanvasShaderData *p_shader) {
	CanvasMaterialData *material_data = memnew(CanvasMaterialData);
	material_data->shader_data = p_shader;
	//update will happen later anyway so do nothing.
	return material_data;
}

void RendererCanvasRenderRD::set_time(double p_time) {
	state.time = p_time;
}

void RendererCanvasRenderRD::update() {
}

RendererCanvasRenderRD::RendererCanvasRenderRD() {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

	{ //create default samplers

		default_samplers.default_filter = RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR;
		default_samplers.default_repeat = RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED;
	}

	// preallocate slots for uniform set 3
	state.batch_texture_uniforms.resize(4);

	{ //shader variants

		String global_defines;
		global_defines += "#define MAX_LIGHTS " + itos(MAX_LIGHTS_PER_RENDER) + "\n";
		global_defines += "\n#define SAMPLERS_BINDING_FIRST_INDEX " + itos(SAMPLERS_BINDING_FIRST_INDEX) + "\n";

		state.light_uniforms = memnew_arr(LightUniform, MAX_LIGHTS_PER_RENDER);
		Vector<String> variants;
		const uint32_t ubershader_iterations = 1;
		for (uint32_t ubershader = 0; ubershader < ubershader_iterations; ubershader++) {
			const String base_define = ubershader ? "\n#define UBERSHADER\n" : "";
			variants.push_back(base_define + ""); // SHADER_VARIANT_QUAD
			variants.push_back(base_define + "#define USE_NINEPATCH\n"); // SHADER_VARIANT_NINEPATCH
			variants.push_back(base_define + "#define USE_PRIMITIVE\n"); // SHADER_VARIANT_PRIMITIVE
			variants.push_back(base_define + "#define USE_PRIMITIVE\n#define USE_POINT_SIZE\n"); // SHADER_VARIANT_PRIMITIVE_POINTS
			variants.push_back(base_define + "#define USE_ATTRIBUTES\n"); // SHADER_VARIANT_ATTRIBUTES
			variants.push_back(base_define + "#define USE_ATTRIBUTES\n#define USE_POINT_SIZE\n"); // SHADER_VARIANT_ATTRIBUTES_POINTS
		}

		shader.canvas_shader.initialize(variants, global_defines, {}, {});

		shader.default_version_data = memnew(CanvasShaderData);
		shader.default_version_data->version = shader.canvas_shader.version_create();
		shader.default_version_data->blend_mode = RendererRD::MaterialStorage::ShaderData::BLEND_MODE_MIX;
		shader.default_version_rd_shader = shader.default_version_data->get_shader(SHADER_VARIANT_QUAD, false);
	}

	{
		//shader compiler
		ShaderCompiler::DefaultIdentifierActions actions;

		actions.renames["VERTEX"] = "vertex";
		actions.renames["LIGHT_VERTEX"] = "light_vertex";
		actions.renames["SHADOW_VERTEX"] = "shadow_vertex";
		actions.renames["UV"] = "uv";
		actions.renames["POINT_SIZE"] = "point_size";

		actions.renames["MODEL_MATRIX"] = "model_matrix";
		actions.renames["CANVAS_MATRIX"] = "canvas_data.canvas_transform";
		actions.renames["SCREEN_MATRIX"] = "canvas_data.screen_transform";
		actions.renames["TIME"] = "canvas_data.time";
		actions.renames["PI"] = String::num(Math::PI);
		actions.renames["TAU"] = String::num(Math::TAU);
		actions.renames["E"] = String::num(Math::E);
		actions.renames["AT_LIGHT_PASS"] = "false";
		actions.renames["INSTANCE_CUSTOM"] = "instance_custom";

		actions.renames["COLOR"] = "color";
		actions.renames["NORMAL"] = "normal";
		actions.renames["NORMAL_MAP"] = "normal_map";
		actions.renames["NORMAL_MAP_DEPTH"] = "normal_map_depth";
		actions.renames["TEXTURE"] = "color_texture";
		actions.renames["TEXTURE_PIXEL_SIZE"] = "read_draw_data_color_texture_pixel_size";
		actions.renames["NORMAL_TEXTURE"] = "normal_texture";
		actions.renames["SPECULAR_SHININESS_TEXTURE"] = "specular_texture";
		actions.renames["SPECULAR_SHININESS"] = "specular_shininess";
		actions.renames["SCREEN_UV"] = "screen_uv";
		actions.renames["REGION_RECT"] = "region_rect";
		actions.renames["SCREEN_PIXEL_SIZE"] = "canvas_data.screen_pixel_size";
		actions.renames["FRAGCOORD"] = "gl_FragCoord";
		actions.renames["POINT_COORD"] = "gl_PointCoord";
		actions.renames["INSTANCE_ID"] = "gl_InstanceIndex";
		actions.renames["VERTEX_ID"] = "gl_VertexIndex";

		actions.renames["CUSTOM0"] = "custom0";
		actions.renames["CUSTOM1"] = "custom1";

		actions.renames["LIGHT_POSITION"] = "light_position";
		actions.renames["LIGHT_DIRECTION"] = "light_direction";
		actions.renames["LIGHT_IS_DIRECTIONAL"] = "is_directional";
		actions.renames["LIGHT_COLOR"] = "light_color";
		actions.renames["LIGHT_ENERGY"] = "light_energy";
		actions.renames["LIGHT"] = "light";
		actions.renames["SHADOW_MODULATE"] = "shadow_modulate";

		actions.renames["texture_sdf"] = "texture_sdf";
		actions.renames["texture_sdf_normal"] = "texture_sdf_normal";
		actions.renames["sdf_to_screen_uv"] = "sdf_to_screen_uv";
		actions.renames["screen_uv_to_sdf"] = "screen_uv_to_sdf";

		actions.usage_defines["COLOR"] = "#define COLOR_USED\n";
		actions.usage_defines["SCREEN_UV"] = "#define SCREEN_UV_USED\n";
		actions.usage_defines["SCREEN_PIXEL_SIZE"] = "@SCREEN_UV";
		actions.usage_defines["NORMAL"] = "#define NORMAL_USED\n";
		actions.usage_defines["NORMAL_MAP"] = "#define NORMAL_MAP_USED\n";
		actions.usage_defines["SPECULAR_SHININESS"] = "#define SPECULAR_SHININESS_USED\n";
		actions.usage_defines["POINT_SIZE"] = "#define USE_POINT_SIZE\n";
		actions.usage_defines["CUSTOM0"] = "#define CUSTOM0_USED\n";
		actions.usage_defines["CUSTOM1"] = "#define CUSTOM1_USED\n";

		actions.render_mode_defines["skip_vertex_transform"] = "#define SKIP_TRANSFORM_USED\n";
		actions.render_mode_defines["unshaded"] = "#define MODE_UNSHADED\n";
		actions.render_mode_defines["light_only"] = "#define MODE_LIGHT_ONLY\n";
		actions.render_mode_defines["world_vertex_coords"] = "#define USE_WORLD_VERTEX_COORDS\n";

		actions.custom_samplers["TEXTURE"] = "texture_sampler";
		actions.custom_samplers["NORMAL_TEXTURE"] = "texture_sampler";
		actions.custom_samplers["SPECULAR_SHININESS_TEXTURE"] = "texture_sampler";
		actions.base_texture_binding_index = 1;
		actions.texture_layout_set = MATERIAL_UNIFORM_SET;
		actions.base_uniform_string = "material.";
		actions.default_filter = ShaderLanguage::FILTER_LINEAR;
		actions.default_repeat = ShaderLanguage::REPEAT_DISABLE;
		actions.base_varying_index = 9;

		actions.global_buffer_array_variable = "global_shader_uniforms.data";
		actions.instance_uniform_index_variable = "read_draw_data_instance_offset";

		shader.compiler.initialize(actions);
	}

	{ //shadow rendering
		Vector<String> versions;
		versions.push_back("\n#define MODE_SHADOW\n"); // Shadow.
		versions.push_back("\n#define MODE_SHADOW\n#define POSITIONAL_SHADOW\n"); // Positional shadow.
		versions.push_back("\n#define MODE_SDF\n"); // SDF.
		shadow_render.shader.initialize(versions);

		{
			Vector<RD::AttachmentFormat> attachments;

			RD::AttachmentFormat af_color;
			af_color.format = RD::DATA_FORMAT_R32_SFLOAT;
			af_color.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

			attachments.push_back(af_color);

			RD::AttachmentFormat af_depth;
			af_depth.format = RD::DATA_FORMAT_D32_SFLOAT;
			af_depth.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

			attachments.push_back(af_depth);

			shadow_render.framebuffer_format = RD::get_singleton()->framebuffer_format_create(attachments);
		}

		{
			Vector<RD::AttachmentFormat> attachments;

			RD::AttachmentFormat af_color;
			af_color.format = RD::DATA_FORMAT_R8_UNORM;
			af_color.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

			attachments.push_back(af_color);

			shadow_render.sdf_framebuffer_format = RD::get_singleton()->framebuffer_format_create(attachments);
		}

		//pipelines
		Vector<RD::VertexAttribute> vf;
		RD::VertexAttribute vd;
		vd.format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
		vd.stride = sizeof(float) * 3;
		vd.location = 0;
		vd.offset = 0;
		vf.push_back(vd);
		shadow_render.vertex_format = RD::get_singleton()->vertex_format_create(vf);

		vd.format = RD::DATA_FORMAT_R32G32_SFLOAT;
		vd.stride = sizeof(float) * 2;

		vf.write[0] = vd;
		shadow_render.sdf_vertex_format = RD::get_singleton()->vertex_format_create(vf);

		shadow_render.shader_version = shadow_render.shader.version_create();

		for (int i = 0; i < 2; i++) {
			RD::PipelineRasterizationState rs;
			RD::PipelineDepthStencilState ds;
			ds.enable_depth_write = true;
			ds.enable_depth_test = true;
			ds.depth_compare_operator = RD::COMPARE_OP_LESS;
			shadow_render.render_pipelines[i] = RD::get_singleton()->render_pipeline_create(shadow_render.shader.version_get_shader(shadow_render.shader_version, ShadowRenderMode(i)), shadow_render.framebuffer_format, shadow_render.vertex_format, RD::RENDER_PRIMITIVE_TRIANGLES, rs, RD::PipelineMultisampleState(), ds, RD::PipelineColorBlendState::create_disabled(), 0);
		}

		for (int i = 0; i < 2; i++) {
			shadow_render.sdf_render_pipelines[i] = RD::get_singleton()->render_pipeline_create(shadow_render.shader.version_get_shader(shadow_render.shader_version, SHADOW_RENDER_MODE_SDF), shadow_render.sdf_framebuffer_format, shadow_render.sdf_vertex_format, i == 0 ? RD::RENDER_PRIMITIVE_TRIANGLES : RD::RENDER_PRIMITIVE_LINES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), RD::PipelineColorBlendState::create_disabled(), 0);
		}

		// Unload shader modules to save memory.
		RD::get_singleton()->shader_destroy_modules(shadow_render.shader.version_get_shader(shadow_render.shader_version, SHADOW_RENDER_MODE_DIRECTIONAL_SHADOW));
		RD::get_singleton()->shader_destroy_modules(shadow_render.shader.version_get_shader(shadow_render.shader_version, SHADOW_RENDER_MODE_POSITIONAL_SHADOW));
		RD::get_singleton()->shader_destroy_modules(shadow_render.shader.version_get_shader(shadow_render.shader_version, SHADOW_RENDER_MODE_SDF));
	}

	{ //bindings

		state.canvas_state_buffer = RD::get_singleton()->uniform_buffer_create(sizeof(State::Buffer));
		state.lights_storage_buffer = RD::get_singleton()->storage_buffer_create(sizeof(LightUniform) * MAX_LIGHTS_PER_RENDER);

		RD::SamplerState shadow_sampler_state;
		shadow_sampler_state.mag_filter = RD::SAMPLER_FILTER_NEAREST;
		shadow_sampler_state.min_filter = RD::SAMPLER_FILTER_NEAREST;
		shadow_sampler_state.repeat_u = RD::SAMPLER_REPEAT_MODE_REPEAT; //shadow wrap around
		state.shadow_sampler = RD::get_singleton()->sampler_create(shadow_sampler_state);
	}

	{
		//polygon buffers
		polygon_buffers.last_id = 1;
	}

	{ // default index buffer

		Vector<uint8_t> pv;
		pv.resize(6 * 2);
		{
			uint8_t *w = pv.ptrw();
			uint16_t *p16 = (uint16_t *)w;
			p16[0] = 0;
			p16[1] = 1;
			p16[2] = 2;
			p16[3] = 0;
			p16[4] = 2;
			p16[5] = 3;
		}
		shader.quad_index_buffer = RD::get_singleton()->index_buffer_create(6, RenderingDevice::INDEX_BUFFER_FORMAT_UINT16, pv);
		shader.quad_index_array = RD::get_singleton()->index_array_create(shader.quad_index_buffer, 0, 6);
	}

	{
		Vector<RD::VertexAttribute> vf;
		uint32_t offset = 0;
		RD::VertexAttribute vd;
		vd.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
		vd.stride = sizeof(InstanceData);
		vd.frequency = RD::VERTEX_FREQUENCY_INSTANCE;
		vd.location = 8;
		vd.binding = 0; // Explicitly assign binding 0 for instance data.
		vd.offset = offset;
		offset += sizeof(float) * 4;
		vf.push_back(vd); // attrib_A

		vd.location = 9;
		vd.offset = offset;
		offset += sizeof(float) * 4;
		vf.push_back(vd); // attrib_B

		vd.location = 10;
		vd.offset = offset;
		offset += sizeof(float) * 4;
		vf.push_back(vd); // attrib_C

		vd.location = 11;
		vd.offset = offset;
		offset += sizeof(float) * 4;
		vf.push_back(vd); // attrib_D

		vd.location = 12;
		vd.offset = offset;
		offset += sizeof(float) * 4;
		vf.push_back(vd); // attrib_E

		uint32_t attrib_F_index = vf.size();
		vd.location = 13;
		vd.offset = offset;
		offset += sizeof(float) * 4;
		vf.push_back(vd); // attrib_F (RECT, NINEPATCH)

		vd.format = RD::DATA_FORMAT_R32G32B32A32_UINT;
		vd.location = 14;
		vd.offset = offset;
		offset += sizeof(uint32_t) * 4;
		vf.push_back(vd); // attrib_G

		vd.location = 15;
		vd.offset = offset;
		offset += sizeof(uint32_t) * 4;
		vf.push_back(vd); // attrib_H

		// RECT, NINEPATCH
		shader.quad_vertex_format_id = RD::get_singleton()->vertex_format_create(vf);
		Vector<RD::VertexAttribute> gdgs_temp_diag_quad_vertex_format_binding1_alias = vf;
		for (int i = 0; i < gdgs_temp_diag_quad_vertex_format_binding1_alias.size(); i++) {
			gdgs_temp_diag_quad_vertex_format_binding1_alias.write[i].binding = 1;
		}
		shader.gdgs_temp_diag_quad_vertex_format_id_duplicate = RD::get_singleton()->vertex_format_create(gdgs_temp_diag_quad_vertex_format_binding1_alias);

		// PRIMITIVE
		vf.write[attrib_F_index].format = RD::DATA_FORMAT_R32G32B32A32_UINT;
		shader.primitive_vertex_format_id = RD::get_singleton()->vertex_format_create(vf);
	}

	{ //primitive
		primitive_arrays.index_array[0] = RD::get_singleton()->index_array_create(shader.quad_index_buffer, 0, 1);
		primitive_arrays.index_array[1] = RD::get_singleton()->index_array_create(shader.quad_index_buffer, 0, 2);
		primitive_arrays.index_array[2] = RD::get_singleton()->index_array_create(shader.quad_index_buffer, 0, 3);
		primitive_arrays.index_array[3] = RD::get_singleton()->index_array_create(shader.quad_index_buffer, 0, 6);
	}

	{
		//default shadow texture to keep uniform set happy
		RD::TextureFormat tf;
		tf.texture_type = RD::TEXTURE_TYPE_2D;
		tf.width = 4;
		tf.height = 4;
		tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT;
		tf.format = RD::DATA_FORMAT_R32_SFLOAT;

		state.shadow_texture = RD::get_singleton()->texture_create(tf, RD::TextureView());
	}

	{
		Vector<RD::Uniform> uniforms;

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 0;
			u.append_id(RendererRD::MeshStorage::get_singleton()->get_default_rd_storage_buffer());
			uniforms.push_back(u);
		}

		state.default_transforms_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, shader.default_version_rd_shader, TRANSFORMS_UNIFORM_SET);
	}

	default_canvas_texture = texture_storage->canvas_texture_allocate();
	texture_storage->canvas_texture_initialize(default_canvas_texture);

	RendererRD::TextureStorage::CanvasTextureInfo info = RendererRD::TextureStorage::get_singleton()->canvas_texture_get_info(default_canvas_texture, default_filter, default_repeat, false, false);
	default_texture_info.diffuse = info.diffuse;
	default_texture_info.normal = info.normal;
	default_texture_info.specular = info.specular;
	default_texture_info.sampler = info.sampler;

	state.shadow_texture_size = GLOBAL_GET("rendering/2d/shadow_atlas/size");

	//create functions for shader and material
	material_storage->shader_set_data_request_function(RendererRD::MaterialStorage::SHADER_TYPE_2D, _create_shader_funcs);
	material_storage->material_set_data_request_function(RendererRD::MaterialStorage::SHADER_TYPE_2D, _create_material_funcs);

	state.time = 0;

	{
		default_canvas_group_shader = material_storage->shader_allocate();
		material_storage->shader_initialize(default_canvas_group_shader);

		material_storage->shader_set_code(default_canvas_group_shader, R"(
// Default CanvasGroup shader.

shader_type canvas_item;
render_mode unshaded;

uniform sampler2D screen_texture : hint_screen_texture, repeat_disable, filter_nearest;

void fragment() {
	vec4 c = textureLod(screen_texture, SCREEN_UV, 0.0);

	if (c.a > 0.0001) {
		c.rgb /= c.a;
	}

	COLOR *= c;
}
)");
		default_canvas_group_material = material_storage->material_allocate();
		material_storage->material_initialize(default_canvas_group_material);

		material_storage->material_set_shader(default_canvas_group_material, default_canvas_group_shader);
	}

	{
		default_clip_children_shader = material_storage->shader_allocate();
		material_storage->shader_initialize(default_clip_children_shader);

		material_storage->shader_set_code(default_clip_children_shader, R"(
// Default clip children shader.

shader_type canvas_item;
render_mode unshaded;

uniform sampler2D screen_texture : hint_screen_texture, repeat_disable, filter_nearest;

void fragment() {
	vec4 c = textureLod(screen_texture, SCREEN_UV, 0.0);
	COLOR.rgb = c.rgb;
}
)");
		default_clip_children_material = material_storage->material_allocate();
		material_storage->material_initialize(default_clip_children_material);

		material_storage->material_set_shader(default_clip_children_material, default_clip_children_shader);
	}

	{
		uint32_t cache_size = uint32_t(GLOBAL_GET("rendering/2d/batching/uniform_set_cache_size"));
		rid_set_to_uniform_set.set_capacity(cache_size);
	}

	{
		state.max_instances_per_buffer = uint32_t(GLOBAL_GET("rendering/2d/batching/item_buffer_size"));
		state.max_instance_buffer_size = state.max_instances_per_buffer * sizeof(InstanceData);
		state.canvas_instance_batches.reserve(200);
		state.instance_buffers.set_vertex_size(0, state.max_instance_buffer_size);
	}
}

bool RendererCanvasRenderRD::free(RID p_rid) {
	if (canvas_light_owner.owns(p_rid)) {
		CanvasLight *cl = canvas_light_owner.get_or_null(p_rid);
		ERR_FAIL_NULL_V(cl, false);
		light_set_use_shadow(p_rid, false);
		canvas_light_owner.free(p_rid);
	} else if (occluder_polygon_owner.owns(p_rid)) {
		occluder_polygon_set_shape(p_rid, Vector<Vector2>(), false);
		occluder_polygon_owner.free(p_rid);
	} else {
		return false;
	}

	return true;
}

void RendererCanvasRenderRD::set_shadow_texture_size(int p_size) {
	p_size = MAX(1, Math::nearest_power_of_2_templated(p_size));
	if (p_size == state.shadow_texture_size) {
		return;
	}
	state.shadow_texture_size = p_size;
	if (state.shadow_fb.is_valid()) {
		RD::get_singleton()->free_rid(state.shadow_texture);
		RD::get_singleton()->free_rid(state.shadow_depth_texture);
		state.shadow_fb = RID();

		{
			//create a default shadow texture to keep uniform set happy (and that it gets erased when a new one is created)
			RD::TextureFormat tf;
			tf.texture_type = RD::TEXTURE_TYPE_2D;
			tf.width = 4;
			tf.height = 4;
			tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT;
			tf.format = RD::DATA_FORMAT_R32_SFLOAT;

			state.shadow_texture = RD::get_singleton()->texture_create(tf, RD::TextureView());
		}
	}
}

void RendererCanvasRenderRD::set_debug_redraw(bool p_enabled, double p_time, const Color &p_color) {
	debug_redraw = p_enabled;
	debug_redraw_time = p_time;
	debug_redraw_color = p_color;
}

uint32_t RendererCanvasRenderRD::get_pipeline_compilations(RSE::PipelineSource p_source) {
	RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
	MutexLock lock(canvas_singleton->shader.mutex);
	return shader.pipeline_compilations[p_source];
}

void RendererCanvasRenderRD::_render_batch_items(RenderTarget p_to_render_target, int p_item_count, const Transform2D &p_canvas_transform_inverse, Light *p_lights, bool &r_sdf_used, bool p_to_backbuffer, RenderingServerTypes::RenderInfo *r_render_info) {
	// Record batches
	{
		RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
		Item *current_clip = nullptr;

		// Record Batches.
		// First item always forms its own batch.
		bool batch_broken = false;
		Batch *current_batch = _new_batch(batch_broken);

		for (int i = 0; i < p_item_count; i++) {
			Item *ci = items[i];

			if (ci->final_clip_owner != current_batch->clip) {
				current_batch = _new_batch(batch_broken);
				current_batch->clip = ci->final_clip_owner;
				current_clip = ci->final_clip_owner;
			}

			RID material = ci->material_owner == nullptr ? ci->material : ci->material_owner->material;

			if (ci->use_canvas_group) {
				if (ci->canvas_group->mode == RSE::CANVAS_GROUP_MODE_CLIP_AND_DRAW) {
					material = default_clip_children_material;
				} else {
					if (material.is_null()) {
						if (ci->canvas_group->mode == RSE::CANVAS_GROUP_MODE_CLIP_ONLY) {
							material = default_clip_children_material;
						} else {
							material = default_canvas_group_material;
						}
					}
				}
			}

			if (material != current_batch->material) {
				current_batch = _new_batch(batch_broken);

				CanvasMaterialData *material_data = nullptr;
				if (material.is_valid()) {
					material_data = static_cast<CanvasMaterialData *>(material_storage->material_get_data(material, RendererRD::MaterialStorage::SHADER_TYPE_2D));
				}

				current_batch->material = material;
				current_batch->material_data = material_data;
			}

			if (ci->repeat_source_item == nullptr || ci->repeat_size == Vector2()) {
				Transform2D base_transform = p_canvas_transform_inverse * ci->final_transform;
				_record_item_commands(ci, p_to_render_target, base_transform, current_clip, p_lights, batch_broken, r_sdf_used, current_batch);
			} else {
				Point2 start_pos = ci->repeat_size * -(ci->repeat_times / 2);
				Point2 offset;
				int repeat_times_x = ci->repeat_size.x ? ci->repeat_times : 0;
				int repeat_times_y = ci->repeat_size.y ? ci->repeat_times : 0;
				for (int ry = 0; ry <= repeat_times_y; ry++) {
					offset.y = start_pos.y + ry * ci->repeat_size.y;
					for (int rx = 0; rx <= repeat_times_x; rx++) {
						offset.x = start_pos.x + rx * ci->repeat_size.x;
						Transform2D base_transform = ci->final_transform;
						base_transform.columns[2] += ci->repeat_source_item->final_transform.basis_xform(offset);
						base_transform = p_canvas_transform_inverse * base_transform;
						_record_item_commands(ci, p_to_render_target, base_transform, current_clip, p_lights, batch_broken, r_sdf_used, current_batch);
					}
				}
			}
		}
	}

	if (state.canvas_instance_batches.is_empty()) {
		// Nothing to render, just return.
		return;
	}

	// Render batches

	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	RID framebuffer;
	RID fb_uniform_set;
	bool clear = false;
	Color clear_color;

	if (p_to_backbuffer) {
		framebuffer = texture_storage->render_target_get_rd_backbuffer_framebuffer(p_to_render_target.render_target);
		fb_uniform_set = texture_storage->render_target_get_backbuffer_uniform_set(p_to_render_target.render_target);
	} else {
		framebuffer = texture_storage->render_target_get_rd_framebuffer(p_to_render_target.render_target);
		texture_storage->render_target_set_msaa_needs_resolve(p_to_render_target.render_target, false); // If MSAA is enabled, our framebuffer will be resolved!

		if (texture_storage->render_target_is_clear_requested(p_to_render_target.render_target)) {
			clear = true;
			clear_color = texture_storage->render_target_get_clear_request_color(p_to_render_target.render_target);
			if (texture_storage->render_target_is_using_hdr(p_to_render_target.render_target)) {
				clear_color = clear_color.srgb_to_linear();
			}
			texture_storage->render_target_disable_clear_request(p_to_render_target.render_target);
		}
		// TODO: Obtain from framebuffer format eventually when this is implemented.
		fb_uniform_set = texture_storage->render_target_get_framebuffer_uniform_set(p_to_render_target.render_target);
	}

	if (fb_uniform_set.is_null() || !RD::get_singleton()->uniform_set_is_valid(fb_uniform_set)) {
		fb_uniform_set = _create_base_uniform_set(p_to_render_target.render_target, p_to_backbuffer);
	}

	RD::FramebufferFormatID fb_format = RD::get_singleton()->framebuffer_get_format(framebuffer);

	const bool gdgs_ui_pass_origin_log = gdgs_debug_ui_pass_origin_enabled();
	const BitField<RD::DrawFlags> ui_draw_flags = clear ? BitField<RD::DrawFlags>(RD::DRAW_CLEAR_COLOR_0) : BitField<RD::DrawFlags>(RD::DRAW_DEFAULT_ALL);
	if (gdgs_ui_pass_origin_log && !p_to_backbuffer) {
		int rendered_batch_count = 0;
		int clip_batch_count = 0;
		int destination_color_batch_count = 0;
		int lcd_blend_batch_count = 0;
		int blend_disabled_batch_count = 0;
		int rect_like_batch_count = 0;
		int polygon_batch_count = 0;
		int primitive_batch_count = 0;
		String first_batch_blend_mode = "none";
		String first_destination_color_batch_blend_mode = "none";

		for (uint32_t i = 0; i <= state.current_batch_index; i++) {
			const Batch *batch = &state.canvas_instance_batches[i];
			if (batch->instance_count == 0) {
				continue;
			}
			rendered_batch_count++;
			if (batch->clip != nullptr) {
				clip_batch_count++;
			}
			if (batch->has_blend) {
				lcd_blend_batch_count++;
			}

			const CanvasShaderData *batch_shader_data = batch->material_data && batch->material_data->shader_data && batch->material_data->shader_data->version.is_valid() && batch->material_data->shader_data->is_valid() ? batch->material_data->shader_data : shader.default_version_data;
			const int batch_blend_mode = batch_shader_data ? batch_shader_data->blend_mode : RendererRD::MaterialStorage::ShaderData::BLEND_MODE_DISABLED;
			const char *batch_blend_mode_name = gdgs_canvas_blend_mode_name(batch_blend_mode);
			if (first_batch_blend_mode == "none") {
				first_batch_blend_mode = batch_blend_mode_name;
			}
			if (gdgs_canvas_blend_mode_uses_prior_color(batch_blend_mode)) {
				destination_color_batch_count++;
				if (first_destination_color_batch_blend_mode == "none") {
					first_destination_color_batch_blend_mode = batch_blend_mode_name;
				}
			} else {
				blend_disabled_batch_count++;
			}

			switch (batch->command_type) {
				case Item::Command::TYPE_RECT:
				case Item::Command::TYPE_NINEPATCH:
					rect_like_batch_count++;
					break;
				case Item::Command::TYPE_POLYGON:
					polygon_batch_count++;
					break;
				case Item::Command::TYPE_PRIMITIVE:
					primitive_batch_count++;
					break;
				default:
					break;
			}
		}

		const bool preserve_required = !clear && (destination_color_batch_count > 0 || lcd_blend_batch_count > 0 || clip_batch_count > 0 || rendered_batch_count > 1);
		const char *overwrite_classifier = clear ? "explicit_clear_overwrite" : (preserve_required ? "preserve_prior_root_contents" : "not_proven_full_overwrite");
		const bool all_rendered_batches_are_rect_like = rendered_batch_count > 0 && rect_like_batch_count == rendered_batch_count && polygon_batch_count == 0 && primitive_batch_count == 0;
		const bool all_rendered_batches_need_preserved_color = rendered_batch_count > 0 && destination_color_batch_count == rendered_batch_count;
		const bool all_rendered_batches_are_clipped = rendered_batch_count > 0 && clip_batch_count == rendered_batch_count;
		const char *graph_build_classifier = rendered_batch_count <= 0 ? "empty_ui_pass" : (rendered_batch_count == 1 && all_rendered_batches_are_rect_like && all_rendered_batches_need_preserved_color && !all_rendered_batches_are_clipped ? "single_rect_preserve_batch" : (all_rendered_batches_are_rect_like && all_rendered_batches_need_preserved_color && all_rendered_batches_are_clipped ? "multi_rect_preserve_clip_batch_chain" : (all_rendered_batches_are_rect_like && all_rendered_batches_need_preserved_color ? "multi_rect_preserve_batch_chain" : "mixed_ui_batch_chain")));
		print_line(vformat("[gdgs-canvas] ui_pass_origin={codepath=\"RendererCanvasRenderRD::_render_batch_items\",framebuffer_source=\"render_target_get_rd_framebuffer\",breadcrumb=%d,draw_list_begin_flags=%s,clear_requested=%s,msaa_resolve_disabled_before_ui=true,label_state=\"none_without_draw_command_begin_label\",batch_summary={rendered=%d,rect_like=%d,polygon=%d,primitive=%d,clipped=%d,lcd_blend=%d,destination_color=%d,blend_disabled=%d,first_blend_mode=\"%s\",first_destination_color_blend_mode=\"%s\"},graph_build_condition={classifier=\"%s\",expected_draw_calls_from_rendered_batches=%d,all_rendered_batches_are_rect_like=%s,all_rendered_batches_need_preserved_color=%s,all_rendered_batches_are_clipped=%s},overwrite_classifier=\"%s\",overwrite_classifier_basis=\"blend_or_clip_or_multi_batch_requires_prior_contents\"}", (int)RDD::BreadcrumbMarker::UI_PASS, clear ? "RD::DRAW_CLEAR_COLOR_0" : "RD::DRAW_DEFAULT_ALL", clear ? "true" : "false", rendered_batch_count, rect_like_batch_count, polygon_batch_count, primitive_batch_count, clip_batch_count, lcd_blend_batch_count, destination_color_batch_count, blend_disabled_batch_count, first_batch_blend_mode, first_destination_color_batch_blend_mode, graph_build_classifier, rendered_batch_count, all_rendered_batches_are_rect_like ? "true" : "false", all_rendered_batches_need_preserved_color ? "true" : "false", all_rendered_batches_are_clipped ? "true" : "false", overwrite_classifier));
	}

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(framebuffer, ui_draw_flags, clear_color, 1.0f, 0, Rect2(), RDD::BreadcrumbMarker::UI_PASS);

	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, fb_uniform_set, BASE_UNIFORM_SET);
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, state.default_transforms_uniform_set, TRANSFORMS_UNIFORM_SET);
	gdgs_temp_diag_first_clipped_preserve_rect_base_uniform_set = fb_uniform_set.is_valid() ? fb_uniform_set.get_id() : 0;
	gdgs_temp_diag_first_clipped_preserve_rect_transforms_uniform_set = state.default_transforms_uniform_set.is_valid() ? state.default_transforms_uniform_set.get_id() : 0;

	const int gdgs_temp_diag_rect_preserve_clip_cap = gdgs_debug_env_int_or("GODOT_GDGS_TEMP_DIAG_CLIPPED_PRESERVE_RECT_CAP", -1);
	const bool gdgs_temp_diag_rect_preserve_clip_only_first = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_CLIPPED_PRESERVE_RECT_ONLY_FIRST");
	const bool gdgs_temp_diag_rect_preserve_clip_skip_first = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_CLIPPED_PRESERVE_RECT_SKIP_FIRST");
	const bool gdgs_temp_diag_rect_preserve_clip_gate_active = gdgs_temp_diag_rect_preserve_clip_cap >= 0 || gdgs_temp_diag_rect_preserve_clip_only_first || gdgs_temp_diag_rect_preserve_clip_skip_first;
	const bool gdgs_temp_diag_first_rect_trace = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE");
	const bool gdgs_temp_diag_first_rect_trace_scissor = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_SCISSOR");
	const bool gdgs_temp_diag_first_rect_trace_uniform_pipeline = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_UNIFORM_PIPELINE");
	const bool gdgs_temp_diag_first_rect_trace_draw_binding = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_DRAW_BINDING");
	const bool gdgs_temp_diag_first_rect_trace_uniform_bind = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_UNIFORM_BIND");
	const bool gdgs_temp_diag_first_rect_trace_pipeline_bind = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_PIPELINE_BIND");
	const bool gdgs_temp_diag_first_rect_trace_blend_constants = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_BLEND_CONSTANTS");
	const bool gdgs_temp_diag_first_rect_trace_blend_recipe = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_BLEND_RECIPE");
	const bool gdgs_temp_diag_first_rect_trace_blend_recipe_selector = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_BLEND_RECIPE_SELECTOR");
	const bool gdgs_temp_diag_first_rect_trace_blend_recipe_attachment = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_BLEND_RECIPE_ATTACHMENT");
	const bool gdgs_temp_diag_first_rect_trace_blend_recipe_dynamic_state = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_BLEND_RECIPE_DYNAMIC_STATE");
	const bool gdgs_temp_diag_first_rect_src_color_premul_experiment = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_SRC_COLOR_PREMUL_EXPERIMENT");
	const bool gdgs_temp_diag_first_rect_dst_factors_zero_experiment = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_DST_FACTORS_ZERO_EXPERIMENT");
	const bool gdgs_temp_diag_first_rect_src_alpha_zero_experiment = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_SRC_ALPHA_ZERO_EXPERIMENT");
	const bool gdgs_temp_diag_first_rect_enable_blend_false_experiment = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_ENABLE_BLEND_FALSE_EXPERIMENT");
	const bool gdgs_temp_diag_first_rect_blend_ops_non_add_experiment = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_BLEND_OPS_NON_ADD_EXPERIMENT");
	const bool gdgs_temp_diag_first_rect_specialization_force_msdf_experiment = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_SPECIALIZATION_FORCE_MSDF_EXPERIMENT");
	const bool gdgs_temp_diag_first_rect_vertex_input_duplicate_format_experiment = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_VERTEX_INPUT_DUPLICATE_FORMAT_EXPERIMENT");
	const bool gdgs_temp_diag_first_rect_trace_vertex_bind = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_VERTEX_BIND");
	const bool gdgs_temp_diag_first_rect_trace_index_bind = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_INDEX_BIND");
	const bool gdgs_temp_diag_first_rect_trace_vertex_input_recipe = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_VERTEX_INPUT_RECIPE");
	const bool gdgs_temp_diag_first_rect_trace_vertex_input_binding_layout = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_VERTEX_INPUT_BINDING_LAYOUT");
	const bool gdgs_temp_diag_first_rect_trace_vertex_input_attribute_layout = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_VERTEX_INPUT_ATTRIBUTE_LAYOUT");
	const bool gdgs_temp_diag_first_rect_trace_vertex_input_provenance = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_VERTEX_INPUT_PROVENANCE");
	const bool gdgs_temp_diag_first_rect_trace_specialization_constants = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_SPECIALIZATION_CONSTANTS");
	const bool gdgs_temp_diag_first_rect_trace_specialization_constants_selector = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_SPECIALIZATION_CONSTANTS_SELECTOR");
	const bool gdgs_temp_diag_first_rect_trace_specialization_constants_packing = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_SPECIALIZATION_CONSTANTS_PACKING");
	const bool gdgs_temp_diag_first_rect_trace_specialization_constants_provenance = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_SPECIALIZATION_CONSTANTS_PROVENANCE");
	const bool gdgs_temp_diag_first_rect_trace_pipeline_layout = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_PIPELINE_LAYOUT");
	const bool gdgs_temp_diag_first_rect_trace_pipeline_layout_descriptor_shape = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_PIPELINE_LAYOUT_DESCRIPTOR_SHAPE");
	const bool gdgs_temp_diag_first_rect_trace_pipeline_layout_push_constant_shape = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_PIPELINE_LAYOUT_PUSH_CONSTANT_SHAPE");
	const bool gdgs_temp_diag_first_rect_trace_pipeline_layout_provenance = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_PIPELINE_LAYOUT_PROVENANCE");
	const bool gdgs_temp_diag_first_rect_trace_pipeline_realization = gdgs_debug_env_bool_enabled("GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_TRACE_PIPELINE_REALIZATION");
	const bool gdgs_temp_diag_first_rect_trace_any = gdgs_temp_diag_first_rect_trace || gdgs_temp_diag_first_rect_trace_scissor || gdgs_temp_diag_first_rect_trace_uniform_pipeline || gdgs_temp_diag_first_rect_trace_draw_binding || gdgs_temp_diag_first_rect_trace_uniform_bind || gdgs_temp_diag_first_rect_trace_pipeline_bind || gdgs_temp_diag_first_rect_trace_blend_constants || gdgs_temp_diag_first_rect_trace_blend_recipe || gdgs_temp_diag_first_rect_trace_blend_recipe_selector || gdgs_temp_diag_first_rect_trace_blend_recipe_attachment || gdgs_temp_diag_first_rect_trace_blend_recipe_dynamic_state || gdgs_temp_diag_first_rect_trace_vertex_bind || gdgs_temp_diag_first_rect_trace_index_bind || gdgs_temp_diag_first_rect_trace_vertex_input_recipe || gdgs_temp_diag_first_rect_trace_vertex_input_binding_layout || gdgs_temp_diag_first_rect_trace_vertex_input_attribute_layout || gdgs_temp_diag_first_rect_trace_vertex_input_provenance || gdgs_temp_diag_first_rect_trace_specialization_constants || gdgs_temp_diag_first_rect_trace_specialization_constants_selector || gdgs_temp_diag_first_rect_trace_specialization_constants_packing || gdgs_temp_diag_first_rect_trace_specialization_constants_provenance || gdgs_temp_diag_first_rect_trace_pipeline_layout || gdgs_temp_diag_first_rect_trace_pipeline_layout_descriptor_shape || gdgs_temp_diag_first_rect_trace_pipeline_layout_push_constant_shape || gdgs_temp_diag_first_rect_trace_pipeline_layout_provenance || gdgs_temp_diag_first_rect_trace_pipeline_realization;
	int gdgs_temp_diag_rect_preserve_clip_match_count = 0;
	int gdgs_temp_diag_rect_preserve_clip_skipped_count = 0;

	gdgs_temp_diag_first_clipped_preserve_rect_trace_active = gdgs_temp_diag_first_rect_trace_any;
	gdgs_temp_diag_first_clipped_preserve_rect_combined_trace_enabled = gdgs_temp_diag_first_rect_trace;
	gdgs_temp_diag_first_clipped_preserve_rect_scissor_trace_enabled = gdgs_temp_diag_first_rect_trace_scissor;
	gdgs_temp_diag_first_clipped_preserve_rect_uniform_pipeline_trace_enabled = gdgs_temp_diag_first_rect_trace_uniform_pipeline;
	gdgs_temp_diag_first_clipped_preserve_rect_draw_binding_trace_enabled = gdgs_temp_diag_first_rect_trace_draw_binding;
	gdgs_temp_diag_first_clipped_preserve_rect_uniform_bind_trace_enabled = gdgs_temp_diag_first_rect_trace_uniform_bind;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_bind_trace_enabled = gdgs_temp_diag_first_rect_trace_pipeline_bind;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_constants_trace_enabled = gdgs_temp_diag_first_rect_trace_blend_constants;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_selector_trace_enabled = gdgs_temp_diag_first_rect_trace_blend_recipe || gdgs_temp_diag_first_rect_trace_blend_recipe_selector;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_attachment_trace_enabled = gdgs_temp_diag_first_rect_trace_blend_recipe || gdgs_temp_diag_first_rect_trace_blend_recipe_attachment;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_dynamic_state_trace_enabled = gdgs_temp_diag_first_rect_trace_blend_recipe || gdgs_temp_diag_first_rect_trace_blend_recipe_dynamic_state;
	gdgs_temp_diag_first_clipped_preserve_rect_src_color_premul_experiment_enabled = gdgs_temp_diag_first_rect_src_color_premul_experiment;
	gdgs_temp_diag_first_clipped_preserve_rect_dst_factors_zero_experiment_enabled = gdgs_temp_diag_first_rect_dst_factors_zero_experiment;
	gdgs_temp_diag_first_clipped_preserve_rect_src_alpha_zero_experiment_enabled = gdgs_temp_diag_first_rect_src_alpha_zero_experiment;
	gdgs_temp_diag_first_clipped_preserve_rect_enable_blend_false_experiment_enabled = gdgs_temp_diag_first_rect_enable_blend_false_experiment;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_ops_non_add_experiment_enabled = gdgs_temp_diag_first_rect_blend_ops_non_add_experiment;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_force_msdf_experiment_enabled = gdgs_temp_diag_first_rect_specialization_force_msdf_experiment;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_duplicate_format_experiment_enabled = gdgs_temp_diag_first_rect_vertex_input_duplicate_format_experiment;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_bind_trace_enabled = gdgs_temp_diag_first_rect_trace_vertex_bind;
	gdgs_temp_diag_first_clipped_preserve_rect_index_bind_trace_enabled = gdgs_temp_diag_first_rect_trace_index_bind;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_binding_layout_trace_enabled = gdgs_temp_diag_first_rect_trace_vertex_input_recipe || gdgs_temp_diag_first_rect_trace_vertex_input_binding_layout || gdgs_temp_diag_first_rect_vertex_input_duplicate_format_experiment;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_attribute_layout_trace_enabled = gdgs_temp_diag_first_rect_trace_vertex_input_recipe || gdgs_temp_diag_first_rect_trace_vertex_input_attribute_layout || gdgs_temp_diag_first_rect_vertex_input_duplicate_format_experiment;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_provenance_trace_enabled = gdgs_temp_diag_first_rect_trace_vertex_input_recipe || gdgs_temp_diag_first_rect_trace_vertex_input_provenance || gdgs_temp_diag_first_rect_vertex_input_duplicate_format_experiment;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_selector_trace_enabled = gdgs_temp_diag_first_rect_trace_specialization_constants || gdgs_temp_diag_first_rect_trace_specialization_constants_selector || gdgs_temp_diag_first_rect_specialization_force_msdf_experiment;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_packing_trace_enabled = gdgs_temp_diag_first_rect_trace_specialization_constants || gdgs_temp_diag_first_rect_trace_specialization_constants_packing || gdgs_temp_diag_first_rect_specialization_force_msdf_experiment;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_provenance_trace_enabled = gdgs_temp_diag_first_rect_trace_specialization_constants || gdgs_temp_diag_first_rect_trace_specialization_constants_provenance || gdgs_temp_diag_first_rect_specialization_force_msdf_experiment;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_descriptor_shape_trace_enabled = gdgs_temp_diag_first_rect_trace_pipeline_layout || gdgs_temp_diag_first_rect_trace_pipeline_layout_descriptor_shape;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_push_constant_shape_trace_enabled = gdgs_temp_diag_first_rect_trace_pipeline_layout || gdgs_temp_diag_first_rect_trace_pipeline_layout_push_constant_shape;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_trace_enabled = gdgs_temp_diag_first_rect_trace_pipeline_layout || gdgs_temp_diag_first_rect_trace_pipeline_layout_provenance;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_trace_enabled = gdgs_temp_diag_first_rect_trace_pipeline_realization;
	gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr = nullptr;
	gdgs_temp_diag_first_clipped_preserve_rect_batch_index = -1;
	gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal = -1;
	gdgs_temp_diag_first_clipped_preserve_rect_scissor_changed = false;
	gdgs_temp_diag_first_clipped_preserve_rect_scissor_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_scissor_rect = Rect2();
	gdgs_temp_diag_first_clipped_preserve_rect_scissor_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_base_uniform_set = 0;
	gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set = 0;
	gdgs_temp_diag_first_clipped_preserve_rect_transforms_uniform_set = 0;
	gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set = 0;
	gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set_rebound = false;
	gdgs_temp_diag_first_clipped_preserve_rect_uniform_pipeline_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_draw_binding_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_uniform_bind_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_bind_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_constants_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_selector_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_attachment_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_dynamic_state_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_bind_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_index_bind_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_binding_layout_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_attribute_layout_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_provenance_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_selector_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_packing_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_provenance_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_descriptor_shape_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_push_constant_shape_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_request_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_result_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_bind_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_context_reached = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_shader_version_valid = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_shader_rid_valid = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_uses_default_shader = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_ubershader = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_pipeline_hash = 0;
	gdgs_temp_diag_first_clipped_preserve_rect_shader_blend_mode = RendererRD::MaterialStorage::ShaderData::BLEND_MODE_DISABLED;
	gdgs_temp_diag_first_clipped_preserve_rect_prereq_logged = false;

	if (gdgs_temp_diag_rect_preserve_clip_gate_active || gdgs_temp_diag_first_rect_trace_any || gdgs_temp_diag_first_rect_src_color_premul_experiment || gdgs_temp_diag_first_rect_dst_factors_zero_experiment || gdgs_temp_diag_first_rect_src_alpha_zero_experiment || gdgs_temp_diag_first_rect_enable_blend_false_experiment || gdgs_temp_diag_first_rect_blend_ops_non_add_experiment || gdgs_temp_diag_first_rect_specialization_force_msdf_experiment || gdgs_temp_diag_first_rect_vertex_input_duplicate_format_experiment) {
		print_line(vformat("[gdgs-canvas] temp_diag_clipped_preserve_rect_gate={cap=%d,only_first=%s,skip_first=%s,first_trace=%s,trace_scissor=%s,trace_uniform_pipeline=%s,trace_draw_binding=%s,trace_uniform_bind=%s,trace_pipeline_bind=%s,trace_blend_constants=%s,trace_blend_recipe=%s,trace_blend_recipe_selector=%s,trace_blend_recipe_attachment=%s,trace_blend_recipe_dynamic_state=%s,trace_vertex_bind=%s,trace_index_bind=%s,trace_vertex_input_recipe=%s,trace_vertex_input_binding_layout=%s,trace_vertex_input_attribute_layout=%s,trace_vertex_input_provenance=%s,trace_specialization_constants=%s,trace_specialization_constants_selector=%s,trace_specialization_constants_packing=%s,trace_specialization_constants_provenance=%s,trace_pipeline_layout=%s,trace_pipeline_layout_descriptor_shape=%s,trace_pipeline_layout_push_constant_shape=%s,trace_pipeline_layout_provenance=%s,trace_pipeline_realization=%s,src_color_premul_experiment=%s,dst_factors_zero_experiment=%s,src_alpha_zero_experiment=%s,enable_blend_false_experiment=%s,blend_ops_non_add_experiment=%s,specialization_force_msdf_experiment=%s,vertex_input_duplicate_format_experiment=%s}", gdgs_temp_diag_rect_preserve_clip_cap, gdgs_temp_diag_rect_preserve_clip_only_first ? "true" : "false", gdgs_temp_diag_rect_preserve_clip_skip_first ? "true" : "false", gdgs_temp_diag_first_rect_trace ? "true" : "false", gdgs_temp_diag_first_rect_trace_scissor ? "true" : "false", gdgs_temp_diag_first_rect_trace_uniform_pipeline ? "true" : "false", gdgs_temp_diag_first_rect_trace_draw_binding ? "true" : "false", gdgs_temp_diag_first_rect_trace_uniform_bind ? "true" : "false", gdgs_temp_diag_first_rect_trace_pipeline_bind ? "true" : "false", gdgs_temp_diag_first_rect_trace_blend_constants ? "true" : "false", gdgs_temp_diag_first_rect_trace_blend_recipe ? "true" : "false", gdgs_temp_diag_first_rect_trace_blend_recipe_selector ? "true" : "false", gdgs_temp_diag_first_rect_trace_blend_recipe_attachment ? "true" : "false", gdgs_temp_diag_first_rect_trace_blend_recipe_dynamic_state ? "true" : "false", gdgs_temp_diag_first_rect_trace_vertex_bind ? "true" : "false", gdgs_temp_diag_first_rect_trace_index_bind ? "true" : "false", gdgs_temp_diag_first_rect_trace_vertex_input_recipe ? "true" : "false", gdgs_temp_diag_first_rect_trace_vertex_input_binding_layout ? "true" : "false", gdgs_temp_diag_first_rect_trace_vertex_input_attribute_layout ? "true" : "false", gdgs_temp_diag_first_rect_trace_vertex_input_provenance ? "true" : "false", gdgs_temp_diag_first_rect_trace_specialization_constants ? "true" : "false", gdgs_temp_diag_first_rect_trace_specialization_constants_selector ? "true" : "false", gdgs_temp_diag_first_rect_trace_specialization_constants_packing ? "true" : "false", gdgs_temp_diag_first_rect_trace_specialization_constants_provenance ? "true" : "false", gdgs_temp_diag_first_rect_trace_pipeline_layout ? "true" : "false", gdgs_temp_diag_first_rect_trace_pipeline_layout_descriptor_shape ? "true" : "false", gdgs_temp_diag_first_rect_trace_pipeline_layout_push_constant_shape ? "true" : "false", gdgs_temp_diag_first_rect_trace_pipeline_layout_provenance ? "true" : "false", gdgs_temp_diag_first_rect_trace_pipeline_realization ? "true" : "false", gdgs_temp_diag_first_rect_src_color_premul_experiment ? "true" : "false", gdgs_temp_diag_first_rect_dst_factors_zero_experiment ? "true" : "false", gdgs_temp_diag_first_rect_src_alpha_zero_experiment ? "true" : "false", gdgs_temp_diag_first_rect_enable_blend_false_experiment ? "true" : "false", gdgs_temp_diag_first_rect_blend_ops_non_add_experiment ? "true" : "false", gdgs_temp_diag_first_rect_specialization_force_msdf_experiment ? "true" : "false", gdgs_temp_diag_first_rect_vertex_input_duplicate_format_experiment ? "true" : "false"));
	}

	Item *current_clip = nullptr;
	state.current_batch_uniform_set = RID();

	for (uint32_t i = 0; i <= state.current_batch_index; i++) {
		Batch *current_batch = &state.canvas_instance_batches[i];
		// Skipping when there is no instances.
		if (current_batch->instance_count == 0) {
			continue;
		}

		bool gdgs_temp_diag_rect_preserve_clip_match = false;
		if (gdgs_temp_diag_rect_preserve_clip_gate_active || gdgs_temp_diag_first_rect_trace_any) {
			const bool gdgs_temp_diag_rect_like = current_batch->command_type == Item::Command::TYPE_RECT || current_batch->command_type == Item::Command::TYPE_NINEPATCH;
			const CanvasShaderData *batch_shader_data = current_batch->material_data && current_batch->material_data->shader_data && current_batch->material_data->shader_data->version.is_valid() && current_batch->material_data->shader_data->is_valid() ? current_batch->material_data->shader_data : shader.default_version_data;
			const int batch_blend_mode = batch_shader_data ? batch_shader_data->blend_mode : RendererRD::MaterialStorage::ShaderData::BLEND_MODE_DISABLED;
			gdgs_temp_diag_rect_preserve_clip_match = gdgs_temp_diag_rect_like && current_batch->clip != nullptr && gdgs_canvas_blend_mode_uses_prior_color(batch_blend_mode);
		}
		if (gdgs_temp_diag_rect_preserve_clip_match) {
			gdgs_temp_diag_rect_preserve_clip_match_count++;

			bool skip_matching_batch = false;
			if (gdgs_temp_diag_rect_preserve_clip_skip_first && gdgs_temp_diag_rect_preserve_clip_match_count == 1) {
				skip_matching_batch = true;
			}
			if (gdgs_temp_diag_rect_preserve_clip_only_first && gdgs_temp_diag_rect_preserve_clip_match_count > 1) {
				skip_matching_batch = true;
			}
			if (gdgs_temp_diag_rect_preserve_clip_cap >= 0 && gdgs_temp_diag_rect_preserve_clip_match_count > gdgs_temp_diag_rect_preserve_clip_cap) {
				skip_matching_batch = true;
			}

			if (skip_matching_batch) {
				gdgs_temp_diag_rect_preserve_clip_skipped_count++;
				continue;
			}

			if (gdgs_temp_diag_first_rect_trace_any && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == nullptr) {
				const CanvasShaderData *batch_shader_data = current_batch->material_data && current_batch->material_data->shader_data && current_batch->material_data->shader_data->version.is_valid() && current_batch->material_data->shader_data->is_valid() ? current_batch->material_data->shader_data : shader.default_version_data;
				const int batch_blend_mode = batch_shader_data ? batch_shader_data->blend_mode : RendererRD::MaterialStorage::ShaderData::BLEND_MODE_DISABLED;
				String command_detail = "none";
				if (current_batch->command_type == Item::Command::TYPE_RECT && current_batch->command != nullptr) {
					const Item::CommandRect *rect = static_cast<const Item::CommandRect *>(current_batch->command);
					command_detail = vformat("{rect=%s,source=%s,flags=0x%x,texture=%s,outline=%f,px_range=%f,modulate={r=%f,g=%f,b=%f,a=%f}}", gdgs_rect2_to_string(rect->rect), gdgs_rect2_to_string(rect->source), rect->flags, gdgs_rid_to_string(rect->texture), rect->outline, rect->px_range, rect->modulate.r, rect->modulate.g, rect->modulate.b, rect->modulate.a);
				} else if (current_batch->command_type == Item::Command::TYPE_NINEPATCH && current_batch->command != nullptr) {
					const Item::CommandNinePatch *ninepatch = static_cast<const Item::CommandNinePatch *>(current_batch->command);
					command_detail = vformat("{rect=%s,source=%s,texture=%s,draw_center=%s,axis_x=%d,axis_y=%d,margins={left=%f,top=%f,right=%f,bottom=%f},color={r=%f,g=%f,b=%f,a=%f}}", gdgs_rect2_to_string(ninepatch->rect), gdgs_rect2_to_string(ninepatch->source), gdgs_rid_to_string(ninepatch->texture), ninepatch->draw_center ? "true" : "false", (int)ninepatch->axis_x, (int)ninepatch->axis_y, ninepatch->margin[0], ninepatch->margin[1], ninepatch->margin[2], ninepatch->margin[3], ninepatch->color.r, ninepatch->color.g, ninepatch->color.b, ninepatch->color.a);
				}

				gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr = current_batch;
				gdgs_temp_diag_first_clipped_preserve_rect_batch_index = i;
				gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal = gdgs_temp_diag_rect_preserve_clip_match_count;
				print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_batch={batch_index=%d,match_ordinal=%d,command_type=%s,shader_variant=%s,render_primitive=%s,instance_start=%d,instance_count=%d,clip_rect=%s,material=%s,texture=%s,blend_mode=%s,uses_prior_color=%s,has_blend=%s,use_lcd=%s,use_msdf=%s,use_lighting=%s,flags=0x%x,texpixel_size={x=%f,y=%f},command=%s}",
						i,
						gdgs_temp_diag_rect_preserve_clip_match_count,
						gdgs_canvas_command_type_name(current_batch->command_type),
						gdgs_canvas_shader_variant_name(current_batch->shader_variant),
						gdgs_canvas_render_primitive_name(current_batch->render_primitive),
						current_batch->start,
						current_batch->instance_count,
						current_batch->clip ? gdgs_rect2_to_string(current_batch->clip->final_clip_rect) : String("none"),
						gdgs_rid_to_string(current_batch->material),
						current_batch->tex_info ? gdgs_rid_to_string(current_batch->tex_info->state.texture) : String("null"),
						gdgs_canvas_blend_mode_name(batch_blend_mode),
						gdgs_canvas_blend_mode_uses_prior_color(batch_blend_mode) ? "true" : "false",
						current_batch->has_blend ? "true" : "false",
						current_batch->use_lcd ? "true" : "false",
						current_batch->use_msdf ? "true" : "false",
						current_batch->use_lighting ? "true" : "false",
						current_batch->flags,
						current_batch->tex_info ? current_batch->tex_info->texpixel_size.x : 0.0f,
						current_batch->tex_info ? current_batch->tex_info->texpixel_size.y : 0.0f,
						command_detail));
			}
		}

		const bool gdgs_scissor_changed_for_batch = current_clip != current_batch->clip;
		const bool gdgs_scissor_enabled_for_batch = current_batch->clip != nullptr;
		const Rect2 gdgs_scissor_rect_for_batch = current_batch->clip ? current_batch->clip->final_clip_rect : Rect2();

		//setup clip
		if (gdgs_scissor_changed_for_batch) {
			current_clip = current_batch->clip;
			if (current_clip) {
				RD::get_singleton()->draw_list_enable_scissor(draw_list, current_clip->final_clip_rect);
			} else {
				RD::get_singleton()->draw_list_disable_scissor(draw_list);
			}
		}
		if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_scissor_trace_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == current_batch && !gdgs_temp_diag_first_clipped_preserve_rect_scissor_bucket_logged) {
			gdgs_temp_diag_first_clipped_preserve_rect_scissor_bucket_logged = true;
			print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_scissor={batch_index=%d,match_ordinal=%d,setup_stage=scissor_reestablishment,reestablished=%s,scissor_state_after={enabled=%s,rect=%s}}",
					gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
					gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
					gdgs_scissor_changed_for_batch ? "true" : "false",
					gdgs_scissor_enabled_for_batch ? "true" : "false",
					gdgs_scissor_enabled_for_batch ? gdgs_rect2_to_string(gdgs_scissor_rect_for_batch) : String("none")));
		}

		CanvasShaderData *shader_data = shader.default_version_data;
		CanvasMaterialData *material_data = current_batch->material_data;
		RID material_uniform_set;
		if (material_data) {
			if (material_data->shader_data->version.is_valid() && material_data->shader_data->is_valid()) {
				shader_data = material_data->shader_data;
				// Update uniform set.
				material_uniform_set = texture_storage->render_target_is_using_hdr(p_to_render_target.render_target) ? material_data->uniform_set : material_data->uniform_set_srgb;
				if (material_uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(material_uniform_set)) { // Material may not have a uniform set.
					RD::get_singleton()->draw_list_bind_uniform_set(draw_list, material_uniform_set, MATERIAL_UNIFORM_SET);
					material_data->set_as_used();
				}
			}
		}

		if (gdgs_temp_diag_first_rect_trace_any && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == current_batch && !gdgs_temp_diag_first_clipped_preserve_rect_prereq_logged) {
			gdgs_temp_diag_first_clipped_preserve_rect_scissor_changed = gdgs_scissor_changed_for_batch;
			gdgs_temp_diag_first_clipped_preserve_rect_scissor_enabled = gdgs_scissor_enabled_for_batch;
			gdgs_temp_diag_first_clipped_preserve_rect_scissor_rect = gdgs_scissor_rect_for_batch;
			gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set = material_uniform_set.is_valid() ? material_uniform_set.get_id() : 0;
			gdgs_temp_diag_first_clipped_preserve_rect_shader_blend_mode = shader_data ? shader_data->blend_mode : RendererRD::MaterialStorage::ShaderData::BLEND_MODE_DISABLED;
		}

		_render_batch(draw_list, shader_data, fb_format, p_lights, current_batch, r_render_info);
	}

	if (gdgs_temp_diag_rect_preserve_clip_gate_active) {
		print_line(vformat("[gdgs-canvas] temp_diag_clipped_preserve_rect_gate_result={matched=%d,skipped=%d}", gdgs_temp_diag_rect_preserve_clip_match_count, gdgs_temp_diag_rect_preserve_clip_skipped_count));
	}
	if (gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_trace_enabled) {
		const char *gdgs_pipeline_layout_provenance_reason = gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == nullptr ? "no_matching_batch" : (gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_bucket_logged ? "emitted" : (gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_context_reached ? "context_reached_but_full_marker_missing" : "setup_block_not_reached"));
		const char *gdgs_pipeline_layout_provenance_shader_source = gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_context_reached ? (gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_uses_default_shader ? "default_canvas_shader" : "material_shader_override") : "unknown";
		const char *gdgs_pipeline_layout_provenance_binding_shape_class = gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set != 0 ? "base_material_transforms_batch" : "base_transforms_batch_without_material";
		print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_status={batch_index=%d,match_ordinal=%d,trace_enabled=true,batch_captured=%s,context_reached=%s,full_marker_emitted=%s,reason=%s,shader_source=%s,shader_version_valid=%s,shader_rid_valid=%s,pipeline_hash=%s,ubershader=%s,binding_shape_class=%s}",
				gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
				gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
				gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr != nullptr ? "true" : "false",
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_context_reached ? "true" : "false",
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_bucket_logged ? "true" : "false",
				gdgs_pipeline_layout_provenance_reason,
				gdgs_pipeline_layout_provenance_shader_source,
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_shader_version_valid ? "true" : "false",
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_shader_rid_valid ? "true" : "false",
				gdgs_u64_hex_string(gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_pipeline_hash),
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_ubershader ? "true" : "false",
				gdgs_pipeline_layout_provenance_binding_shape_class));
	}

	RD::get_singleton()->draw_list_end();

	gdgs_temp_diag_first_clipped_preserve_rect_trace_active = false;
	gdgs_temp_diag_first_clipped_preserve_rect_combined_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_scissor_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_uniform_pipeline_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_draw_binding_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_uniform_bind_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_bind_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_constants_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_selector_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_attachment_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_dynamic_state_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_src_color_premul_experiment_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_dst_factors_zero_experiment_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_src_alpha_zero_experiment_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_enable_blend_false_experiment_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_ops_non_add_experiment_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_force_msdf_experiment_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_duplicate_format_experiment_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_bind_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_index_bind_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_binding_layout_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_attribute_layout_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_provenance_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_selector_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_packing_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_provenance_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_descriptor_shape_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_push_constant_shape_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_trace_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr = nullptr;
	gdgs_temp_diag_first_clipped_preserve_rect_batch_index = -1;
	gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal = -1;
	gdgs_temp_diag_first_clipped_preserve_rect_scissor_changed = false;
	gdgs_temp_diag_first_clipped_preserve_rect_scissor_enabled = false;
	gdgs_temp_diag_first_clipped_preserve_rect_scissor_rect = Rect2();
	gdgs_temp_diag_first_clipped_preserve_rect_scissor_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_base_uniform_set = 0;
	gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set = 0;
	gdgs_temp_diag_first_clipped_preserve_rect_transforms_uniform_set = 0;
	gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set = 0;
	gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set_rebound = false;
	gdgs_temp_diag_first_clipped_preserve_rect_uniform_pipeline_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_draw_binding_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_uniform_bind_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_bind_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_constants_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_selector_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_attachment_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_dynamic_state_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_bind_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_index_bind_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_binding_layout_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_attribute_layout_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_provenance_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_selector_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_packing_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_provenance_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_descriptor_shape_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_push_constant_shape_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_request_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_result_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_bind_bucket_logged = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_context_reached = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_shader_version_valid = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_shader_rid_valid = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_uses_default_shader = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_ubershader = false;
	gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_pipeline_hash = 0;
	gdgs_temp_diag_first_clipped_preserve_rect_prereq_logged = false;
	gdgs_canvas_pipeline_realization_trace_disarm(0);
	gdgs_first_l88_render_pipeline_create_trace_disarm();
	gdgs_first_l88_post_create_trace_disarm();
	gdgs_first_l88_execution_packet_trace_disarm();

	state.current_batch_index = 0;
	state.canvas_instance_batches.clear();
}

void RendererCanvasRenderRD::_record_item_commands(const Item *p_item, RenderTarget p_render_target, const Transform2D &p_base_transform, Item *&r_current_clip, Light *p_lights, bool &r_batch_broken, bool &r_sdf_used, Batch *&r_current_batch) {
	const RSE::CanvasItemTextureFilter texture_filter = p_item->texture_filter == RSE::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT ? default_filter : p_item->texture_filter;
	const RSE::CanvasItemTextureRepeat texture_repeat = p_item->texture_repeat == RSE::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT ? default_repeat : p_item->texture_repeat;

	Transform2D base_transform = p_base_transform;

	InstanceData template_instance;
	memset(&template_instance, 0, sizeof(InstanceData));

	Transform2D draw_transform; // Used by transform command
	_update_transform_2d_to_mat2x3(base_transform, template_instance.world);

	Color base_color = p_item->final_modulate;
	bool use_linear_colors = p_render_target.use_linear_colors;
	template_instance.instance_uniforms_ofs = static_cast<uint32_t>(p_item->instance_allocated_shader_uniforms_offset);

	bool reclip = false;

	bool skipping = false;

	uint16_t light_count = 0;
	uint16_t shadow_mask = 0;

	{
		Light *light = p_lights;

		while (light) {
			if (light->render_index_cache >= 0 && p_item->light_mask & light->item_mask && p_item->z_final >= light->z_min && p_item->z_final <= light->z_max && p_item->global_rect_cache.intersects(light->rect_cache)) {
				uint32_t light_index = light->render_index_cache;
				// TODO: consider making lights a per-batch property and then baking light operations in the shader for better performance.
				template_instance.lights[light_count >> 2] |= light_index << ((light_count & 3) * 8);

				if (p_item->light_mask & light->item_shadow_mask) {
					shadow_mask |= 1 << light_count;
				}

				light_count++;

				if (light_count == MAX_LIGHTS_PER_ITEM - 1) {
					break;
				}
			}
			light = light->next_ptr;
		}

		template_instance.flags |= light_count << INSTANCE_FLAGS_LIGHT_COUNT_SHIFT;
		template_instance.flags |= shadow_mask << INSTANCE_FLAGS_SHADOW_MASKED_SHIFT;
	}

	bool use_lighting = (light_count > 0 || using_directional_lights);

	if (use_lighting != r_current_batch->use_lighting) {
		r_current_batch = _new_batch(r_batch_broken);
		r_current_batch->use_lighting = use_lighting;
	}

	const Item::Command *c = p_item->commands;
	while (c) {
		if (skipping && c->type != Item::Command::TYPE_ANIMATION_SLICE) {
			c = c->next;
			continue;
		}

		switch (c->type) {
			case Item::Command::TYPE_RECT: {
				const Item::CommandRect *rect = static_cast<const Item::CommandRect *>(c);

				// 1: If commands are different, start a new batch.
				if (r_current_batch->command_type != Item::Command::TYPE_RECT) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->command_type = Item::Command::TYPE_RECT;
					r_current_batch->command = c;
					// default variant
					r_current_batch->shader_variant = SHADER_VARIANT_QUAD;
					r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_TRIANGLES;
					r_current_batch->flags = 0;
				}

				RSE::CanvasItemTextureRepeat rect_repeat = texture_repeat;
				if (bool(rect->flags & CANVAS_RECT_TILE)) {
					rect_repeat = RSE::CanvasItemTextureRepeat::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED;
				}

				Color modulated = rect->modulate * base_color;
				if (use_linear_colors) {
					modulated = modulated.srgb_to_linear();
				}

				bool has_blend = bool(rect->flags & CANVAS_RECT_LCD);
				// Start a new batch if the blend mode has changed,
				// or blend mode is enabled and the modulation has changed.
				if (has_blend != r_current_batch->has_blend || (has_blend && modulated != r_current_batch->modulate)) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->has_blend = has_blend;
					r_current_batch->modulate = modulated;
					r_current_batch->shader_variant = SHADER_VARIANT_QUAD;
					r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_TRIANGLES;
				}

				bool has_msdf = bool(rect->flags & CANVAS_RECT_MSDF);
				TextureState tex_state(rect->texture, texture_filter, rect_repeat, has_msdf, use_linear_colors);
				TextureInfo *tex_info = texture_info_map.getptr(tex_state);
				if (!tex_info) {
					tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
					_prepare_batch_texture_info(rect->texture, tex_state, tex_info);
				}

				if (has_msdf != r_current_batch->use_msdf || rect->px_range != r_current_batch->msdf_pix_range || rect->outline != r_current_batch->msdf_outline) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->use_msdf = has_msdf;
					r_current_batch->msdf_pix_range = rect->px_range;
					r_current_batch->msdf_outline = rect->outline;
				}

				bool has_lcd = bool(rect->flags & CANVAS_RECT_LCD);
				if (has_lcd != r_current_batch->use_lcd) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->use_lcd = has_lcd;
				}

				if (r_current_batch->tex_info != tex_info) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->tex_info = tex_info;
				}

				InstanceData *instance_data = new_instance_data(*r_current_batch, template_instance);
				Rect2 src_rect;
				Rect2 dst_rect;

				if (rect->texture.is_valid()) {
					src_rect = (rect->flags & CANVAS_RECT_REGION) ? Rect2(rect->source.position * tex_info->texpixel_size, rect->source.size * tex_info->texpixel_size) : Rect2(0, 0, 1, 1);
					dst_rect = Rect2(rect->rect.position, rect->rect.size);

					if (dst_rect.size.width < 0) {
						dst_rect.position.x += dst_rect.size.width;
						dst_rect.size.width *= -1;
					}
					if (dst_rect.size.height < 0) {
						dst_rect.position.y += dst_rect.size.height;
						dst_rect.size.height *= -1;
					}

					if (rect->flags & CANVAS_RECT_FLIP_H) {
						src_rect.size.x *= -1;
					}

					if (rect->flags & CANVAS_RECT_FLIP_V) {
						src_rect.size.y *= -1;
					}

					if (rect->flags & CANVAS_RECT_TRANSPOSE) {
						instance_data->flags |= INSTANCE_FLAGS_TRANSPOSE_RECT;
					}

					if (rect->flags & CANVAS_RECT_CLIP_UV) {
						instance_data->flags |= INSTANCE_FLAGS_CLIP_RECT_UV;
					}

				} else {
					dst_rect = Rect2(rect->rect.position, rect->rect.size);

					if (dst_rect.size.width < 0) {
						dst_rect.position.x += dst_rect.size.width;
						dst_rect.size.width *= -1;
					}
					if (dst_rect.size.height < 0) {
						dst_rect.position.y += dst_rect.size.height;
						dst_rect.size.height *= -1;
					}

					src_rect = Rect2(0, 0, 1, 1);
				}

				instance_data->modulation[0] = modulated.r;
				instance_data->modulation[1] = modulated.g;
				instance_data->modulation[2] = modulated.b;
				instance_data->modulation[3] = modulated.a;

				instance_data->src_rect[0] = src_rect.position.x;
				instance_data->src_rect[1] = src_rect.position.y;
				instance_data->src_rect[2] = src_rect.size.width;
				instance_data->src_rect[3] = src_rect.size.height;

				instance_data->dst_rect[0] = dst_rect.position.x;
				instance_data->dst_rect[1] = dst_rect.position.y;
				instance_data->dst_rect[2] = dst_rect.size.width;
				instance_data->dst_rect[3] = dst_rect.size.height;

				_add_to_batch(r_batch_broken, r_current_batch);
			} break;

			case Item::Command::TYPE_NINEPATCH: {
				const Item::CommandNinePatch *np = static_cast<const Item::CommandNinePatch *>(c);

				if (r_current_batch->command_type != Item::Command::TYPE_NINEPATCH) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->command_type = Item::Command::TYPE_NINEPATCH;
					r_current_batch->command = c;
					r_current_batch->has_blend = false;
					r_current_batch->shader_variant = SHADER_VARIANT_NINEPATCH;
					r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_TRIANGLES;
					r_current_batch->flags = 0;
					r_current_batch->use_msdf = false;
					r_current_batch->use_lcd = false;
				}

				TextureState tex_state(np->texture, texture_filter, texture_repeat, false, use_linear_colors);
				TextureInfo *tex_info = texture_info_map.getptr(tex_state);
				if (!tex_info) {
					tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
					_prepare_batch_texture_info(np->texture, tex_state, tex_info);
				}

				if (r_current_batch->tex_info != tex_info) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->tex_info = tex_info;
				}

				InstanceData *instance_data = new_instance_data(*r_current_batch, template_instance);

				Rect2 src_rect;
				Rect2 dst_rect(np->rect.position.x, np->rect.position.y, np->rect.size.x, np->rect.size.y);

				if (np->texture.is_valid() && np->source != Rect2()) {
					src_rect = Rect2(np->source.position.x * tex_info->texpixel_size.width, np->source.position.y * tex_info->texpixel_size.height, np->source.size.x * tex_info->texpixel_size.width, np->source.size.y * tex_info->texpixel_size.height);
					instance_data->ninepatch_pixel_size[0] = 1.0 / np->source.size.width;
					instance_data->ninepatch_pixel_size[1] = 1.0 / np->source.size.height;
				} else {
					src_rect = Rect2(0, 0, 1, 1);
					// Set the default ninepatch pixel size to the full texture size.
					instance_data->ninepatch_pixel_size[0] = tex_info->texpixel_size.width;
					instance_data->ninepatch_pixel_size[1] = tex_info->texpixel_size.height;
				}

				Color modulated = np->color * base_color;
				if (use_linear_colors) {
					modulated = modulated.srgb_to_linear();
				}

				instance_data->modulation[0] = modulated.r;
				instance_data->modulation[1] = modulated.g;
				instance_data->modulation[2] = modulated.b;
				instance_data->modulation[3] = modulated.a;

				instance_data->src_rect[0] = src_rect.position.x;
				instance_data->src_rect[1] = src_rect.position.y;
				instance_data->src_rect[2] = src_rect.size.width;
				instance_data->src_rect[3] = src_rect.size.height;

				instance_data->dst_rect[0] = dst_rect.position.x;
				instance_data->dst_rect[1] = dst_rect.position.y;
				instance_data->dst_rect[2] = dst_rect.size.width;
				instance_data->dst_rect[3] = dst_rect.size.height;

				instance_data->flags |= int(np->axis_x) << INSTANCE_FLAGS_NINEPATCH_H_MODE_SHIFT;
				instance_data->flags |= int(np->axis_y) << INSTANCE_FLAGS_NINEPATCH_V_MODE_SHIFT;

				if (np->draw_center) {
					instance_data->flags |= INSTANCE_FLAGS_NINEPACH_DRAW_CENTER;
				}

				instance_data->ninepatch_margins[0] = np->margin[SIDE_LEFT];
				instance_data->ninepatch_margins[1] = np->margin[SIDE_TOP];
				instance_data->ninepatch_margins[2] = np->margin[SIDE_RIGHT];
				instance_data->ninepatch_margins[3] = np->margin[SIDE_BOTTOM];

				_add_to_batch(r_batch_broken, r_current_batch);
			} break;

			case Item::Command::TYPE_POLYGON: {
				const Item::CommandPolygon *polygon = static_cast<const Item::CommandPolygon *>(c);

				// Polygon's can't be batched, so always create a new batch
				r_current_batch = _new_batch(r_batch_broken);

				r_current_batch->command_type = Item::Command::TYPE_POLYGON;
				r_current_batch->has_blend = false;
				r_current_batch->command = c;
				r_current_batch->flags = 0;
				r_current_batch->use_msdf = false;
				r_current_batch->use_lcd = false;

				TextureState tex_state(polygon->texture, texture_filter, texture_repeat, false, use_linear_colors);
				TextureInfo *tex_info = texture_info_map.getptr(tex_state);
				if (!tex_info) {
					tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
					_prepare_batch_texture_info(polygon->texture, tex_state, tex_info);
				}

				if (r_current_batch->tex_info != tex_info) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->tex_info = tex_info;
				}

				// pipeline variant
				{
					ERR_CONTINUE(polygon->primitive < 0 || polygon->primitive >= RSE::PRIMITIVE_MAX);
					r_current_batch->shader_variant = polygon->primitive == RSE::PRIMITIVE_POINTS ? SHADER_VARIANT_ATTRIBUTES_POINTS : SHADER_VARIANT_ATTRIBUTES;
					r_current_batch->render_primitive = _primitive_type_to_render_primitive(polygon->primitive);
				}

				InstanceData *instance_data = new_instance_data(*r_current_batch, template_instance, true);

				Color color = base_color;
				if (use_linear_colors) {
					color = color.srgb_to_linear();
				}

				instance_data->modulation[0] = color.r;
				instance_data->modulation[1] = color.g;
				instance_data->modulation[2] = color.b;
				instance_data->modulation[3] = color.a;
			} break;

			case Item::Command::TYPE_PRIMITIVE: {
				const Item::CommandPrimitive *primitive = static_cast<const Item::CommandPrimitive *>(c);

				if (primitive->point_count != r_current_batch->primitive_points || r_current_batch->command_type != Item::Command::TYPE_PRIMITIVE) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->command_type = Item::Command::TYPE_PRIMITIVE;
					r_current_batch->has_blend = false;
					r_current_batch->command = c;
					r_current_batch->primitive_points = primitive->point_count;
					r_current_batch->flags = 0;

					ERR_CONTINUE(primitive->point_count == 0 || primitive->point_count > 4);

					switch (primitive->point_count) {
						case 1:
							r_current_batch->shader_variant = SHADER_VARIANT_PRIMITIVE_POINTS;
							r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_POINTS;
							break;
						case 2:
							r_current_batch->shader_variant = SHADER_VARIANT_PRIMITIVE;
							r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_LINES;
							break;
						case 3:
						case 4:
							r_current_batch->shader_variant = SHADER_VARIANT_PRIMITIVE;
							r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_TRIANGLES;
							break;
						default:
							// Unknown point count.
							break;
					}
				}

				TextureState tex_state(primitive->texture, texture_filter, texture_repeat, false, use_linear_colors);
				TextureInfo *tex_info = texture_info_map.getptr(tex_state);
				if (!tex_info) {
					tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
					_prepare_batch_texture_info(primitive->texture, tex_state, tex_info);
				}

				if (r_current_batch->tex_info != tex_info) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->tex_info = tex_info;
				}

				InstanceData *instance_data = new_instance_data(*r_current_batch, template_instance);

				for (uint32_t j = 0; j < MIN(3u, primitive->point_count); j++) {
					instance_data->points[j * 2 + 0] = primitive->points[j].x;
					instance_data->points[j * 2 + 1] = primitive->points[j].y;
					instance_data->uvs[j * 2 + 0] = primitive->uvs[j].x;
					instance_data->uvs[j * 2 + 1] = primitive->uvs[j].y;
					Color col = primitive->colors[j] * base_color;
					if (use_linear_colors) {
						col = col.srgb_to_linear();
					}
					instance_data->colors[j * 2 + 0] = (uint32_t(Math::make_half_float(col.g)) << 16) | Math::make_half_float(col.r);
					instance_data->colors[j * 2 + 1] = (uint32_t(Math::make_half_float(col.a)) << 16) | Math::make_half_float(col.b);
				}

				_add_to_batch(r_batch_broken, r_current_batch);

				if (primitive->point_count == 4) {
					instance_data = new_instance_data(*r_current_batch, template_instance);

					for (uint32_t j = 0; j < 3; j++) {
						int offset = j == 0 ? 0 : 1;
						// Second triangle in the quad. Uses vertices 0, 2, 3.
						instance_data->points[j * 2 + 0] = primitive->points[j + offset].x;
						instance_data->points[j * 2 + 1] = primitive->points[j + offset].y;
						instance_data->uvs[j * 2 + 0] = primitive->uvs[j + offset].x;
						instance_data->uvs[j * 2 + 1] = primitive->uvs[j + offset].y;
						Color col = primitive->colors[j + offset] * base_color;
						if (use_linear_colors) {
							col = col.srgb_to_linear();
						}
						instance_data->colors[j * 2 + 0] = (uint32_t(Math::make_half_float(col.g)) << 16) | Math::make_half_float(col.r);
						instance_data->colors[j * 2 + 1] = (uint32_t(Math::make_half_float(col.a)) << 16) | Math::make_half_float(col.b);
					}

					_add_to_batch(r_batch_broken, r_current_batch);
				}
			} break;

			case Item::Command::TYPE_MESH:
			case Item::Command::TYPE_MULTIMESH:
			case Item::Command::TYPE_PARTICLES: {
				// Mesh's can't be batched, so always create a new batch
				r_current_batch = _new_batch(r_batch_broken);
				r_current_batch->command = c;
				r_current_batch->command_type = c->type;
				r_current_batch->has_blend = false;
				r_current_batch->flags = 0;
				r_current_batch->use_msdf = false;
				r_current_batch->use_lcd = false;

				InstanceData *instance_data = nullptr;

				Color modulate(1, 1, 1, 1);
				if (c->type == Item::Command::TYPE_MESH) {
					const Item::CommandMesh *m = static_cast<const Item::CommandMesh *>(c);
					TextureState tex_state(m->texture, texture_filter, texture_repeat, false, use_linear_colors);
					TextureInfo *tex_info = texture_info_map.getptr(tex_state);
					if (!tex_info) {
						tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
						_prepare_batch_texture_info(m->texture, tex_state, tex_info);
					}
					r_current_batch->tex_info = tex_info;
					instance_data = new_instance_data(*r_current_batch, template_instance, true);

					r_current_batch->mesh_instance_count = 1;
					_update_transform_2d_to_mat2x3(base_transform * draw_transform * m->transform, instance_data->world);
					modulate = m->modulate;
				} else if (c->type == Item::Command::TYPE_MULTIMESH) {
					RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();

					const Item::CommandMultiMesh *mm = static_cast<const Item::CommandMultiMesh *>(c);
					RID multimesh = mm->multimesh;

					if (mesh_storage->multimesh_get_transform_format(multimesh) != RSE::MULTIMESH_TRANSFORM_2D) {
						break;
					}

					r_current_batch->mesh_instance_count = mesh_storage->multimesh_get_instances_to_draw(multimesh);
					if (r_current_batch->mesh_instance_count == 0) {
						break;
					}

					TextureState tex_state(mm->texture, texture_filter, texture_repeat, false, use_linear_colors);
					TextureInfo *tex_info = texture_info_map.getptr(tex_state);
					if (!tex_info) {
						tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
						_prepare_batch_texture_info(mm->texture, tex_state, tex_info);
					}
					r_current_batch->tex_info = tex_info;
					instance_data = new_instance_data(*r_current_batch, template_instance, true);

					r_current_batch->flags |= 1; // multimesh, trails disabled

					if (mesh_storage->multimesh_uses_colors(mm->multimesh)) {
						r_current_batch->flags |= BATCH_FLAGS_INSTANCING_HAS_COLORS;
					}
					if (mesh_storage->multimesh_uses_custom_data(mm->multimesh)) {
						r_current_batch->flags |= BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA;
					}
				} else if (c->type == Item::Command::TYPE_PARTICLES) {
					RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
					RendererRD::ParticlesStorage *particles_storage = RendererRD::ParticlesStorage::get_singleton();

					const Item::CommandParticles *pt = static_cast<const Item::CommandParticles *>(c);
					TextureState tex_state(pt->texture, texture_filter, texture_repeat, false, use_linear_colors);
					TextureInfo *tex_info = texture_info_map.getptr(tex_state);
					if (!tex_info) {
						tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
						_prepare_batch_texture_info(pt->texture, tex_state, tex_info);
					}
					r_current_batch->tex_info = tex_info;
					instance_data = new_instance_data(*r_current_batch, template_instance, true);

					uint32_t divisor = 1;
					r_current_batch->mesh_instance_count = particles_storage->particles_get_amount(pt->particles, divisor);
					r_current_batch->flags |= (divisor & BATCH_FLAGS_INSTANCING_MASK);
					r_current_batch->mesh_instance_count /= divisor;

					RID particles = pt->particles;

					r_current_batch->flags |= BATCH_FLAGS_INSTANCING_HAS_COLORS;
					r_current_batch->flags |= BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA;

					if (particles_storage->particles_has_collision(particles) && texture_storage->render_target_is_sdf_enabled(p_render_target.render_target)) {
						// Pass collision information.
						Transform2D xform = p_item->final_transform;

						RID sdf_texture = texture_storage->render_target_get_sdf_texture(p_render_target.render_target);

						Rect2 to_screen;
						{
							Rect2 sdf_rect = texture_storage->render_target_get_sdf_rect(p_render_target.render_target);

							to_screen.size = Vector2(1.0 / sdf_rect.size.width, 1.0 / sdf_rect.size.height);
							to_screen.position = -sdf_rect.position * to_screen.size;
						}

						particles_storage->particles_set_canvas_sdf_collision(pt->particles, true, xform, to_screen, sdf_texture);
					} else {
						particles_storage->particles_set_canvas_sdf_collision(pt->particles, false, Transform2D(), Rect2(), RID());
					}
					r_sdf_used |= particles_storage->particles_has_collision(particles);
				}

				Color modulated = modulate * base_color;
				if (use_linear_colors) {
					modulated = modulated.srgb_to_linear();
				}

				instance_data->modulation[0] = modulated.r;
				instance_data->modulation[1] = modulated.g;
				instance_data->modulation[2] = modulated.b;
				instance_data->modulation[3] = modulated.a;
			} break;

			case Item::Command::TYPE_TRANSFORM: {
				const Item::CommandTransform *transform = static_cast<const Item::CommandTransform *>(c);
				draw_transform = transform->xform;
				_update_transform_2d_to_mat2x3(base_transform * transform->xform, template_instance.world);
			} break;

			case Item::Command::TYPE_CLIP_IGNORE: {
				const Item::CommandClipIgnore *ci = static_cast<const Item::CommandClipIgnore *>(c);
				if (r_current_clip) {
					if (ci->ignore != reclip) {
						r_current_batch = _new_batch(r_batch_broken);
						if (ci->ignore) {
							r_current_batch->clip = nullptr;
							reclip = true;
						} else {
							r_current_batch->clip = r_current_clip;
							reclip = false;
						}
					}
				}
			} break;

			case Item::Command::TYPE_ANIMATION_SLICE: {
				const Item::CommandAnimationSlice *as = static_cast<const Item::CommandAnimationSlice *>(c);
				double current_time = RSG::rasterizer->get_total_time();
				double local_time = Math::fposmod(current_time - as->offset, as->animation_length);
				skipping = !(local_time >= as->slice_begin && local_time < as->slice_end);

				RenderingServerDefault::redraw_request(); // animation visible means redraw request
			} break;
		}

		c = c->next;
		r_batch_broken = false;
	}

#ifdef DEBUG_ENABLED
	if (debug_redraw && p_item->debug_redraw_time > 0.0) {
		Color dc = debug_redraw_color;
		dc.a *= p_item->debug_redraw_time / debug_redraw_time;

		// 1: If commands are different, start a new batch.
		if (r_current_batch->command_type != Item::Command::TYPE_RECT) {
			r_current_batch = _new_batch(r_batch_broken);
			r_current_batch->command_type = Item::Command::TYPE_RECT;
			// it is ok to be null for a TYPE_RECT
			r_current_batch->command = nullptr;
			// default variant
			r_current_batch->shader_variant = SHADER_VARIANT_QUAD;
			r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_TRIANGLES;
			r_current_batch->flags = 0;
		}

		// 2: If the current batch has lighting, start a new batch.
		if (r_current_batch->use_lighting) {
			r_current_batch = _new_batch(r_batch_broken);
			r_current_batch->use_lighting = false;
		}

		// 3: If the current batch has blend, start a new batch.
		if (r_current_batch->has_blend) {
			r_current_batch = _new_batch(r_batch_broken);
			r_current_batch->has_blend = false;
		}

		TextureState tex_state(default_canvas_texture, texture_filter, texture_repeat, false, use_linear_colors);
		TextureInfo *tex_info = texture_info_map.getptr(tex_state);
		if (!tex_info) {
			tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
			_prepare_batch_texture_info(default_canvas_texture, tex_state, tex_info);
		}

		if (r_current_batch->tex_info != tex_info) {
			r_current_batch = _new_batch(r_batch_broken);
			r_current_batch->tex_info = tex_info;
		}

		_update_transform_2d_to_mat2x3(base_transform, template_instance.world);
		InstanceData *instance_data = new_instance_data(*r_current_batch, template_instance);

		Rect2 src_rect;
		Rect2 dst_rect;

		dst_rect = p_item->rect;
		if (dst_rect.size.width < 0) {
			dst_rect.position.x += dst_rect.size.width;
			dst_rect.size.width *= -1;
		}
		if (dst_rect.size.height < 0) {
			dst_rect.position.y += dst_rect.size.height;
			dst_rect.size.height *= -1;
		}

		src_rect = Rect2(0, 0, 1, 1);

		instance_data->modulation[0] = dc.r;
		instance_data->modulation[1] = dc.g;
		instance_data->modulation[2] = dc.b;
		instance_data->modulation[3] = dc.a;

		instance_data->src_rect[0] = src_rect.position.x;
		instance_data->src_rect[1] = src_rect.position.y;
		instance_data->src_rect[2] = src_rect.size.width;
		instance_data->src_rect[3] = src_rect.size.height;

		instance_data->dst_rect[0] = dst_rect.position.x;
		instance_data->dst_rect[1] = dst_rect.position.y;
		instance_data->dst_rect[2] = dst_rect.size.width;
		instance_data->dst_rect[3] = dst_rect.size.height;

		_add_to_batch(r_batch_broken, r_current_batch);

		p_item->debug_redraw_time -= RSG::rasterizer->get_frame_delta_time();

		RenderingServerDefault::redraw_request();

		r_batch_broken = false;
	}
#endif

	if (r_current_clip && reclip) {
		// will make it re-enable clipping if needed afterwards
		r_current_clip = nullptr;
	}
}

void RendererCanvasRenderRD::_before_evict(RendererCanvasRenderRD::RIDSetKey &p_key, RID &p_rid) {
	RD::get_singleton()->uniform_set_set_invalidation_callback(p_rid, nullptr, nullptr);
	RD::get_singleton()->free_rid(p_rid);
}

void RendererCanvasRenderRD::_uniform_set_invalidation_callback(void *p_userdata) {
	const RIDSetKey *key = static_cast<RIDSetKey *>(p_userdata);
	static_cast<RendererCanvasRenderRD *>(singleton)->rid_set_to_uniform_set.erase(*key);
}

void RendererCanvasRenderRD::_canvas_texture_invalidation_callback(bool p_deleted, void *p_userdata) {
	KeyValue<RID, TightLocalVector<RID>> *kv = static_cast<KeyValue<RID, TightLocalVector<RID>> *>(p_userdata);
	RD *rd = RD::get_singleton();
	for (RID rid : kv->value) {
		// The invalidation callback will also take care of clearing rid_set_to_uniform_set cache.
		rd->free_rid(rid);
	}
	kv->value.clear();
	if (p_deleted) {
		static_cast<RendererCanvasRenderRD *>(singleton)->canvas_texture_to_uniform_set.erase(kv->key);
	}
}

void RendererCanvasRenderRD::_render_batch(RD::DrawListID p_draw_list, CanvasShaderData *p_shader_data, RenderingDevice::FramebufferFormatID p_framebuffer_format, Light *p_lights, Batch const *p_batch, RenderingServerTypes::RenderInfo *r_render_info) {
	{
		RendererRD::TextureStorage *ts = RendererRD::TextureStorage::get_singleton();

		RIDSetKey key(p_batch->tex_info->state);

		const RID *uniform_set = rid_set_to_uniform_set.getptr(key);
		if (uniform_set == nullptr) {
			RD::Uniform *uniform_ptrw = state.batch_texture_uniforms.ptrw();
			uniform_ptrw[0] = RD::Uniform(RD::UNIFORM_TYPE_TEXTURE, 0, p_batch->tex_info->diffuse);
			uniform_ptrw[1] = RD::Uniform(RD::UNIFORM_TYPE_TEXTURE, 1, p_batch->tex_info->normal);
			uniform_ptrw[2] = RD::Uniform(RD::UNIFORM_TYPE_TEXTURE, 2, p_batch->tex_info->specular);
			uniform_ptrw[3] = RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, 3, p_batch->tex_info->sampler);

			RID rid = RD::get_singleton()->uniform_set_create(state.batch_texture_uniforms, shader.default_version_rd_shader, BATCH_UNIFORM_SET);
			ERR_FAIL_COND_MSG(rid.is_null(), "Failed to create uniform set for batch.");

			const RIDCache::Pair *iter = rid_set_to_uniform_set.insert(key, rid);
			uniform_set = &iter->data;
			RD::get_singleton()->uniform_set_set_invalidation_callback(rid, RendererCanvasRenderRD::_uniform_set_invalidation_callback, (void *)&iter->key);

			// If this is a CanvasTexture, it must be tracked so that any changes to the diffuse, normal,
			// or specular channels invalidate all associated uniform sets.
			if (ts->owns_canvas_texture(p_batch->tex_info->state.texture)) {
				KeyValue<RID, TightLocalVector<RID>> *kv = nullptr;
				if (HashMap<RID, TightLocalVector<RID>>::Iterator i = canvas_texture_to_uniform_set.find(p_batch->tex_info->state.texture); i == canvas_texture_to_uniform_set.end()) {
					kv = &*canvas_texture_to_uniform_set.insert(p_batch->tex_info->state.texture, { *uniform_set });
				} else {
					i->value.push_back(rid);
					kv = &*i;
				}
				ts->canvas_texture_set_invalidation_callback(p_batch->tex_info->state.texture, RendererCanvasRenderRD::_canvas_texture_invalidation_callback, kv);
			}
		}

		const bool gdgs_batch_uniform_set_rebound = state.current_batch_uniform_set != *uniform_set;
		if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch) {
			gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set = uniform_set->is_valid() ? uniform_set->get_id() : 0;
			gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set_rebound = gdgs_batch_uniform_set_rebound;
		}
		if (gdgs_batch_uniform_set_rebound) {
			state.current_batch_uniform_set = *uniform_set;
			RD::get_singleton()->draw_list_bind_uniform_set(p_draw_list, *uniform_set, BATCH_UNIFORM_SET);
		}
		if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_uniform_bind_trace_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch && !gdgs_temp_diag_first_clipped_preserve_rect_uniform_bind_bucket_logged) {
			gdgs_temp_diag_first_clipped_preserve_rect_uniform_bind_bucket_logged = true;
			print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_uniform_bind={batch_index=%d,match_ordinal=%d,setup_stage=batch_uniform_set_bind,material_uniform_set=%s,batch_uniform_set={rid=%s,rebound=%s,active_after=%s}}",
					gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
					gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
					gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set != 0 ? itos(gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set) : String("none"),
					gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set != 0 ? itos(gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set) : String("none"),
					gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set_rebound ? "true" : "false",
					state.current_batch_uniform_set.is_valid() ? gdgs_rid_to_string(state.current_batch_uniform_set) : String("none")));
		}
	}

	RID pipeline;
	PipelineKey pipeline_key;
	pipeline_key.framebuffer_format_id = p_framebuffer_format;
	pipeline_key.variant = p_batch->shader_variant;
	pipeline_key.render_primitive = p_batch->render_primitive;
	const bool gdgs_specialization_force_msdf_experiment_requested = gdgs_temp_diag_first_clipped_preserve_rect_specialization_force_msdf_experiment_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch;
	const bool gdgs_specialization_force_msdf_experiment_applied = gdgs_specialization_force_msdf_experiment_requested && !p_batch->use_msdf;
	pipeline_key.shader_specialization.use_lighting = p_batch->use_lighting;
	pipeline_key.shader_specialization.use_msdf = p_batch->use_msdf || gdgs_specialization_force_msdf_experiment_requested;
	pipeline_key.shader_specialization.use_lcd = p_batch->use_lcd;
	pipeline_key.lcd_blend = p_batch->has_blend;
	pipeline_key.gdgs_temp_diag_src_color_premul_experiment = gdgs_temp_diag_first_clipped_preserve_rect_src_color_premul_experiment_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch ? 1 : 0;
	pipeline_key.gdgs_temp_diag_dst_factors_zero_experiment = gdgs_temp_diag_first_clipped_preserve_rect_dst_factors_zero_experiment_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch ? 1 : 0;
	pipeline_key.gdgs_temp_diag_src_alpha_zero_experiment = gdgs_temp_diag_first_clipped_preserve_rect_src_alpha_zero_experiment_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch ? 1 : 0;
	pipeline_key.gdgs_temp_diag_enable_blend_false_experiment = gdgs_temp_diag_first_clipped_preserve_rect_enable_blend_false_experiment_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch ? 1 : 0;
	pipeline_key.gdgs_temp_diag_blend_ops_non_add_experiment = gdgs_temp_diag_first_clipped_preserve_rect_blend_ops_non_add_experiment_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch ? 1 : 0;
	pipeline_key.gdgs_temp_diag_specialization_force_msdf_experiment = gdgs_specialization_force_msdf_experiment_requested ? 1 : 0;

	switch (p_batch->command_type) {
		case Item::Command::TYPE_RECT:
		case Item::Command::TYPE_NINEPATCH: {
			PushConstant push_constant = p_batch->push_constant();
			const bool gdgs_vertex_input_duplicate_format_experiment_requested = gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_duplicate_format_experiment_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch;
			const RD::VertexFormatID gdgs_quad_vertex_format_id = gdgs_vertex_input_duplicate_format_experiment_requested ? shader.gdgs_temp_diag_quad_vertex_format_id_duplicate : shader.quad_vertex_format_id;
			const bool gdgs_vertex_input_duplicate_format_experiment_applied = gdgs_vertex_input_duplicate_format_experiment_requested && gdgs_quad_vertex_format_id != shader.quad_vertex_format_id;
			const uint32_t gdgs_vertex_input_binding_index = gdgs_vertex_input_duplicate_format_experiment_applied ? 1 : 0;
			const uint32_t gdgs_vertex_input_binding_count = gdgs_vertex_input_duplicate_format_experiment_applied ? 2 : 1;
			const char *gdgs_vertex_input_experiment_shape = gdgs_vertex_input_duplicate_format_experiment_applied ? "binding1_alias_same_buffer" : "baseline_binding0_single_buffer";

			pipeline_key.vertex_format_id = gdgs_quad_vertex_format_id;
			const int gdgs_specialization_active_field_count = (p_batch->use_lighting ? 1 : 0) + (p_batch->use_msdf ? 1 : 0) + (p_batch->use_lcd ? 1 : 0);
			const char *gdgs_specialization_selector_class = gdgs_specialization_active_field_count == 0 ? "no_specialization_flags" : (gdgs_specialization_active_field_count == 1 ? "single_specialization_flag" : (gdgs_specialization_active_field_count == 2 ? "dual_specialization_flags" : "triple_specialization_flags"));
			const RID gdgs_pipeline_realization_shader_rid = p_shader_data->get_shader(p_batch->shader_variant, pipeline_key.ubershader);
			const uint32_t gdgs_pipeline_realization_hash = pipeline_key.hash();
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_trace_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch && !gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_request_bucket_logged) {
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_request_bucket_logged = true;
				gdgs_canvas_pipeline_realization_trace_arm(gdgs_pipeline_realization_hash, gdgs_temp_diag_first_clipped_preserve_rect_batch_index, gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal);
				gdgs_first_l88_render_pipeline_create_trace_arm(gdgs_pipeline_realization_hash, gdgs_pipeline_realization_shader_rid, pipeline_key.framebuffer_format_id, pipeline_key.vertex_format_id, pipeline_key.render_primitive, 0, pipeline_key.shader_specialization.packed_0, gdgs_temp_diag_first_clipped_preserve_rect_batch_index, gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal);
				print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_pipeline_realization_request={batch_index=%d,match_ordinal=%d,setup_stage=first_l88_pipeline_request,pipeline_hash=%s,shader_variant=%s,render_primitive=%s,framebuffer_format_id=%d,vertex_format_id=%d,specialization_packed_0=0x%x,lcd_blend=%s,ubershader=%s}",
						gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
						gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
						gdgs_u64_hex_string(gdgs_pipeline_realization_hash),
						gdgs_canvas_shader_variant_name(p_batch->shader_variant),
						gdgs_canvas_render_primitive_name(p_batch->render_primitive),
						(int)pipeline_key.framebuffer_format_id,
						(int)gdgs_quad_vertex_format_id,
						pipeline_key.shader_specialization.packed_0,
						p_batch->has_blend ? "true" : "false",
						pipeline_key.ubershader ? "true" : "false"));
			}
			pipeline = _get_pipeline_specialization_or_ubershader(p_shader_data, pipeline_key, push_constant);
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_trace_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch && !gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_result_bucket_logged) {
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_result_bucket_logged = true;
				bool gdgs_pipeline_realization_target_armed = false;
				bool gdgs_pipeline_realization_create_observed = false;
				RID gdgs_pipeline_realization_created_pipeline;
				gdgs_canvas_pipeline_realization_trace_snapshot(gdgs_pipeline_realization_hash, gdgs_pipeline_realization_target_armed, gdgs_pipeline_realization_create_observed, gdgs_pipeline_realization_created_pipeline);
				(void)gdgs_pipeline_realization_target_armed;
				print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_pipeline_realization_result={batch_index=%d,match_ordinal=%d,setup_stage=first_l88_pipeline_result,pipeline_hash=%s,get_pipeline_rid=%s,create_path_observed=%s,created_pipeline_rid=%s,shader_variant=%s,render_primitive=%s,vertex_format_id=%d}",
						gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
						gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
						gdgs_u64_hex_string(gdgs_pipeline_realization_hash),
						gdgs_rid_to_string(pipeline),
						gdgs_pipeline_realization_create_observed ? "true" : "false",
						gdgs_pipeline_realization_create_observed ? gdgs_rid_to_string(gdgs_pipeline_realization_created_pipeline) : String("none"),
						gdgs_canvas_shader_variant_name(p_batch->shader_variant),
						gdgs_canvas_render_primitive_name(p_batch->render_primitive),
						(int)gdgs_quad_vertex_format_id));
			}
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch) {
				if (gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_selector_trace_enabled && !gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_selector_bucket_logged) {
					gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_selector_bucket_logged = true;
					print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_specialization_constants_selector={batch_index=%d,match_ordinal=%d,setup_stage=specialization_constants_selector,shader_variant=%s,command_type=%s,selector_class=%s,use_lighting=%s,use_msdf=%s,use_lcd=%s,active_field_count=%d,specialization_force_msdf_experiment={requested=%s,applied=%s,baseline_use_msdf=%s,experiment_use_msdf=%s,held_use_lighting=%s,held_use_lcd=%s}}",
							gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
							gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
							gdgs_canvas_shader_variant_name(p_batch->shader_variant),
							gdgs_canvas_command_type_name(p_batch->command_type),
							gdgs_specialization_selector_class,
							p_batch->use_lighting ? "true" : "false",
							p_batch->use_msdf ? "true" : "false",
							p_batch->use_lcd ? "true" : "false",
							gdgs_specialization_active_field_count,
							gdgs_specialization_force_msdf_experiment_requested ? "true" : "false",
							gdgs_specialization_force_msdf_experiment_applied ? "true" : "false",
							p_batch->use_msdf ? "true" : "false",
							pipeline_key.shader_specialization.use_msdf ? "true" : "false",
							p_batch->use_lighting ? "true" : "false",
							p_batch->use_lcd ? "true" : "false"));
				}
				if (gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_packing_trace_enabled && !gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_packing_bucket_logged) {
					gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_packing_bucket_logged = true;
					print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_specialization_constants_packing={batch_index=%d,match_ordinal=%d,setup_stage=specialization_constants_packing,constant_id=0,constant_type=int,packed_0=0x%x,baseline_packed_0=0x%x,bit_layout={use_lighting_bit=0,use_msdf_bit=1,use_lcd_bit=2},encoded_values={bit0=%d,bit1=%d,bit2=%d},specialization_force_msdf_experiment={requested=%s,applied=%s,baseline_bit1=%d,experiment_bit1=%d}}",
							gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
							gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
							pipeline_key.shader_specialization.packed_0,
							(uint32_t)(p_batch->use_lighting ? 1 : 0) | (uint32_t)(p_batch->use_msdf ? 2 : 0) | (uint32_t)(p_batch->use_lcd ? 4 : 0),
							p_batch->use_lighting ? 1 : 0,
							pipeline_key.shader_specialization.use_msdf ? 1 : 0,
							p_batch->use_lcd ? 1 : 0,
							gdgs_specialization_force_msdf_experiment_requested ? "true" : "false",
							gdgs_specialization_force_msdf_experiment_applied ? "true" : "false",
							p_batch->use_msdf ? 1 : 0,
							pipeline_key.shader_specialization.use_msdf ? 1 : 0));
				}
				if (gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_provenance_trace_enabled && !gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_provenance_bucket_logged) {
					gdgs_temp_diag_first_clipped_preserve_rect_specialization_constants_provenance_bucket_logged = true;
					print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_specialization_constants_provenance={batch_index=%d,match_ordinal=%d,setup_stage=specialization_constants_provenance,recipe_owner=PipelineKey::shader_specialization,assignment_path={use_lighting=batch.use_lighting,use_msdf=batch.use_msdf,use_lcd=batch.use_lcd},batch_fields={use_lighting=%s,use_msdf=%s,use_lcd=%s,has_blend=%s},specialization_force_msdf_experiment={requested=%s,applied=%s,baseline_use_msdf=%s,experiment_use_msdf=%s,cache_identity_flag=%u},push_constant_context={msdf_px_range=%f,msdf_outline=%f},pipeline={rid=%s,vertex_format=%d,render_primitive=%s,shader_variant=%s}}",
							gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
							gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
							p_batch->use_lighting ? "true" : "false",
							p_batch->use_msdf ? "true" : "false",
							p_batch->use_lcd ? "true" : "false",
							p_batch->has_blend ? "true" : "false",
							gdgs_specialization_force_msdf_experiment_requested ? "true" : "false",
							gdgs_specialization_force_msdf_experiment_applied ? "true" : "false",
							p_batch->use_msdf ? "true" : "false",
							pipeline_key.shader_specialization.use_msdf ? "true" : "false",
							pipeline_key.gdgs_temp_diag_specialization_force_msdf_experiment,
							push_constant.msdf[0],
							push_constant.msdf[1],
							gdgs_rid_to_string(pipeline),
							(int)gdgs_quad_vertex_format_id,
							gdgs_canvas_render_primitive_name(p_batch->render_primitive),
							gdgs_canvas_shader_variant_name(p_batch->shader_variant)));
				}
			}
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch) {
				const int gdgs_pipeline_layout_bound_set_count = 3 + (gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set != 0 ? 1 : 0);
				const char *gdgs_pipeline_layout_binding_shape_class = gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set != 0 ? "base_material_transforms_batch" : "base_transforms_batch_without_material";
				RID gdgs_pipeline_layout_shader_rid = p_shader_data->get_shader(p_batch->shader_variant, pipeline_key.ubershader);
				const bool gdgs_pipeline_layout_uses_default_shader = p_shader_data == shader.default_version_data;
				const bool gdgs_pipeline_layout_shader_version_valid = p_shader_data->version.is_valid();
				const bool gdgs_pipeline_layout_shader_rid_valid = gdgs_pipeline_layout_shader_rid.is_valid();
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_context_reached = true;
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_shader_version_valid = gdgs_pipeline_layout_shader_version_valid;
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_shader_rid_valid = gdgs_pipeline_layout_shader_rid_valid;
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_uses_default_shader = gdgs_pipeline_layout_uses_default_shader;
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_ubershader = pipeline_key.ubershader;
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_pipeline_hash = pipeline_key.hash();
				if (gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_descriptor_shape_trace_enabled && !gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_descriptor_shape_bucket_logged) {
					gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_descriptor_shape_bucket_logged = true;
					print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_pipeline_layout_descriptor_shape={batch_index=%d,match_ordinal=%d,setup_stage=pipeline_layout_descriptor_shape,shader_layout_set_count=4,actively_bound_set_count=%d,binding_shape_class=%s,descriptor_sets=[{set=0,role=base_framebuffer_scope,rid=%s},{set=1,role=material_shader_params,rid=%s,bound=%s},{set=2,role=canvas_transforms,rid=%s},{set=3,role=batch_texture_state,rid=%s,rebound=%s}]}",
							gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
							gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
							gdgs_pipeline_layout_bound_set_count,
							gdgs_pipeline_layout_binding_shape_class,
							gdgs_temp_diag_first_clipped_preserve_rect_base_uniform_set != 0 ? itos(gdgs_temp_diag_first_clipped_preserve_rect_base_uniform_set) : String("none"),
							gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set != 0 ? itos(gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set) : String("none"),
							gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set != 0 ? "true" : "false",
							gdgs_temp_diag_first_clipped_preserve_rect_transforms_uniform_set != 0 ? itos(gdgs_temp_diag_first_clipped_preserve_rect_transforms_uniform_set) : String("none"),
							gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set != 0 ? itos(gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set) : String("none"),
							gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set_rebound ? "true" : "false"));
				}
				if (gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_push_constant_shape_trace_enabled && !gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_push_constant_shape_bucket_logged) {
					gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_push_constant_shape_bucket_logged = true;
					print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_pipeline_layout_push_constant_shape={batch_index=%d,match_ordinal=%d,setup_stage=pipeline_layout_push_constant_shape,push_constant_type=PushConstant,size_bytes=%d,command_type=%s,shader_variant=%s,inline_specialization_shadow={packed_0=0x%x,zeroed_for_specialized_pipeline=%s},payload_flags={batch_flags=0x%x,use_lighting=%s,use_msdf=%s,use_lcd=%s},payload_fields={msdf_px_range=%f,msdf_outline=%f,color_texture_pixel_size={x=%f,y=%f}}}",
							gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
							gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
							(int)sizeof(PushConstant),
							gdgs_canvas_command_type_name(p_batch->command_type),
							gdgs_canvas_shader_variant_name(p_batch->shader_variant),
							push_constant.shader_specialization.packed_0,
							push_constant.shader_specialization.packed_0 == 0 ? "true" : "false",
							push_constant.batch_flags,
							p_batch->use_lighting ? "true" : "false",
							p_batch->use_msdf ? "true" : "false",
							p_batch->use_lcd ? "true" : "false",
							push_constant.msdf[0],
							push_constant.msdf[1],
							push_constant.color_texture_pixel_size[0],
							push_constant.color_texture_pixel_size[1]));
				}
				if (gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_trace_enabled && !gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_bucket_logged) {
					gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_bucket_logged = true;
					print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance={batch_index=%d,match_ordinal=%d,setup_stage=pipeline_layout_provenance,recipe_owner=CanvasShaderData::pipeline_hash_map,shader_source=%s,shader_version=%s,shader_rid=%s,pipeline_hash=%s,ubershader=%s,framebuffer_format_id=%d,vertex_format_id=%d,render_primitive=%s,shader_variant=%s,lcd_blend=%s,binding_shape_class=%s}",
							gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
							gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
							gdgs_pipeline_layout_uses_default_shader ? "default_canvas_shader" : "material_shader_override",
							gdgs_pipeline_layout_shader_version_valid ? itos(p_shader_data->version.get_id()) : String("none"),
							gdgs_rid_to_string(gdgs_pipeline_layout_shader_rid),
							gdgs_u64_hex_string(gdgs_temp_diag_first_clipped_preserve_rect_pipeline_layout_provenance_pipeline_hash),
							pipeline_key.ubershader ? "true" : "false",
							(int)pipeline_key.framebuffer_format_id,
							(int)pipeline_key.vertex_format_id,
							gdgs_canvas_render_primitive_name(p_batch->render_primitive),
							gdgs_canvas_shader_variant_name(p_batch->shader_variant),
							p_batch->has_blend ? "true" : "false",
							gdgs_pipeline_layout_binding_shape_class));
				}
			}
			const RendererRD::MaterialStorage::ShaderData::BlendMode gdgs_blend_mode_rd = RendererRD::MaterialStorage::ShaderData::BlendMode(gdgs_temp_diag_first_clipped_preserve_rect_shader_blend_mode);
			RD::PipelineColorBlendState::Attachment gdgs_blend_recipe_attachment;
			uint32_t gdgs_blend_recipe_dynamic_state_flags = 0;
			const char *gdgs_blend_recipe_branch = nullptr;
			bool gdgs_blend_recipe_src_color_premul_override_applied = false;
			bool gdgs_blend_recipe_dst_factors_zero_override_applied = false;
			bool gdgs_blend_recipe_src_alpha_zero_override_applied = false;
			bool gdgs_blend_recipe_enable_blend_false_override_applied = false;
			bool gdgs_blend_recipe_blend_ops_non_add_override_applied = false;
			gdgs_canvas_compute_blend_recipe(gdgs_blend_mode_rd, p_batch->has_blend, pipeline_key.gdgs_temp_diag_src_color_premul_experiment != 0, pipeline_key.gdgs_temp_diag_dst_factors_zero_experiment != 0, pipeline_key.gdgs_temp_diag_src_alpha_zero_experiment != 0, pipeline_key.gdgs_temp_diag_enable_blend_false_experiment != 0, pipeline_key.gdgs_temp_diag_blend_ops_non_add_experiment != 0, gdgs_blend_recipe_attachment, gdgs_blend_recipe_dynamic_state_flags, gdgs_blend_recipe_branch, gdgs_blend_recipe_src_color_premul_override_applied, gdgs_blend_recipe_dst_factors_zero_override_applied, gdgs_blend_recipe_src_alpha_zero_override_applied, gdgs_blend_recipe_enable_blend_false_override_applied, gdgs_blend_recipe_blend_ops_non_add_override_applied);
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch) {
				if (gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_selector_trace_enabled && !gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_selector_bucket_logged) {
					gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_selector_bucket_logged = true;
					print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_blend_recipe_selector={batch_index=%d,match_ordinal=%d,setup_stage=blend_recipe_selector,shader_blend_mode=%s,uses_prior_color=%s,use_lcd=%s,has_blend=%s,recipe_branch=%s,src_color_premul_experiment={requested=%s,applied=%s},dst_factors_zero_experiment={requested=%s,applied=%s},src_alpha_zero_experiment={requested=%s,applied=%s},enable_blend_false_experiment={requested=%s,applied=%s},blend_ops_non_add_experiment={requested=%s,applied=%s},pipeline={rid=%s,vertex_format=%d,render_primitive=%s,shader_variant=%s}}",
							gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
							gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
							gdgs_canvas_blend_mode_name(gdgs_temp_diag_first_clipped_preserve_rect_shader_blend_mode),
							gdgs_canvas_blend_mode_uses_prior_color(gdgs_temp_diag_first_clipped_preserve_rect_shader_blend_mode) ? "true" : "false",
							p_batch->use_lcd ? "true" : "false",
							p_batch->has_blend ? "true" : "false",
							gdgs_blend_recipe_branch,
							pipeline_key.gdgs_temp_diag_src_color_premul_experiment != 0 ? "true" : "false",
							gdgs_blend_recipe_src_color_premul_override_applied ? "true" : "false",
							pipeline_key.gdgs_temp_diag_dst_factors_zero_experiment != 0 ? "true" : "false",
							gdgs_blend_recipe_dst_factors_zero_override_applied ? "true" : "false",
							pipeline_key.gdgs_temp_diag_src_alpha_zero_experiment != 0 ? "true" : "false",
							gdgs_blend_recipe_src_alpha_zero_override_applied ? "true" : "false",
							pipeline_key.gdgs_temp_diag_enable_blend_false_experiment != 0 ? "true" : "false",
							gdgs_blend_recipe_enable_blend_false_override_applied ? "true" : "false",
							pipeline_key.gdgs_temp_diag_blend_ops_non_add_experiment != 0 ? "true" : "false",
							gdgs_blend_recipe_blend_ops_non_add_override_applied ? "true" : "false",
							gdgs_rid_to_string(pipeline),
							(int)gdgs_quad_vertex_format_id,
							gdgs_canvas_render_primitive_name(p_batch->render_primitive),
							gdgs_canvas_shader_variant_name(p_batch->shader_variant)));
				}
				if (gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_attachment_trace_enabled && !gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_attachment_bucket_logged) {
					gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_attachment_bucket_logged = true;
					print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_blend_recipe_attachment={batch_index=%d,match_ordinal=%d,setup_stage=blend_recipe_attachment,enable_blend=%s,color_op=%s,alpha_op=%s,src_color=%s,dst_color=%s,src_alpha=%s,dst_alpha=%s,src_color_premul_experiment={requested=%s,applied=%s,baseline_src_color=src_alpha,experiment_src_color=one},dst_factors_zero_experiment={requested=%s,applied=%s,baseline_dst_color=one_minus_src_alpha,baseline_dst_alpha=one_minus_src_alpha,experiment_dst_color=zero,experiment_dst_alpha=zero},src_alpha_zero_experiment={requested=%s,applied=%s,baseline_src_alpha=one,experiment_src_alpha=zero,held_src_color=one,held_dst_color=zero,held_dst_alpha=zero},enable_blend_false_experiment={requested=%s,applied=%s,baseline_enable_blend=true,experiment_enable_blend=false,held_color_op=add,held_alpha_op=add,held_src_color=one,held_dst_color=zero,held_src_alpha=zero,held_dst_alpha=zero},blend_ops_non_add_experiment={requested=%s,applied=%s,baseline_color_op=add,baseline_alpha_op=add,experiment_color_op=reverse_subtract,experiment_alpha_op=reverse_subtract,held_enable_blend=false,held_src_color=one,held_dst_color=zero,held_src_alpha=zero,held_dst_alpha=zero},write_mask={r=%s,g=%s,b=%s,a=%s}}",
							gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
							gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
							gdgs_blend_recipe_attachment.enable_blend ? "true" : "false",
							gdgs_canvas_blend_op_name(gdgs_blend_recipe_attachment.color_blend_op),
							gdgs_canvas_blend_op_name(gdgs_blend_recipe_attachment.alpha_blend_op),
							gdgs_canvas_blend_factor_name(gdgs_blend_recipe_attachment.src_color_blend_factor),
							gdgs_canvas_blend_factor_name(gdgs_blend_recipe_attachment.dst_color_blend_factor),
							gdgs_canvas_blend_factor_name(gdgs_blend_recipe_attachment.src_alpha_blend_factor),
							gdgs_canvas_blend_factor_name(gdgs_blend_recipe_attachment.dst_alpha_blend_factor),
							pipeline_key.gdgs_temp_diag_src_color_premul_experiment != 0 ? "true" : "false",
							gdgs_blend_recipe_src_color_premul_override_applied ? "true" : "false",
							pipeline_key.gdgs_temp_diag_dst_factors_zero_experiment != 0 ? "true" : "false",
							gdgs_blend_recipe_dst_factors_zero_override_applied ? "true" : "false",
							pipeline_key.gdgs_temp_diag_src_alpha_zero_experiment != 0 ? "true" : "false",
							gdgs_blend_recipe_src_alpha_zero_override_applied ? "true" : "false",
							pipeline_key.gdgs_temp_diag_enable_blend_false_experiment != 0 ? "true" : "false",
							gdgs_blend_recipe_enable_blend_false_override_applied ? "true" : "false",
							pipeline_key.gdgs_temp_diag_blend_ops_non_add_experiment != 0 ? "true" : "false",
							gdgs_blend_recipe_blend_ops_non_add_override_applied ? "true" : "false",
							gdgs_blend_recipe_attachment.write_r ? "true" : "false",
							gdgs_blend_recipe_attachment.write_g ? "true" : "false",
							gdgs_blend_recipe_attachment.write_b ? "true" : "false",
							gdgs_blend_recipe_attachment.write_a ? "true" : "false"));
				}
				if (gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_dynamic_state_trace_enabled && !gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_dynamic_state_bucket_logged) {
					gdgs_temp_diag_first_clipped_preserve_rect_blend_recipe_dynamic_state_bucket_logged = true;
					print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_blend_recipe_dynamic_state={batch_index=%d,match_ordinal=%d,setup_stage=blend_recipe_dynamic_state,dynamic_state_flags=0x%x,blend_constants_dynamic=%s,blend_constants_applied=%s,blend_constants_value=%s}",
							gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
							gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
							gdgs_blend_recipe_dynamic_state_flags,
							(gdgs_blend_recipe_dynamic_state_flags & RD::DYNAMIC_STATE_BLEND_CONSTANTS) != 0 ? "true" : "false",
							p_batch->has_blend ? "true" : "false",
							p_batch->has_blend ? vformat("{r=%f,g=%f,b=%f,a=%f}", p_batch->modulate.r, p_batch->modulate.g, p_batch->modulate.b, p_batch->modulate.a) : String("none")));
				}
			}
			RD::get_singleton()->draw_list_bind_render_pipeline(p_draw_list, pipeline);
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_trace_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch && !gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_bind_bucket_logged) {
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_realization_bind_bucket_logged = true;
				print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_pipeline_realization_bind_consume={batch_index=%d,match_ordinal=%d,setup_stage=first_l88_pipeline_bind_consume,pipeline_hash=%s,bound_pipeline_rid=%s,shader_variant=%s,render_primitive=%s,vertex_format_id=%d}",
						gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
						gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
						gdgs_u64_hex_string(gdgs_pipeline_realization_hash),
						gdgs_rid_to_string(pipeline),
						gdgs_canvas_shader_variant_name(p_batch->shader_variant),
						gdgs_canvas_render_primitive_name(p_batch->render_primitive),
						(int)gdgs_quad_vertex_format_id));
				gdgs_canvas_pipeline_realization_trace_disarm(gdgs_pipeline_realization_hash);
			}
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_pipeline_bind_trace_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch && !gdgs_temp_diag_first_clipped_preserve_rect_pipeline_bind_bucket_logged) {
				gdgs_temp_diag_first_clipped_preserve_rect_pipeline_bind_bucket_logged = true;
				print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_pipeline_bind={batch_index=%d,match_ordinal=%d,setup_stage=render_pipeline_bind,material_uniform_set=%s,batch_uniform_set={rid=%s,rebound=%s},shader_blend_mode=%s,pipeline={rid=%s,vertex_format=%d,render_primitive=%s,shader_variant=%s,use_lighting=%s,use_msdf=%s,use_lcd=%s,lcd_blend=%s}}",
						gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
						gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
						gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set != 0 ? itos(gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set) : String("none"),
						gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set != 0 ? itos(gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set) : String("none"),
						gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set_rebound ? "true" : "false",
						gdgs_canvas_blend_mode_name(gdgs_temp_diag_first_clipped_preserve_rect_shader_blend_mode),
						gdgs_rid_to_string(pipeline),
						(int)gdgs_quad_vertex_format_id,
						gdgs_canvas_render_primitive_name(p_batch->render_primitive),
						gdgs_canvas_shader_variant_name(p_batch->shader_variant),
						p_batch->use_lighting ? "true" : "false",
						p_batch->use_msdf ? "true" : "false",
						p_batch->use_lcd ? "true" : "false",
						p_batch->has_blend ? "true" : "false"));
			}
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_uniform_pipeline_trace_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch && !gdgs_temp_diag_first_clipped_preserve_rect_uniform_pipeline_bucket_logged) {
				gdgs_temp_diag_first_clipped_preserve_rect_uniform_pipeline_bucket_logged = true;
				print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_uniform_pipeline={batch_index=%d,match_ordinal=%d,setup_stage=batch_uniform_set_pipeline_pairing,material_uniform_set=%s,batch_uniform_set={rid=%s,rebound=%s},shader_blend_mode=%s,pipeline={rid=%s,vertex_format=%d,render_primitive=%s,shader_variant=%s,use_lighting=%s,use_msdf=%s,use_lcd=%s,lcd_blend=%s}}",
						gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
						gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
						gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set != 0 ? itos(gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set) : String("none"),
						gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set != 0 ? itos(gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set) : String("none"),
						gdgs_temp_diag_first_clipped_preserve_rect_batch_uniform_set_rebound ? "true" : "false",
						gdgs_canvas_blend_mode_name(gdgs_temp_diag_first_clipped_preserve_rect_shader_blend_mode),
						gdgs_rid_to_string(pipeline),
						(int)gdgs_quad_vertex_format_id,
						gdgs_canvas_render_primitive_name(p_batch->render_primitive),
						gdgs_canvas_shader_variant_name(p_batch->shader_variant),
						p_batch->use_lighting ? "true" : "false",
						p_batch->use_msdf ? "true" : "false",
						p_batch->use_lcd ? "true" : "false",
						p_batch->has_blend ? "true" : "false"));
			}
			if (p_batch->has_blend) {
				RD::get_singleton()->draw_list_set_blend_constants(p_draw_list, p_batch->modulate);
			}
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_blend_constants_trace_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch && !gdgs_temp_diag_first_clipped_preserve_rect_blend_constants_bucket_logged) {
				gdgs_temp_diag_first_clipped_preserve_rect_blend_constants_bucket_logged = true;
				print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_blend_constants={batch_index=%d,match_ordinal=%d,setup_stage=blend_constants,applied=%s,value=%s}",
						gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
						gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
						p_batch->has_blend ? "true" : "false",
						p_batch->has_blend ? vformat("{r=%f,g=%f,b=%f,a=%f}", p_batch->modulate.r, p_batch->modulate.g, p_batch->modulate.b, p_batch->modulate.a) : String("none")));
			}

			RD::get_singleton()->draw_list_set_push_constant(p_draw_list, &push_constant, sizeof(push_constant));
			FixedVector<RID, 2> vb;
			FixedVector<uint64_t, 2> vo;
			vb.resize_initialized(gdgs_vertex_input_binding_count);
			vo.resize_initialized(gdgs_vertex_input_binding_count);
			const uint64_t gdgs_vertex_input_buffer_offset = uint64_t(p_batch->start) * sizeof(InstanceData);
			if (gdgs_vertex_input_duplicate_format_experiment_applied) {
				vb[0] = p_batch->instance_buffer;
				vb[1] = p_batch->instance_buffer;
				vo[0] = gdgs_vertex_input_buffer_offset;
				vo[1] = gdgs_vertex_input_buffer_offset;
			} else {
				vb[0] = p_batch->instance_buffer;
				vo[0] = gdgs_vertex_input_buffer_offset;
			}
			RD::get_singleton()->draw_list_bind_vertex_buffers_format(p_draw_list, gdgs_quad_vertex_format_id, 1, vb, vo);
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_vertex_bind_trace_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch && !gdgs_temp_diag_first_clipped_preserve_rect_vertex_bind_bucket_logged) {
				gdgs_temp_diag_first_clipped_preserve_rect_vertex_bind_bucket_logged = true;
				print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_vertex_bind={batch_index=%d,match_ordinal=%d,setup_stage=vertex_buffer_bind,vertex_format=%d,instance_buffer=%s,instance_start=%d,instance_count=%d,vertex_offset_bytes=%d,buffer_bind_count=%d,selected_binding_index=%d,experiment_shape=%s}",
						gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
						gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
						(int)gdgs_quad_vertex_format_id,
						gdgs_rid_to_string(p_batch->instance_buffer),
						p_batch->start,
						p_batch->instance_count,
						(int)gdgs_vertex_input_buffer_offset,
						(int)gdgs_vertex_input_binding_count,
						(int)gdgs_vertex_input_binding_index,
						gdgs_vertex_input_experiment_shape));
			}
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch) {
				if (gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_binding_layout_trace_enabled && !gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_binding_layout_bucket_logged) {
					gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_binding_layout_bucket_logged = true;
					print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_vertex_input_binding_layout={batch_index=%d,match_ordinal=%d,setup_stage=vertex_input_recipe_binding_layout,vertex_format=%d,binding_count=%d,selected_binding_index=%d,experiment_shape=%s,bindings=%s}",
							gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
							gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
							(int)gdgs_quad_vertex_format_id,
							(int)gdgs_vertex_input_binding_count,
							(int)gdgs_vertex_input_binding_index,
							gdgs_vertex_input_experiment_shape,
							gdgs_vertex_input_duplicate_format_experiment_applied ? vformat("[{binding=0,stride=%d,frequency=%s,source=instance_buffer,used_by_attributes=false},{binding=1,stride=%d,frequency=%s,source=instance_buffer,used_by_attributes=true}]",
									(int)sizeof(InstanceData),
									gdgs_canvas_vertex_frequency_name(RD::VERTEX_FREQUENCY_INSTANCE),
									(int)sizeof(InstanceData),
									gdgs_canvas_vertex_frequency_name(RD::VERTEX_FREQUENCY_INSTANCE)) : vformat("[{binding=0,stride=%d,frequency=%s,source=instance_buffer,used_by_attributes=true}]",
									(int)sizeof(InstanceData),
									gdgs_canvas_vertex_frequency_name(RD::VERTEX_FREQUENCY_INSTANCE))));
				}
				if (gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_attribute_layout_trace_enabled && !gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_attribute_layout_bucket_logged) {
					gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_attribute_layout_bucket_logged = true;
					print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_vertex_input_attribute_layout={batch_index=%d,match_ordinal=%d,setup_stage=vertex_input_recipe_attribute_layout,vertex_format=%d,attribute_count=8,selected_binding_index=%d,experiment_shape=%s,attributes=[{location=8,binding=%d,offset=0,format=%s,semantic=%s},{location=9,binding=%d,offset=16,format=%s,semantic=%s},{location=10,binding=%d,offset=32,format=%s,semantic=%s},{location=11,binding=%d,offset=48,format=%s,semantic=%s},{location=12,binding=%d,offset=64,format=%s,semantic=%s},{location=13,binding=%d,offset=80,format=%s,semantic=%s},{location=14,binding=%d,offset=96,format=%s,semantic=%s},{location=15,binding=%d,offset=112,format=%s,semantic=%s}]}",
							gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
							gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
							(int)gdgs_quad_vertex_format_id,
							(int)gdgs_vertex_input_binding_index,
							gdgs_vertex_input_experiment_shape,
							(int)gdgs_vertex_input_binding_index,
							gdgs_canvas_data_format_name(RD::DATA_FORMAT_R32G32B32A32_SFLOAT), gdgs_canvas_quad_vertex_attribute_semantic(8),
							(int)gdgs_vertex_input_binding_index,
							gdgs_canvas_data_format_name(RD::DATA_FORMAT_R32G32B32A32_SFLOAT), gdgs_canvas_quad_vertex_attribute_semantic(9),
							(int)gdgs_vertex_input_binding_index,
							gdgs_canvas_data_format_name(RD::DATA_FORMAT_R32G32B32A32_SFLOAT), gdgs_canvas_quad_vertex_attribute_semantic(10),
							(int)gdgs_vertex_input_binding_index,
							gdgs_canvas_data_format_name(RD::DATA_FORMAT_R32G32B32A32_SFLOAT), gdgs_canvas_quad_vertex_attribute_semantic(11),
							(int)gdgs_vertex_input_binding_index,
							gdgs_canvas_data_format_name(RD::DATA_FORMAT_R32G32B32A32_SFLOAT), gdgs_canvas_quad_vertex_attribute_semantic(12),
							(int)gdgs_vertex_input_binding_index,
							gdgs_canvas_data_format_name(RD::DATA_FORMAT_R32G32B32A32_SFLOAT), gdgs_canvas_quad_vertex_attribute_semantic(13),
							(int)gdgs_vertex_input_binding_index,
							gdgs_canvas_data_format_name(RD::DATA_FORMAT_R32G32B32A32_UINT), gdgs_canvas_quad_vertex_attribute_semantic(14),
							(int)gdgs_vertex_input_binding_index,
							gdgs_canvas_data_format_name(RD::DATA_FORMAT_R32G32B32A32_UINT), gdgs_canvas_quad_vertex_attribute_semantic(15)));
				}
				if (gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_provenance_trace_enabled && !gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_provenance_bucket_logged) {
					gdgs_temp_diag_first_clipped_preserve_rect_vertex_input_provenance_bucket_logged = true;
					print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_vertex_input_provenance={batch_index=%d,match_ordinal=%d,setup_stage=vertex_input_recipe_provenance,vertex_format=%d,recipe_owner=RendererCanvasRenderRD::shader.quad_vertex_format_id,recipe_kind=static_canvas_rect_instance_format,command_type=%s,shader_variant=%s,instance_struct={name=InstanceData,size=%d,branch=rect_ninepatch},buffer_source={rid=%s,vertex_offset_bytes=%d,instance_start=%d,instance_count=%d},attribute_path={locations=8_to_15,location_13_mode=vec4_src_rect,location_14_mode=uvec4_flags_instance_uniforms_ofs,location_15_mode=uvec4_lights,selected_binding_index=%d},vertex_input_duplicate_format_experiment={requested=%s,applied=%s,baseline_vertex_format=%d,experiment_vertex_format=%d,selected_vertex_format=%d,layout_preserved=true,layout_semantics=identical_instance_buffer_rebound_to_binding1,experiment_shape=%s,cache_identity_source=vertex_format_id}}",
							gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
							gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
							(int)gdgs_quad_vertex_format_id,
							gdgs_canvas_command_type_name(p_batch->command_type),
							gdgs_canvas_shader_variant_name(p_batch->shader_variant),
							(int)sizeof(InstanceData),
							gdgs_rid_to_string(p_batch->instance_buffer),
							(int)gdgs_vertex_input_buffer_offset,
							p_batch->start,
							p_batch->instance_count,
							(int)gdgs_vertex_input_binding_index,
							gdgs_vertex_input_duplicate_format_experiment_requested ? "true" : "false",
							gdgs_vertex_input_duplicate_format_experiment_applied ? "true" : "false",
							(int)shader.quad_vertex_format_id,
							(int)shader.gdgs_temp_diag_quad_vertex_format_id_duplicate,
							(int)gdgs_quad_vertex_format_id,
							gdgs_vertex_input_experiment_shape));
				}
			}
			RD::get_singleton()->draw_list_bind_index_array(p_draw_list, shader.quad_index_array);
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_index_bind_trace_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch && !gdgs_temp_diag_first_clipped_preserve_rect_index_bind_bucket_logged) {
				gdgs_temp_diag_first_clipped_preserve_rect_index_bind_bucket_logged = true;
				print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_index_bind={batch_index=%d,match_ordinal=%d,setup_stage=index_array_bind,index_array=%s,render_primitive=%s}",
						gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
						gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
						gdgs_rid_to_string(shader.quad_index_array),
						gdgs_canvas_render_primitive_name(p_batch->render_primitive)));
			}
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_draw_binding_trace_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch && !gdgs_temp_diag_first_clipped_preserve_rect_draw_binding_bucket_logged) {
				gdgs_temp_diag_first_clipped_preserve_rect_draw_binding_bucket_logged = true;
				print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_draw_binding={batch_index=%d,match_ordinal=%d,setup_stage=draw_binding_package,instance_buffer=%s,instance_start=%d,instance_count=%d,vertex_offset_bytes=%d,buffer_bind_count=%d,selected_binding_index=%d,index_array=%s,blend_constants_applied=%s}",
						gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
						gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
						gdgs_rid_to_string(p_batch->instance_buffer),
						p_batch->start,
						p_batch->instance_count,
						(int)gdgs_vertex_input_buffer_offset,
						(int)gdgs_vertex_input_binding_count,
						(int)gdgs_vertex_input_binding_index,
						gdgs_rid_to_string(shader.quad_index_array),
						p_batch->has_blend ? "true" : "false"));
			}
			if (gdgs_temp_diag_first_clipped_preserve_rect_trace_active && gdgs_temp_diag_first_clipped_preserve_rect_combined_trace_enabled && gdgs_temp_diag_first_clipped_preserve_rect_batch_ptr == p_batch && !gdgs_temp_diag_first_clipped_preserve_rect_prereq_logged) {
				gdgs_temp_diag_first_clipped_preserve_rect_prereq_logged = true;
				print_line(vformat("[gdgs-canvas] temp_diag_first_clipped_preserve_rect_prereq={batch_index=%d,match_ordinal=%d,setup_stage=pre_l88_draw,scissor={changed=%s,enabled=%s,rect=%s},material_uniform_set=%s,batch_uniform_set=%s,shader_blend_mode=%s,pipeline={rid=%s,vertex_format=%d,render_primitive=%s,shader_variant=%s,use_lighting=%s,use_msdf=%s,use_lcd=%s,lcd_blend=%s},push_constant={batch_flags=0x%x,specular_shininess=%f,msdf={px_range=%f,outline=%f},color_texture_pixel_size={x=%f,y=%f}},draw_binding={instance_buffer=%s,instance_start=%d,instance_count=%d,vertex_offset=%d,buffer_bind_count=%d,selected_binding_index=%d,index_array=%s,blend_constants=%s}}",
						gdgs_temp_diag_first_clipped_preserve_rect_batch_index,
						gdgs_temp_diag_first_clipped_preserve_rect_match_ordinal,
						gdgs_temp_diag_first_clipped_preserve_rect_scissor_changed ? "true" : "false",
						gdgs_temp_diag_first_clipped_preserve_rect_scissor_enabled ? "true" : "false",
						gdgs_temp_diag_first_clipped_preserve_rect_scissor_enabled ? gdgs_rect2_to_string(gdgs_temp_diag_first_clipped_preserve_rect_scissor_rect) : String("none"),
						gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set != 0 ? itos(gdgs_temp_diag_first_clipped_preserve_rect_material_uniform_set) : String("none"),
						state.current_batch_uniform_set.is_valid() ? gdgs_rid_to_string(state.current_batch_uniform_set) : String("none"),
						gdgs_canvas_blend_mode_name(gdgs_temp_diag_first_clipped_preserve_rect_shader_blend_mode),
						gdgs_rid_to_string(pipeline),
						(int)gdgs_quad_vertex_format_id,
						gdgs_canvas_render_primitive_name(p_batch->render_primitive),
						gdgs_canvas_shader_variant_name(p_batch->shader_variant),
						p_batch->use_lighting ? "true" : "false",
						p_batch->use_msdf ? "true" : "false",
						p_batch->use_lcd ? "true" : "false",
						p_batch->has_blend ? "true" : "false",
						push_constant.batch_flags,
						push_constant.specular_shininess,
						push_constant.msdf[0],
						push_constant.msdf[1],
						push_constant.color_texture_pixel_size[0],
						push_constant.color_texture_pixel_size[1],
						gdgs_rid_to_string(p_batch->instance_buffer),
						p_batch->start,
						p_batch->instance_count,
						(int)gdgs_vertex_input_buffer_offset,
						(int)gdgs_vertex_input_binding_count,
						(int)gdgs_vertex_input_binding_index,
						gdgs_rid_to_string(shader.quad_index_array),
						p_batch->has_blend ? vformat("{r=%f,g=%f,b=%f,a=%f}", p_batch->modulate.r, p_batch->modulate.g, p_batch->modulate.b, p_batch->modulate.a) : String("none")));
			}
			RD::get_singleton()->draw_list_draw(p_draw_list, true, p_batch->instance_count);

			if (r_render_info) {
				r_render_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] += p_batch->instance_count;
				r_render_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += 2 * p_batch->instance_count;
				r_render_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]++;
			}
		} break;

		case Item::Command::TYPE_POLYGON: {
			ERR_FAIL_NULL(p_batch->command);
			PushConstantAttributes push_constant = p_batch->push_constant_attributes();

			const Item::CommandPolygon *polygon = static_cast<const Item::CommandPolygon *>(p_batch->command);

			PolygonBuffers *pb = polygon_buffers.polygons.getptr(polygon->polygon.polygon_id);
			ERR_FAIL_NULL(pb);

			pipeline_key.vertex_format_id = pb->vertex_format_id;
			pipeline = _get_pipeline_specialization_or_ubershader(p_shader_data, pipeline_key, push_constant);
			RD::get_singleton()->draw_list_bind_render_pipeline(p_draw_list, pipeline);

			RD::get_singleton()->draw_list_set_push_constant(p_draw_list, &push_constant, sizeof(push_constant));
			RD::get_singleton()->draw_list_bind_vertex_array(p_draw_list, pb->vertex_array);
			if (pb->indices.is_valid()) {
				RD::get_singleton()->draw_list_bind_index_array(p_draw_list, pb->indices);
			}

			RD::get_singleton()->draw_list_draw(p_draw_list, pb->indices.is_valid());
			if (r_render_info) {
				r_render_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME]++;
				r_render_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += _indices_to_primitives(polygon->primitive, pb->primitive_count);
				r_render_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]++;
			}
		} break;

		case Item::Command::TYPE_PRIMITIVE: {
			ERR_FAIL_NULL(p_batch->command);

			const Item::CommandPrimitive *primitive = static_cast<const Item::CommandPrimitive *>(p_batch->command);

			PushConstant push_constant = p_batch->push_constant();
			pipeline_key.vertex_format_id = shader.primitive_vertex_format_id;
			pipeline = _get_pipeline_specialization_or_ubershader(p_shader_data, pipeline_key, push_constant);
			RD::get_singleton()->draw_list_bind_render_pipeline(p_draw_list, pipeline);

			RD::get_singleton()->draw_list_set_push_constant(p_draw_list, &push_constant, sizeof(push_constant));
			FixedVector<RID, 1> vb = { p_batch->instance_buffer };
			FixedVector<uint64_t, 1> vo = { uint64_t(p_batch->start) * sizeof(InstanceData) };
			RD::get_singleton()->draw_list_bind_vertex_buffers_format(p_draw_list, shader.primitive_vertex_format_id, 1, vb, vo);
			RD::get_singleton()->draw_list_bind_index_array(p_draw_list, primitive_arrays.index_array[MIN(3u, primitive->point_count) - 1]);
			uint32_t instance_count = p_batch->instance_count;
			RD::get_singleton()->draw_list_draw(p_draw_list, true, instance_count);

			if (r_render_info) {
				const RSE::PrimitiveType rs_primitive[5] = { RSE::PRIMITIVE_POINTS, RSE::PRIMITIVE_POINTS, RSE::PRIMITIVE_LINES, RSE::PRIMITIVE_TRIANGLES, RSE::PRIMITIVE_TRIANGLES };
				r_render_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] += instance_count;
				r_render_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += _indices_to_primitives(rs_primitive[p_batch->primitive_points], p_batch->primitive_points) * instance_count;
				r_render_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]++;
			}
		} break;

		case Item::Command::TYPE_MESH:
		case Item::Command::TYPE_MULTIMESH:
		case Item::Command::TYPE_PARTICLES: {
			ERR_FAIL_NULL(p_batch->command);

			PushConstantAttributes push_constant = p_batch->push_constant_attributes();

			RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();
			RendererRD::ParticlesStorage *particles_storage = RendererRD::ParticlesStorage::get_singleton();

			RID mesh;
			RID mesh_instance;

			if (p_batch->command_type == Item::Command::TYPE_MESH) {
				const Item::CommandMesh *m = static_cast<const Item::CommandMesh *>(p_batch->command);
				mesh = m->mesh;
				mesh_instance = m->mesh_instance;
			} else if (p_batch->command_type == Item::Command::TYPE_MULTIMESH) {
				const Item::CommandMultiMesh *mm = static_cast<const Item::CommandMultiMesh *>(p_batch->command);
				RID multimesh = mm->multimesh;
				mesh = mesh_storage->multimesh_get_mesh(multimesh);

				RID uniform_set = mesh_storage->multimesh_get_2d_uniform_set(multimesh, shader.default_version_rd_shader, TRANSFORMS_UNIFORM_SET);
				RD::get_singleton()->draw_list_bind_uniform_set(p_draw_list, uniform_set, TRANSFORMS_UNIFORM_SET);
			} else if (p_batch->command_type == Item::Command::TYPE_PARTICLES) {
				const Item::CommandParticles *pt = static_cast<const Item::CommandParticles *>(p_batch->command);
				RID particles = pt->particles;
				mesh = particles_storage->particles_get_draw_pass_mesh(particles, 0);

				ERR_BREAK(particles_storage->particles_get_mode(particles) != RSE::PARTICLES_MODE_2D);
				particles_storage->particles_request_process(particles);

				if (particles_storage->particles_is_inactive(particles)) {
					break;
				}

				RenderingServerDefault::redraw_request(); // Active particles means redraw request.

				int dpc = particles_storage->particles_get_draw_passes(particles);
				if (dpc == 0) {
					break; // Nothing to draw.
				}

				RID uniform_set = particles_storage->particles_get_instance_buffer_uniform_set(pt->particles, shader.default_version_rd_shader, TRANSFORMS_UNIFORM_SET);
				RD::get_singleton()->draw_list_bind_uniform_set(p_draw_list, uniform_set, TRANSFORMS_UNIFORM_SET);
			}

			if (mesh.is_null()) {
				break;
			}

			uint32_t surf_count = mesh_storage->mesh_get_surface_count(mesh);

			for (uint32_t j = 0; j < surf_count; j++) {
				void *surface = mesh_storage->mesh_get_surface(mesh, j);

				RSE::PrimitiveType primitive = mesh_storage->mesh_surface_get_primitive(surface);
				ERR_CONTINUE(primitive < 0 || primitive >= RSE::PRIMITIVE_MAX);

				RID vertex_array;
				pipeline_key.variant = primitive == RSE::PRIMITIVE_POINTS ? SHADER_VARIANT_ATTRIBUTES_POINTS : SHADER_VARIANT_ATTRIBUTES;
				pipeline_key.render_primitive = _primitive_type_to_render_primitive(primitive);
				pipeline_key.vertex_format_id = RD::INVALID_FORMAT_ID;

				pipeline = _get_pipeline_specialization_or_ubershader(p_shader_data, pipeline_key, push_constant, mesh_instance, surface, j, &vertex_array);
				RD::get_singleton()->draw_list_bind_render_pipeline(p_draw_list, pipeline);

				RD::get_singleton()->draw_list_set_push_constant(p_draw_list, &push_constant, sizeof(push_constant));

				RID index_array = mesh_storage->mesh_surface_get_index_array(surface, 0);

				if (index_array.is_valid()) {
					RD::get_singleton()->draw_list_bind_index_array(p_draw_list, index_array);
				}

				RD::get_singleton()->draw_list_bind_vertex_array(p_draw_list, vertex_array);
				RD::get_singleton()->draw_list_draw(p_draw_list, index_array.is_valid(), p_batch->mesh_instance_count);

				if (r_render_info) {
					r_render_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME]++;
					r_render_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += _indices_to_primitives(primitive, mesh_storage->mesh_surface_get_vertices_drawn_count(surface)) * p_batch->mesh_instance_count;
					r_render_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]++;
				}
			}
		} break;
		case Item::Command::TYPE_TRANSFORM:
		case Item::Command::TYPE_CLIP_IGNORE:
		case Item::Command::TYPE_ANIMATION_SLICE: {
			// Can ignore these as they only impact batch creation.
		} break;
	}
}

RendererCanvasRenderRD::InstanceData *RendererCanvasRenderRD::new_instance_data(Batch &p_current_batch, const InstanceData &template_instance, bool p_use_push_data) {
	InstanceData *instance_data = nullptr;

	if (unlikely(p_use_push_data)) {
		instance_data = &p_current_batch.push_data;
		// instance_count must be > 0 to indicate the batch has been used when calling _new_batch, so we set a flag.
		p_current_batch.instance_count = PUSH_DATA_INSTANCE_COUNT;
	} else {
		// Return the intermediary instance data to prevent the caller from accidentally reading write-combined memory pages, which has huge performance implications.
		instance_data = &state.intermediary_instance_data;
	}

	memcpy(instance_data, &template_instance, sizeof(InstanceData));
	return instance_data;
}

RendererCanvasRenderRD::Batch *RendererCanvasRenderRD::_new_batch(bool &r_batch_broken) {
	if (state.canvas_instance_batches.is_empty()) {
		Batch new_batch;
		// First try to reuse previous instance buffer if possible.
		if (state.prev_instance_data && state.prev_instance_data_index < state.max_instances_per_buffer) {
			bool must_remap = state.instance_buffers.prepare_for_map(true);
			// must_remap will be false if we're preparing to map the buffer for the same frame and can reuse the existing UMA buffer.
			if (!must_remap) {
				state.instance_data = state.prev_instance_data;
				state.instance_data_index = state.prev_instance_data_index;
			}
			state.prev_instance_data = nullptr;
			state.prev_instance_data_index = 0;
		}
		// This will still be a valid point when multiple calls to _render_batch_items
		// are made in the same draw call.
		if (state.instance_data == nullptr) {
			// If there is no existing instance buffer, we must allocate a new one.
			_allocate_instance_buffer();
		} else {
			// Otherwise, just use the existing one from where it last left off.
			new_batch.start = state.instance_data_index;
		}
		new_batch.instance_buffer = state.instance_buffers._get(0);
		state.canvas_instance_batches.push_back(new_batch);
		return state.canvas_instance_batches.ptr();
	}

	if (r_batch_broken || state.canvas_instance_batches[state.current_batch_index].instance_count == 0) {
		return &state.canvas_instance_batches[state.current_batch_index];
	}

	r_batch_broken = true;

	// Copy the properties of the current batch, we will manually update the things that changed.
	Batch new_batch = state.canvas_instance_batches[state.current_batch_index];
	new_batch.instance_count = 0;
	new_batch.start = state.instance_data_index;
	memset(&new_batch.push_data, 0, sizeof(new_batch.push_data));
	state.current_batch_index++;
	state.canvas_instance_batches.push_back(new_batch);
	return &state.canvas_instance_batches[state.current_batch_index];
}

void RendererCanvasRenderRD::_add_to_batch(bool &r_batch_broken, Batch *&r_current_batch) {
	DEV_ASSERT(r_current_batch->command_type == Item::Command::TYPE_RECT ||
			r_current_batch->command_type == Item::Command::TYPE_NINEPATCH ||
			r_current_batch->command_type == Item::Command::TYPE_PRIMITIVE);
	r_current_batch->instance_count++;
	memcpy(&state.instance_data[state.instance_data_index], &state.intermediary_instance_data, sizeof(InstanceData));
	state.instance_data_index++;
	if (state.instance_data_index >= state.max_instances_per_buffer) {
		RD::get_singleton()->buffer_flush(r_current_batch->instance_buffer);
		state.instance_data = nullptr;
		_allocate_instance_buffer();
		state.instance_data_index = 0;
		r_batch_broken = false; // Force a new batch to be created
		r_current_batch = _new_batch(r_batch_broken);
		r_current_batch->instance_buffer = state.instance_buffers._get(0);
	}
}

void RendererCanvasRenderRD::_allocate_instance_buffer() {
	state.instance_buffers.prepare_for_upload();
	state.instance_data = reinterpret_cast<InstanceData *>(state.instance_buffers.map_raw_for_upload(0));
}

void RendererCanvasRenderRD::_prepare_batch_texture_info(RID p_texture, TextureState &p_state, TextureInfo *p_info) {
	if (p_texture.is_null()) {
		p_texture = default_canvas_texture;
	}

	RendererRD::TextureStorage::CanvasTextureInfo info =
			RendererRD::TextureStorage::get_singleton()->canvas_texture_get_info(
					p_texture,
					p_state.texture_filter(),
					p_state.texture_repeat(),
					p_state.linear_colors(),
					p_state.texture_is_data());
	// something odd happened
	if (info.is_null()) {
		_prepare_batch_texture_info(default_canvas_texture, p_state, p_info);
		return;
	}

	p_info->state = p_state;
	p_info->diffuse = info.diffuse;
	p_info->normal = info.normal;
	p_info->specular = info.specular;
	p_info->sampler = info.sampler;

	// cache values to be copied to instance data
	if (info.specular_color.a < 0.999) {
		p_info->flags |= BATCH_FLAGS_DEFAULT_SPECULAR_MAP_USED;
	}

	if (info.use_normal) {
		p_info->flags |= BATCH_FLAGS_DEFAULT_NORMAL_MAP_USED;
	}

	uint8_t a = uint8_t(CLAMP(info.specular_color.a * 255.0, 0.0, 255.0));
	uint8_t b = uint8_t(CLAMP(info.specular_color.b * 255.0, 0.0, 255.0));
	uint8_t g = uint8_t(CLAMP(info.specular_color.g * 255.0, 0.0, 255.0));
	uint8_t r = uint8_t(CLAMP(info.specular_color.r * 255.0, 0.0, 255.0));
	p_info->specular_shininess = uint32_t(a) << 24 | uint32_t(b) << 16 | uint32_t(g) << 8 | uint32_t(r);

	p_info->texpixel_size = Vector2(1.0 / float(info.size.width), 1.0 / float(info.size.height));
}

RendererCanvasRenderRD::~RendererCanvasRenderRD() {
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	//canvas state

	material_storage->material_free(default_canvas_group_material);
	material_storage->shader_free(default_canvas_group_shader);

	material_storage->material_free(default_clip_children_material);
	material_storage->shader_free(default_clip_children_shader);

	{
		if (state.canvas_state_buffer.is_valid()) {
			RD::get_singleton()->free_rid(state.canvas_state_buffer);
		}

		memdelete_arr(state.light_uniforms);
		RD::get_singleton()->free_rid(state.lights_storage_buffer);
	}

	//shadow rendering
	{
		shadow_render.shader.version_free(shadow_render.shader_version);
		//this will also automatically clear all pipelines
		RD::get_singleton()->free_rid(state.shadow_sampler);
	}

	//buffers
	{
		RD::get_singleton()->free_rid(shader.quad_index_array);
		RD::get_singleton()->free_rid(shader.quad_index_buffer);
		//primitives are erase by dependency
	}

	if (state.shadow_fb.is_valid()) {
		RD::get_singleton()->free_rid(state.shadow_depth_texture);
	}
	RD::get_singleton()->free_rid(state.shadow_texture);

	if (state.shadow_occluder_buffer.is_valid()) {
		RD::get_singleton()->free_rid(state.shadow_occluder_buffer);
	}

	state.instance_buffers.uninit();

	// Disable the callback, as we're tearing everything down
	texture_storage->canvas_texture_set_invalidation_callback(default_canvas_texture, nullptr, nullptr);
	texture_storage->canvas_texture_free(default_canvas_texture);
	//pipelines don't need freeing, they are all gone after shaders are gone

	memdelete(shader.default_version_data);
}
