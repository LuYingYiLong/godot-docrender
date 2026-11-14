#pragma once

#include <litehtml/css_length.h>

#include <memory>

namespace litehtml {
	class document;
	struct css_math_node;
	using css_math_node_ptr = std::shared_ptr<const css_math_node>;

	struct css_math_expression {
		css_math_node_ptr root;
		bool has_percentage;
		bool non_negative;

		double evaluate(double p_basis) const;
		string to_string() const;
	};

	std::shared_ptr<const css_math_expression> parse_css_math(const css_token& p_token, int p_options);
	std::shared_ptr<const css_math_expression> compute_css_math(const css_math_expression& p_value,
		const document& p_document, const font_metrics& p_metrics);
}
