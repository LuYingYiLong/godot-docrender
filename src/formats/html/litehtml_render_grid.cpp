#include "litehtml_render_grid.h"

#include <litehtml/html_tag.h>

namespace litehtml {
	namespace {

		using context = containing_block_context;

		bool is_definite(context::typed_pixel value) {
			return is_one_of(value.type, context::cbc_value_type_absolute, context::cbc_value_type_percentage);
		}

		bool has_scrollable_overflow(const render_item& item) {
			return is_one_of(item.css().get_overflow(), overflow_hidden, overflow_scroll, overflow_auto);
		}

		pixel_t clamp_size(pixel_t size, context::typed_pixel min, context::typed_pixel max) {
			if (is_definite(max)) size = std::min(size, max.value);
			if (is_definite(min)) size = std::max(size, min.value);
			return std::max<pixel_t>(0, size);
		}

		context cell_context(const render_item& item, const context& grid, pixel_t width, std::optional<pixel_t> height) {
			context cell;
			cell.context_idx = grid.context_idx;
			cell.width = { width, context::cbc_value_type_absolute };
			cell.render_width = cell.width;
			if (height) cell.height = { *height, context::cbc_value_type_absolute };
			if (!item.src_el()->is_replaced())
			{
				if (item.css().get_width().is_predefined()) cell.size_mode |= context::size_mode_grid_stretch_width;
				if (height && item.css().get_height().is_predefined() &&
					!item.css().get_margins().top.is_predefined() && !item.css().get_margins().bottom.is_predefined())
					cell.size_mode |= context::size_mode_grid_stretch_height;
			}
			return cell;
		}

		std::vector<pixel_t> track_offsets(const std::vector<pixel_t>& tracks, pixel_t gap) {
			std::vector<pixel_t> offsets;
			offsets.reserve(tracks.size());
			pixel_t offset = 0;
			for (const auto track : tracks)
			{
				offsets.push_back(offset);
				offset += track + gap;
			}
			return offsets;
		}

		pixel_t track_end(const std::vector<pixel_t>& tracks, const std::vector<pixel_t>& offsets) {
			return tracks.empty() ? 0 : offsets.back() + tracks.back();
		}

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

		std::vector<std::shared_ptr<render_item>> items;
		for (const auto& child : m_children)
			if (child->src_el()->in_normal_flow()) items.push_back(child);

		auto columns = css().get_grid_template_columns();
		auto rows = css().get_grid_template_rows();
		if (columns.empty() && !items.empty()) columns.resize(1);
		if (!items.empty())
			rows.resize(std::max(rows.size(), 1 + (items.size() - 1) / columns.size()));

		std::vector<grid_track_contribution> column_content(columns.size());
		for (std::size_t index = 0; index < items.size(); ++index) {
			auto& content = column_content[index % columns.size()];
			if (!grid_track_needs_content(columns[index % columns.size()], definite_width)) continue;
			const auto& item = *items[index];
			// Ask the existing renderers for intrinsic widths with room for unwrapped content.
			context probe;
			probe.width = 1 << 20;
			probe.render_width = probe.width;
			probe.size_mode = context::size_mode_content | context::size_mode_grid_measure_width;
			probe.context_idx = self.context_idx;
			auto widths = items[index]->render(0, 0, probe, nullptr);
			const auto& width = item.css().get_width();
			const bool explicit_width = !width.is_predefined() && width.units() != css_units_percentage;
			const bool has_min_width = !item.css().get_min_width().is_predefined() &&
				item.css().get_min_width().units() != css_units_percentage;
			const auto min_width = has_min_width ? item.css().get_min_width().calc_percent(0) : 0;
			const auto outer_min_width = min_width + item.content_offset_width() - item.box_sizing_width();
			const bool nowrap = is_one_of(item.css().get_white_space(), white_space_nowrap, white_space_pre);
			const auto minimum = explicit_width ? item.width() : std::max(outer_min_width,
				has_scrollable_overflow(item) ? item.content_offset_width() :
				nowrap || item.src_el()->is_replaced() ? item.width() : std::min(widths.min_width, item.width()));
			content.minimum = std::max(content.minimum, minimum);
			content.preferred = std::max(content.preferred, item.width());
		}

