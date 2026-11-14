#include "litehtml_render_grid.h"

#include "litehtml_grid_placement.h"

#include <litehtml/html_tag.h>

#include <numeric>

namespace litehtml {
	namespace {

		using context = containing_block_context;
		using alignment = alignment_keyword;

		bool is_definite(context::typed_pixel value) {
			return is_one_of(value.type, context::cbc_value_type_absolute, context::cbc_value_type_percentage);
		}

		std::optional<pixel_t> definite_size(context::typed_pixel value) {
			if (!is_definite(value)) return std::nullopt;
			return std::max<pixel_t>(0, value.value);
		}

		bool has_scrollable_overflow(const render_item& item) {
			return is_one_of(item.css().get_overflow(), overflow_hidden, overflow_scroll, overflow_auto);
		}

		pixel_t clamp_size(pixel_t size, context::typed_pixel min, context::typed_pixel max) {
			if (is_definite(max)) size = std::min(size, max.value);
			if (is_definite(min)) size = std::max(size, min.value);
			return std::max<pixel_t>(0, size);
		}

		bool stretches(css_alignment value, bool replaced = false) {
			return value.keyword == alignment::stretch || (value.keyword == alignment::normal && !replaced);
		}

		pixel_t position_offset(css_alignment value, pixel_t free_space) {
			if (free_space < 0 && value.overflow == alignment_overflow::safe) return 0;
			switch (value.keyword) {
				case alignment::end:
				case alignment::self_end:
				case alignment::flex_end:
				case alignment::right:
					return free_space;
				case alignment::last_baseline:
					return std::max<pixel_t>(0, free_space);
				case alignment::center:
					return free_space / 2;
				default:
					return 0;
			}
		}

		struct grid_item_alignment {
			css_alignment horizontal;
			css_alignment vertical;
		};

		css_alignment resolve_self_alignment(css_alignment self, css_alignment items) {
			return self.keyword == alignment::automatic ? items : self;
		}

		rendered_width render_intrinsic_width(render_item& item, const context& grid) {
			// Give the renderer room for unwrapped content and an indefinite percentage basis.
			context probe;
			probe.width = 1 << 20;
			probe.render_width = probe.width;
			probe.size_mode = context::size_mode_content | context::size_mode_grid_measure_width;
			probe.context_idx = grid.context_idx;
			return item.render(0, 0, probe, nullptr);
		}

		struct grid_intrinsic_width {
			pixel_t minimum;
			pixel_t preferred;
		};

		grid_intrinsic_width intrinsic_content_width(const render_item& item, rendered_width measured) {
			const auto preferred = std::max<pixel_t>(0, item.width() - item.content_offset_width());
			const bool nowrap = is_one_of(item.css().get_white_space(), white_space_nowrap, white_space_pre);
			const auto minimum = nowrap ? preferred :
				std::clamp(measured.min_width - item.content_offset_width(), pixel_t(0), preferred);
			return { minimum, preferred };
		}

		context cell_context(render_item& item, const context& grid, grid_item_alignment align,
			std::optional<grid_intrinsic_width>& intrinsic, pixel_t width, std::optional<pixel_t> height) {
			context cell;
			cell.context_idx = grid.context_idx;
			cell.width = { width, context::cbc_value_type_absolute };
			cell.render_width = cell.width;
			if (height) cell.height = { *height, context::cbc_value_type_absolute };
			const auto& margins = item.css().get_margins();
			const bool replaced = item.src_el()->is_replaced();
			if (item.css().get_width().is_predefined()) {
				if (stretches(align.horizontal, replaced) && !margins.left.is_predefined() && !margins.right.is_predefined()) {
					cell.size_mode |= context::size_mode_grid_assigned_width;
				} else if (!replaced) {
					if (!intrinsic) {
						const auto measured = render_intrinsic_width(item, grid);
						intrinsic = intrinsic_content_width(item, measured);
					}
					item.calc_outlines(width);
					const auto outlines = item.content_offset_width();
					const auto fit = std::clamp(width - outlines, intrinsic->minimum, intrinsic->preferred);
					// Keep the full grid area as the percentage basis.
					cell.render_width.value = fit + outlines;
					cell.size_mode |= context::size_mode_grid_assigned_width;
				}
			}
			if (height && item.css().get_height().is_predefined() && stretches(align.vertical, replaced) &&
				!margins.top.is_predefined() && !margins.bottom.is_predefined())
				cell.size_mode |= context::size_mode_grid_stretch_height;
			return cell;
		}

