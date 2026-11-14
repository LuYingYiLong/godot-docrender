#pragma once

#include <litehtml/render_block.h>

namespace litehtml {

	size size_grid_image(const render_item& item, const containing_block_context& self, const size& intrinsic);

	class render_item_grid : public render_item_block {
		size track_extent;

		rendered_width _render(pixel_t x, pixel_t y, const containing_block_context& containing_block_size,
			formatting_context* fmt_ctx, bool second_pass) override;
		size layout_overflow() const override { return track_extent; }

	public:
		explicit render_item_grid(const std::shared_ptr<element>& element) : render_item_block(element) {}
		std::shared_ptr<render_item> init() override;
		std::shared_ptr<render_item> clone() override { return std::make_shared<render_item_grid>(src_el()); }
	};
}
