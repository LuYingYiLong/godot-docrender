// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/docrender/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_value_parser.h"

#include <litehtml/css_parser.h>

namespace litehtml {

static bool parse_font_weight(const css_token& token, css_length& weight);

namespace {
	std::optional<string> parse_emphasis_style(const css_token_vector& tokens) {
		if (tokens.size() == 1 && (tokens[0].type == STRING || tokens[0].ident() == "none")) {
			return tokens[0].get_repr(true);
		}
		bool fill = false;
		bool shape = false;
		for (const auto& token : tokens) {
			const auto word = token.ident();
			if (!fill && is_one_of(word, "filled", "open")) {
				fill = true;
			}
			else if (!shape && is_one_of(word, "dot", "circle", "double-circle", "triangle", "sesame")) {
				shape = true;
			}
			else {
				return std::nullopt;
			}
		}
		if (!fill && !shape) {
			return std::nullopt;
		}
		return get_repr(tokens, 0, -1, true);
	}
}

void css_value_parser::parse_text(string_id name, const css_token_vector& value, bool important, document_container* container)
{
	const css_token val = value.size() == 1 ? value.front() : css_token();
	css_length length;
	switch (name)
	{
	case _font_:
		parse_font(value, important);
		break;

	case _font_family_:
	{
		css_font_families families;
		if (parse_font_families(value, families))
			add_parsed_property(name, property_value(families, important));
		break;
	}

	case _font_weight_:
		if (parse_font_weight(val, length))
			add_parsed_property(name, property_value(length, important));
		break;

	case _text_decoration_:
		parse_text_decoration(value, important, container);
		break;

	case _text_decoration_thickness_:
		add_length_property(name, val, style_text_decoration_thickness_strings, f_length_percentage|f_positive, important);
		break;

	case _text_decoration_color_:
		parse_text_decoration_color(val, important, container);
		break;

	case _text_decoration_line_:
		parse_text_decoration_line(value, important);
		break;

	case _text_emphasis_:
		parse_text_emphasis(value, important, container);
		break;

	case _text_emphasis_style_:
		if (const auto emphasis = parse_emphasis_style(value))
			add_parsed_property(name, property_value(*emphasis, important));
		break;

	case _text_emphasis_color_:
		parse_text_emphasis_color(val, important, container);
		break;

	case _text_emphasis_position_:
		parse_text_emphasis_position(value, important);
		break;
	default:
		break;
	}
}

static bool parse_font_weight(const css_token& tok, css_length& weight)
{
	if (int idx = value_index(tok.ident(), font_weight_strings); idx >= 0)
	{
		weight.predef(idx);
		return true;
	}

	// https://drafts.csswg.org/css-fonts/#font-weight-absolute-values
	// Note: fractional values are allowed.
	if (tok.type == NUMBER && tok.n.number >= 1 && tok.n.number <= 1000)
	{
		weight.set_value(tok.n.number, css_units_none);
		return true;
	}

	return false;
}

// <font-style> || <font-variant-css2> || <font-weight>
// None of the allowed values intersect with <font-size>, it cannot consume <font-size>.
// Note: <font-weight> can be a number >=1, but <font-size> inside <font> is <length-percentage>,
// so it can be only 0 number. <font-size> as a standalone property does allow <number>s in quirks mode.
bool parse_font_style_variant_weight(const css_token_vector& tokens, int& index,
	int& style, int& variant, css_length& weight)
{
	bool style_found = false;
	bool variant_found = false;
	bool weight_found = false;
	bool res = false;

	int i = index, count = 0;
	while (i < (int)tokens.size() && count++ < 3)
	{
		const auto& tok = tokens[i++];
		// All three properties can have value "normal", and it changes nothing because initial
		// values of all three properties are "normal".
		if (tok.ident() == "normal")
		{
			index++;
			res = true;
		} else if (!style_found && parse_keyword(tok, style, font_style_strings))
		{
			style_found = true;
			index++;
			res = true;
		}
		else if (!variant_found && parse_keyword(tok, variant, font_variant_strings))
		{
			variant_found = true;
			index++;
			res = true;
		}
		else if (!weight_found && parse_font_weight(tok, weight))
		{
			weight_found = true;
			index++;
			res = true;
		} else break;
	}
	return res;
}

// https://developer.mozilla.org/en-US/docs/Web/CSS/font
// https://drafts.csswg.org/css-fonts/#font-prop
// font = <font-style-weight>? <font-size> [ / <line-height> ]? <font-family>
// font = <system-family-name*>
// <font-style-weight> = <font-style> || <font-variant-css2> || <font-weight> || <font-width-css3*>
// values marked * are not supported
void css_value_parser::parse_font(css_token_vector tokens, bool important)
{
	// initial values
	int style   = font_style_normal;
	int variant = font_variant_normal;
	css_length weight      = css_length::predef_value(font_weight_normal);
	css_length size        = css_length::predef_value(font_size_medium);
	css_length line_height = css_length::predef_value(line_height_normal);
	css_font_families font_family;

	if (tokens.size() == 1 && value_in_list(tokens[0].ident(), font_system_family_name_strings))
	{
		font_family.push_back({ "system-ui", true });
	} else
	{
		int index = 0;
		parse_font_style_variant_weight(tokens, index, style, variant, weight);

		// font-size = <absolute-size> | <relative-size> | <length-percentage [0,∞]> | math
		if (!size.from_token(at(tokens, index), f_length_percentage | f_positive, font_size_strings))
			return;
		index++;

		if (at(tokens, index).ch == '/')
		{
			index++;
			// https://drafts.csswg.org/css2/#propdef-line-height
			// line-height = normal | <number> | <length> | <percentage>
			if (!line_height.from_token(at(tokens, index), f_number | f_length_percentage | f_positive, line_height_strings))
				return;
			index++;
		}

		remove(tokens, 0, index);
		if (!parse_font_families(tokens, font_family))
			return;
	}
	add_parsed_property(_font_style_,   property_value(style,       important));
	add_parsed_property(_font_variant_, property_value(variant,     important));
	add_parsed_property(_font_weight_,  property_value(weight,      important));
	add_parsed_property(_font_size_,    property_value(size,        important));
	add_parsed_property(_line_height_,  property_value(line_height, important));
	add_parsed_property(_font_family_,  property_value(font_family, important));
}

void css_value_parser::parse_text_decoration(const css_token_vector& tokens, bool important, document_container* container)
{
	text_decoration decoration;
	if (!parse_decoration(tokens, container, decoration)) return;
	add_parsed_property(_text_decoration_line_, property_value(decoration.line, important));
	add_parsed_property(_text_decoration_style_, property_value(static_cast<int>(decoration.style), important));
	add_parsed_property(_text_decoration_thickness_, property_value(decoration.thickness, important));
	add_parsed_property(_text_decoration_color_, property_value(decoration.color, important));
}

void css_value_parser::parse_text_decoration_color(const css_token& token, bool important, document_container* container)
{
	web_color _color;
	if(parse_color(token, _color, container))
	{
		add_parsed_property(_text_decoration_color_, property_value(_color, important));
		return;
	}
	if(token.type == IDENT && value_in_list(token.ident(), "auto;currentcolor"))
	{
		add_parsed_property(_text_decoration_color_, property_value(web_color::current_color, important));
		return;
	}
}

void css_value_parser::parse_text_decoration_line(const css_token_vector& tokens, bool important)
{
	int lines;
	if (parse_decoration_line(tokens, lines))
		add_parsed_property(_text_decoration_line_, property_value(lines, important));
}

void css_value_parser::parse_text_emphasis(const css_token_vector& tokens, bool important, document_container* container)
{
	css_token_vector style_tokens;
	web_color color = web_color::current_color;
	bool has_color = false;
	for (const auto& token : tokens)
	{
		if (!has_color && parse_color(token, color, container)) has_color = true;
		else style_tokens.push_back(token);
	}
	const auto style = style_tokens.empty() && has_color ? std::optional<string>("none") : parse_emphasis_style(style_tokens);
	if (!style) return;
	add_parsed_property(_text_emphasis_style_, property_value(*style, important));
	add_parsed_property(_text_emphasis_color_, property_value(color, important));
}

void css_value_parser::parse_text_emphasis_color(const css_token& token, bool important, document_container* container)
{
	web_color color = web_color::current_color;
	if (token.ident() == "auto" || parse_color(token, color, container))
		add_parsed_property(_text_emphasis_color_, property_value(color, important));
}

void css_value_parser::parse_text_emphasis_position(const css_token_vector& tokens, bool important)
{
	int position = 0;
	bool vertical = false;
	bool horizontal = false;
	for (const auto& token : tokens)
	{
		const auto word = token.ident();
		const bool is_vertical = is_one_of(word, "over", "under");
		const bool is_horizontal = is_one_of(word, "left", "right");
		if ((!is_vertical && !is_horizontal) || (is_vertical && vertical) || (is_horizontal && horizontal)) return;
		vertical |= is_vertical;
		horizontal |= is_horizontal;
		position |= 1 << (value_index(word, style_text_emphasis_position_strings) - 1);
	}
	if (vertical) add_parsed_property(_text_emphasis_position_, property_value(position, important));
}

} // namespace litehtml
