#pragma once

#include <litehtml/render_block.h>

namespace litehtml {

	size size_grid_image(const render_item& item, const containing_block_context& self, const size& intrinsic);

	class render_item_grid : public render_item_block {
		size track_extent;
		pixel_t occupied_row_height = 0;
		std::optional<pixel_t> first_baseline;
		std::optional<pixel_t> last_baseline;

		rendered_width _render(pixel_t x, pixel_t y, const containing_block_context& containing_block_size,
			formatting_context* fmt_ctx, bool second_pass) override;
		size layout_overflow() const override { return track_extent; }
		std::optional<pixel_t> find_layout_baseline(bool last) const override;

	public:
		explicit render_item_grid(const std::shared_ptr<element>& element) : render_item_block(element) {}
		pixel_t get_first_baseline() override { return baseline_or_border_bottom(false); }
		pixel_t get_last_baseline() override { return baseline_or_border_bottom(true); }
		pixel_t get_inline_baseline() override { return layout_baseline(false).value_or(height()); }
		pixel_t baseline_content_height() const override { return occupied_row_height; }
		std::shared_ptr<render_item> init() override;
		std::shared_ptr<render_item> clone() override { return std::make_shared<render_item_grid>(src_el()); }
	};
}