		const auto initial_columns = size_grid_tracks(columns, column_content, column_gap.calc_percent(definite_width ? available_width : 0),
			definite_width ? std::optional<pixel_t>(available_width) : std::nullopt);
		m_pos.width = definite_width ? available_width : clamp_size(
			measure_width ? initial_columns.extent : std::clamp(available_width, initial_columns.minimum, initial_columns.extent),
			self.min_width, self.max_width);
		if (measure_width)
			return { m_pos.width + content_offset_width(), std::min(m_pos.width, initial_columns.minimum) + content_offset_width() };

		const auto horizontal_gap = column_gap.calc_percent(m_pos.width);
		const auto column_sizes = !definite_width ?
			size_grid_tracks(columns, column_content, horizontal_gap, m_pos.width) : initial_columns;

		std::vector<grid_track_contribution> row_content(rows.size());
		for (std::size_t index = 0; index < items.size(); ++index) {
			const auto row = index / columns.size();
			const auto width = column_sizes.tracks[index % columns.size()];
			if (!grid_track_needs_content(rows[row], definite_height) &&
				(definite_height || !grid_track_needs_content(rows[row], true))) continue;
			auto& item = *items[index];
			auto cell = cell_context(item, self, width, std::nullopt);
			cell.size_mode |= context::size_mode_grid_measure_height;
			item.render(0, 0, cell, nullptr);
			auto& content = row_content[row];
			const bool explicit_height = !item.css().get_height().is_predefined() &&
				item.css().get_height().units() != css_units_percentage;
			const auto& min_height = item.css().get_min_height();
			const bool has_min_height = !min_height.is_predefined() && min_height.units() != css_units_percentage;
			const auto minimum = !explicit_height && has_scrollable_overflow(item) ?
				item.content_offset_height() + (has_min_height ? std::max<pixel_t>(0, min_height.calc_percent(0) - item.box_sizing_height()) : 0) :
				item.height();
			content.minimum = std::max(content.minimum, minimum);
			content.preferred = std::max(content.preferred, item.height());
		}

		const auto initial_rows = size_grid_tracks(rows, row_content, row_gap.calc_percent(definite_height ? available_height : 0),
			definite_height ? std::optional<pixel_t>(available_height) : std::nullopt);
		m_pos.height = definite_height ? available_height : clamp_size(initial_rows.extent, self.min_height, self.max_height);
		const rendered_width widths{ m_pos.width + content_offset_width(),
			(definite_width && is_definite(self.width) ? m_pos.width : initial_columns.minimum) + content_offset_width() };
		if (containing_block_size.size_mode & context::size_mode_grid_measure_height) return widths;

		const auto vertical_gap = row_gap.calc_percent(m_pos.height);
		const bool resolve_rows = !definite_height &&
			(grid_tracks_use_percentage(rows) || m_pos.height != initial_rows.extent);
		const auto row_sizes = resolve_rows ? size_grid_tracks(rows, row_content, vertical_gap, m_pos.height) : initial_rows;
		const auto column_offsets = track_offsets(column_sizes.tracks, horizontal_gap);
		const auto row_offsets = track_offsets(row_sizes.tracks, vertical_gap);
		track_extent = { track_end(column_sizes.tracks, column_offsets), track_end(row_sizes.tracks, row_offsets) };

		for (std::size_t index = 0; index < items.size(); ++index) {
			const auto column = index % columns.size();
			const auto row = index / columns.size();
			const auto cell = cell_context(*items[index], self, column_sizes.tracks[column], row_sizes.tracks[row]);
			auto& item = *items[index];
			item.render(column_offsets[column], row_offsets[row], cell, nullptr);
			item.pos().x += item.calc_auto_margins(cell.width);
			const auto auto_top = item.css().get_margins().top.is_predefined();
			const auto auto_bottom = item.css().get_margins().bottom.is_predefined();
			if (auto_top) item.pos().y += std::max<pixel_t>(0, cell.height - item.height()) / (auto_bottom ? 2 : 1);
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