		pixel_t align_item(css_alignment align, pixel_t free_space, pixel_t& leading, pixel_t& trailing,
			const css_length& leading_style, const css_length& trailing_style) {
			const bool auto_leading = leading_style.is_predefined();
			const bool auto_trailing = trailing_style.is_predefined();
			if (free_space > 0 && (auto_leading || auto_trailing)) {
				const auto share = free_space / (auto_leading && auto_trailing ? 2 : 1);
				if (auto_leading) leading = share;
				if (auto_trailing) trailing = share;
				return auto_leading ? share : 0;
			}
			return position_offset(align, free_space);
		}

		struct grid_axis_spacing {
			pixel_t leading = 0;
			pixel_t extra_gap = 0;
		};

		grid_axis_spacing distribute_tracks(css_alignment align, pixel_t free_space, std::size_t active_tracks) {
			if (active_tracks == 0) return {};
			switch (align.keyword) {
				case alignment::space_between:
					if (free_space > 0 && active_tracks > 1)
						return { 0, free_space / static_cast<pixel_t>(active_tracks - 1) };
					return {};
				case alignment::space_around:
					if (free_space <= 0) return {};
					return { free_space / (2 * static_cast<pixel_t>(active_tracks)),
						free_space / static_cast<pixel_t>(active_tracks) };
				case alignment::space_evenly: {
					if (free_space <= 0) return {};
					const auto share = free_space / static_cast<pixel_t>(active_tracks + 1);
					return { share, share };
				}
				default:
					return { position_offset(align, free_space), 0 };
			}
		}

		struct grid_axis_geometry {
			const std::vector<pixel_t>& tracks;
			std::vector<pixel_t> offsets;

			grid_axis_geometry(const std::vector<pixel_t>& track_sizes, const grid_track_gutters& gutters,
				pixel_t gap, pixel_t available, css_alignment align) : tracks(track_sizes) {
				const auto gutter_extent = gap * static_cast<pixel_t>(gutters.count_inside({0, tracks.size()}));
				const auto occupied = std::accumulate(tracks.begin(), tracks.end(), static_cast<double>(gutter_extent));
				const auto spacing = distribute_tracks(align, static_cast<pixel_t>(available - occupied), gutters.active_count());
				offsets.reserve(tracks.size());
				auto offset = spacing.leading;
				for (std::size_t i = 0; i < tracks.size(); ++i) {
					offsets.push_back(offset);
					offset += tracks[i] + (gutters.after(i) ? gap + spacing.extra_gap : 0);
				}
			}

			pixel_t start(grid_track_range range) const {
				return offsets[range.start];
			}

			pixel_t extent(grid_track_range range) const {
				const auto last = range.end - 1;
				// Keep single-track sizes exact even when positioned far from the origin.
				if (last == range.start) return tracks[last];
				return offsets[last] - start(range) + tracks[last];
			}

			pixel_t extent() const {
				return tracks.empty() ? 0 : offsets.back() + tracks.back();
			}
		};

	}

