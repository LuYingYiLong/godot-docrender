// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/docrender/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_value_parser.h"

#include <litehtml/css_parser.h>

#include <cmath>

namespace litehtml {

void css_value_parser::parse_box(string_id name, const css_token_vector& value, bool important)
{
	const css_token val = value.size() == 1 ? value.front() : css_token();
	css_length len[4];
	switch (name)
	{
	case _z_index_:
		return add_length_property(name, val, "auto", f_integer, important);

	// <length-percentage>  https://developer.mozilla.org/en-US/docs/Web/CSS/text-indent#formal_syntax
	case _text_indent_:
		return add_length_property(name, val, "", f_length_percentage, important);

	// <length-percentage [0,∞]>  https://developer.mozilla.org/en-US/docs/Web/CSS/padding-left
	case _padding_left_:
	case _padding_right_:
	case _padding_top_:
	case _padding_bottom_:
		return add_length_property(name, val, "", f_length_percentage|f_positive|f_math, important);

	// auto | <length-percentage>  https://developer.mozilla.org/en-US/docs/Web/CSS/left
	case _left_:
	case _right_:
	case _top_:
	case _bottom_:
	case _margin_left_:
	case _margin_right_:
	case _margin_top_:
	case _margin_bottom_:
		return add_length_property(name, val, "auto", f_length_percentage|f_math, important);

	// auto | min-content | max-content | fit-content | <length-percentage [0,∞]>   https://developer.mozilla.org/en-US/docs/Web/CSS/width#formal_syntax
	case _width_:
	case _height_:
	case _min_width_:
	case _min_height_:
		return add_length_property(name, val, "auto", f_length_percentage|f_positive|f_math, important);

	// none | min-content | max-content | fit-content | <length-percentage [0,∞]>   https://developer.mozilla.org/en-US/docs/Web/CSS/max-width#formal_syntax
	case _max_width_:
	case _max_height_:
		return add_length_property(name, val, "none", f_length_percentage|f_positive|f_math, important);

	//  normal | <number [0,∞]> | <length-percentage [0,∞]>   https://developer.mozilla.org/en-US/docs/Web/CSS/line-height#formal_syntax
	case _line_height_:
		return add_length_property(name, val, "normal", f_number|f_length_percentage|f_positive, important);

	// font-size = <absolute-size> | <relative-size> | <length-percentage [0,∞]>   https://developer.mozilla.org/en-US/docs/Web/CSS/font-size#formal_syntax
	case _font_size_:
		return add_length_property(name, val, font_size_strings, f_length_percentage|f_positive, important);

	case _margin_:
		if (int n = parse_1234_lengths(value, len, f_length_percentage | f_math, "auto"))
			add_four_properties(_margin_top_, len, n, important);
		break;

	case _padding_:
		if (int n = parse_1234_lengths(value, len, f_length_percentage | f_positive | f_math))
			add_four_properties(_padding_top_, len, n, important);
		break;
	default:
		break;
	}
}

void css_value_parser::add_length_property(string_id name, css_token val, string keywords, int options, bool important)
{
	css_length length;
	if (length.from_token(val, options, keywords) && (length.is_predefined() || std::isfinite(length.val())))
		add_parsed_property(name, property_value(length, important));
}

bool parse_length(const css_token& tok, css_length& length, int options, string keywords)
{
	return length.from_token(tok, options, keywords);
}

// parses 1 or 2 lengths, but always returns 2 lengths
bool parse_two_lengths(const css_token_vector& tokens, css_length len[2], int options)
{
	auto n = tokens.size();
	if (n != 1 && n != 2) return false;

	css_length a, b;
	if (!a.from_token(tokens[0], options)) return false;
	if (n == 1) b = a;

	if (n == 2 && !b.from_token(tokens[1], options)) return false;

	len[0] = a;
	len[1] = b;
	return true;
}

int parse_1234_lengths(const css_token_vector& tokens, css_length len[4], int options, string keywords)
{
	return parse_1234_values(tokens, len, parse_length, options, keywords);
}

} // namespace litehtml
