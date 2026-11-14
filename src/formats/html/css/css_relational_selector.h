#pragma once

#include <litehtml/css_selector.h>

namespace litehtml {
	class element;

	bool matches_relative_selector(const element& p_anchor, const css_selector::vector& p_selectors);
}