	std::shared_ptr<render_item> render_item_grid::init() {
		std::list<std::shared_ptr<render_item>> items;
		std::shared_ptr<render_item> text_run;
		bool has_text = false;
		const auto flush_text = [&] {
			if (has_text) items.push_back(text_run);
			text_run.reset();
			has_text = false;
			};

		for (const auto& child : m_children) {
			if (child->css().get_display() == display_inline_text) {
				if (!text_run) {
					auto box = std::make_shared<html_tag>(src_el(), "display: block");
					text_run = std::make_shared<render_item_block>(box);
					text_run->parent(shared_from_this());
				}
				text_run->add_child(child);
				has_text |= !child->src_el()->is_white_space();
			}
			else {
				// Out-of-flow nodes do not interrupt a run of text in the grid.
				if (child->src_el()->in_normal_flow()) flush_text();
				items.push_back(child);
			}
		}
		flush_text();
		items.sort([](const auto& a, const auto& b) { return a->css().get_order() < b->css().get_order(); });
		m_children.swap(items);

		src_el()->add_render(shared_from_this());
		for (auto& child : m_children) child = child->init();
		return shared_from_this();
	}

	size size_grid_image(const render_item& item, const context& self, const size& intrinsic) {
		const bool width_auto = !is_definite(self.width);
		const bool height_auto = !is_definite(self.height);
		const pixel_t ratio = intrinsic.height > 0 ? intrinsic.width / intrinsic.height : 0;
		pixel_t width = width_auto ? intrinsic.width : self.render_width;
		pixel_t height = height_auto ? intrinsic.height : self.height - item.box_sizing_height();
		if (ratio > 0 && width_auto && height_auto) {
			// Use the intersection of the two axes' limits to scale the natural image.
			auto lower = self.min_width;
			auto upper = self.max_width;
			if (is_definite(self.min_height))
				lower = { std::max(is_definite(lower) ? lower.value : 0, self.min_height * ratio), context::cbc_value_type_absolute };
			if (is_definite(self.max_height))
				upper = { is_definite(upper) ? std::min(upper.value, self.max_height * ratio) :
					self.max_height * ratio, context::cbc_value_type_absolute };
			width = clamp_size(width, lower, upper);
			return { width, width / ratio };
		}
		if (!width_auto) width = clamp_size(width, self.min_width, self.max_width);
		if (!height_auto) height = clamp_size(height, self.min_height, self.max_height);
		if (ratio > 0 && width_auto) width = height * ratio;
		if (ratio > 0 && height_auto) height = width / ratio;
		return { clamp_size(width, self.min_width, self.max_width), clamp_size(height, self.min_height, self.max_height) };
	}

