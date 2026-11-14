#pragma once

#include <litehtml/css_length.h>

#include <array>

namespace litehtml {
	class document;

	struct css_transform_function {
		enum class kind { translate, rotate, scale, skew, matrix };

		kind type;
		css_length x;
		css_length y;
		double first = 0;
		double second = 0;
		std::array<double, 6> matrix{};
	};

	using css_transform_list = std::vector<css_transform_function>;

	struct css_transform_origin {
		css_length x{ 50, css_units_percentage };
		css_length y{ 50, css_units_percentage };
	};

	bool parse_css_transform(const css_token_vector& p_tokens, css_transform_list& r_value);
	bool parse_css_transform_origin(const css_token_vector& p_tokens, css_transform_origin& r_value);
}