	rendered_width render_item_grid::_render(
		pixel_t x,
		pixel_t y,
		const context& containing_block_size,
		formatting_context*, bool
	) {
		const auto self = calculate_containing_block_context(containing_block_size);
		const bool measure_width = containing_block_size.size_mode & context::size_mode_grid_measure_width;
		const bool fit_content = containing_block_size.size_mode & context::size_mode_content;
		const bool definite_width = (is_definite(self.width) && !(fit_content &&
			(containing_block_size.size_mode & context::size_mode_exact_width))) ||
			(!fit_content && src_el()->in_normal_flow() && css().get_float() == float_none);
		const bool definite_height = is_definite(self.height);
		const auto available_width = clamp_size(self.render_width, self.min_width, self.max_width);
		const auto available_height = clamp_size(self.height - box_sizing_height(), self.min_height, self.max_height);
		const auto& column_gap = css().get_column_gap();
		const auto& row_gap = css().get_row_gap();
		const auto width = definite_width ? std::optional<pixel_t>(available_width) : std::nullopt;
		const auto height = definite_height ? std::optional<pixel_t>(available_height) : std::nullopt;

		auto column_axis = expand_grid_tracks(css().get_grid_template_columns(), column_gap,
			width, definite_size(self.min_width), definite_size(self.max_width));
		auto row_axis = expand_grid_tracks(css().get_grid_template_rows(), row_gap,
			height, definite_size(self.min_height), definite_size(self.max_height));
		apply_grid_template_areas(css().get_grid_template_areas(), row_axis, column_axis);
		const auto& columns = column_axis.tracks;
		const auto& rows = row_axis.tracks;
		std::vector<render_item*> items;
		std::vector<grid_item_lines> positions;
		for (const auto& child : m_children) {
			if (child->src_el()->in_normal_flow()) {
				items.push_back(child.get());
				positions.push_back(child->css().get_grid_lines());
			}
		}
		const auto placement = place_grid_items(positions, row_axis.explicit_count, column_axis.explicit_count,
			row_axis.names, column_axis.names, css().get_grid_auto_flow());
		const auto column_gutters = finish_grid_tracks(column_axis, css().get_grid_auto_columns(), placement.column_offset, placement.column_count,
			placement.items, &grid_item_placement::columns);
		const auto row_gutters = finish_grid_tracks(row_axis, css().get_grid_auto_rows(), placement.row_offset, placement.row_count,
			placement.items, &grid_item_placement::rows);

		const auto justify_content = css().get_justify_content();
		const auto align_content = css().get_align_content();
		std::vector<grid_item_alignment> alignments;
		alignments.reserve(items.size());
		for (const auto* item : items) {
			alignments.push_back({
				resolve_self_alignment(item->css().get_justify_self(), css().get_justify_items()),
				resolve_self_alignment(item->css().get_align_self(), css().get_align_items())
			});
		}
		std::vector<std::optional<grid_intrinsic_width>> intrinsic(items.size());
		std::vector<grid_track_contribution> column_content;
		column_content.reserve(items.size());
		for (std::size_t index = 0; index < items.size(); ++index) {
			const auto range = placement.items[index].columns;
			if (!grid_range_needs_content(columns, range, definite_width)) continue;
			auto& item = *items[index];
			const auto widths = render_intrinsic_width(item, self);
			intrinsic[index] = intrinsic_content_width(item, widths);
			const auto& width = item.css().get_width();
			const bool explicit_width = !width.is_predefined() && width.units() != css_units_percentage;
			const bool has_min_width = !item.css().get_min_width().is_predefined() &&
				item.css().get_min_width().units() != css_units_percentage;
			const auto min_width = has_min_width ? item.css().get_min_width().calc_percent(0) : 0;
			const auto outer_min_width = min_width + item.content_offset_width() - item.box_sizing_width();
			const bool nowrap = is_one_of(item.css().get_white_space(), white_space_nowrap, white_space_pre);
			const bool auto_minimum = grid_range_has_auto_minimum(columns, range, definite_width);
			const auto minimum = explicit_width ? item.width() : std::max(outer_min_width,
				has_scrollable_overflow(item) || !auto_minimum ? item.content_offset_width() :
				nowrap || item.src_el()->is_replaced() ? item.width() : std::min(widths.min_width, item.width()));
			column_content.push_back({ range, minimum, item.width() });
		}

		const auto initial_columns = size_grid_tracks(columns, column_content, column_gutters,
			column_gap.calc_percent(width.value_or(0)), width, stretches(justify_content));
		m_pos.width = definite_width ? available_width : clamp_size(
			measure_width ? initial_columns.extent : std::clamp(available_width, initial_columns.minimum, initial_columns.extent),
			self.min_width, self.max_width);
		if (measure_width)
			return { m_pos.width + content_offset_width(), std::min(m_pos.width, initial_columns.minimum) + content_offset_width() };

		const auto horizontal_gap = column_gap.calc_percent(m_pos.width);
		const auto column_sizes = !definite_width ?
			size_grid_tracks(columns, column_content, column_gutters, horizontal_gap, m_pos.width, stretches(justify_content)) : initial_columns;
		const grid_axis_geometry column_geometry(column_sizes.tracks, column_gutters, horizontal_gap, m_pos.width, justify_content);

		std::vector<grid_track_contribution> row_content;
		row_content.reserve(items.size());
		for (std::size_t index = 0; index < items.size(); ++index) {
			const auto& placed = placement.items[index];
			if (!grid_range_needs_content(rows, placed.rows, definite_height) &&
				(definite_height || !grid_range_needs_content(rows, placed.rows, true))) continue;
			auto& item = *items[index];
			auto cell = cell_context(item, self, alignments[index], intrinsic[index],
				column_geometry.extent(placed.columns), std::nullopt);
			cell.size_mode |= context::size_mode_grid_measure_height;
			item.render(0, 0, cell, nullptr);
			const bool explicit_height = !item.css().get_height().is_predefined() &&
				item.css().get_height().units() != css_units_percentage;
			const auto& min_height = item.css().get_min_height();
			const bool has_min_height = !min_height.is_predefined() && min_height.units() != css_units_percentage;
			const bool auto_minimum = grid_range_has_auto_minimum(rows, placed.rows, definite_height);
			const auto minimum = !explicit_height && (has_scrollable_overflow(item) || !auto_minimum) ?
				item.content_offset_height() + (has_min_height ? std::max<pixel_t>(0, min_height.calc_percent(0) - item.box_sizing_height()) : 0) :
				item.height();
			row_content.push_back({ placed.rows, minimum, item.height() });
		}

		const auto initial_rows = size_grid_tracks(rows, row_content, row_gutters,
			row_gap.calc_percent(height.value_or(0)), height, stretches(align_content));
		m_pos.height = definite_height ? available_height : clamp_size(initial_rows.extent, self.min_height, self.max_height);
		const rendered_width widths{ m_pos.width + content_offset_width(),
			(definite_width && is_definite(self.width) ? m_pos.width : initial_columns.minimum) + content_offset_width() };
		if (containing_block_size.size_mode & context::size_mode_grid_measure_height) return widths;

		const auto vertical_gap = row_gap.calc_percent(m_pos.height);
		const bool resolve_rows = !definite_height &&
			(grid_tracks_use_percentage(rows) || m_pos.height != initial_rows.extent);
		const auto row_sizes = resolve_rows ?
			size_grid_tracks(rows, row_content, row_gutters, vertical_gap, m_pos.height, stretches(align_content)) : initial_rows;
		const grid_axis_geometry row_geometry(row_sizes.tracks, row_gutters, vertical_gap, m_pos.height, align_content);
		track_extent = { column_geometry.extent(), row_geometry.extent() };

		for (std::size_t index = 0; index < items.size(); ++index) {
			const auto& placed = placement.items[index];
			const position area{ column_geometry.start(placed.columns), row_geometry.start(placed.rows),
				column_geometry.extent(placed.columns), row_geometry.extent(placed.rows) };
			auto& item = *items[index];
			const auto align = alignments[index];
			const auto cell = cell_context(item, self, align, intrinsic[index], area.width, area.height);
			item.render(area.x, area.y, cell, nullptr);
			const auto& css_margins = item.css().get_margins();
			auto& used_margins = item.get_margins();
			item.pos().x += align_item(align.horizontal, area.width - item.width(),
				used_margins.left, used_margins.right, css_margins.left, css_margins.right);
			item.pos().y += align_item(align.vertical, area.height - item.height(),
				used_margins.top, used_margins.bottom, css_margins.top, css_margins.bottom);
			item.apply_relative_shift(cell);
		}

		context positioned;
		positioned.context_idx = self.context_idx;
		positioned.width = { m_pos.width, context::cbc_value_type_absolute };
		positioned.render_width = positioned.width;
		positioned.height = { m_pos.height, context::cbc_value_type_absolute };
		for (const auto& child : m_children) {
			if (child->src_el()->in_normal_flow()) continue;
			const auto natural = child->render(0, 0, positioned, nullptr).natural_width;
			if (child->css().get_width().is_predefined() && natural < child->width())
				child->render(0, 0, positioned.new_width(natural), nullptr);
		}
		m_pos.move_to(x + content_offset_left(), y + content_offset_top());
		return widths;
	}

}
