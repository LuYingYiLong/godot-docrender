// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/docrender/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_value_parser.h"

#include <litehtml/css_parser.h>

namespace litehtml {

static bool parse_bg_image(const css_token& token, image& bg_image, document_container* container);
static bool parse_bg_position_size(const css_token_vector& tokens, int& index, css_length& x, css_length& y, css_size& size);
static bool parse_bg_size(const css_token_vector& tokens, int& index, css_size& size);
static bool parse_border_width(const css_token& token, css_length& width);

void css_value_parser::parse_background_and_border(string_id name, const css_token_vector& value,
	const string& baseurl, bool important, document_container* container)
{
	if (name == _id("box-shadow") || name == _id("text-shadow")) {
		std::vector<box_shadow> shadows;
		const bool parsed = name == _id("box-shadow") ? parse_box_shadows(value, container, shadows)
			: parse_text_shadows(value, container, shadows);
		if (parsed) {
			add_parsed_property(name, property_value(shadows, important));
		}
		return;
	}
	const css_token val = value.size() == 1 ? value[0] : css_token();
	const auto ident = val.ident();
	web_color clr[4];
	css_length len[4];
	int idx[4];
	switch (name)
	{
	//  =============================  COLOR  =============================

	case _color_:
		if (ident == "currentcolor") return add_parsed_property(name, property_value(inherit{}, important));
		[[fallthrough]];
	case _background_color_:
	case _border_top_color_:
	case _border_bottom_color_:
	case _border_left_color_:
	case _border_right_color_:
		if (parse_color(val, *clr, container))
			add_parsed_property(name, property_value(*clr, important));
		break;

	//  =============================  BACKGROUND  =============================

	case _background_:
		parse_background(value, baseurl, important, container);
		break;

	case _background_image_:
		parse_background_image(value, baseurl, important, container);
		break;

	case _background_position_:
		parse_background_position(value, important);
		break;

	case _background_size_:
		parse_background_size(value, important);
		break;

	case _background_repeat_:
	case _background_attachment_:
	case _background_origin_:
	case _background_clip_:
		parse_keyword_comma_list(name, value, important);
		break;

	//  =============================  BORDER  =============================

	case _border_:
		parse_border(value, important, container);
		break;

	case _border_left_:
	case _border_right_:
	case _border_top_:
	case _border_bottom_:
		parse_border_side(name, value, important, container);
		break;

	case _border_width_:
		if (int n = parse_1234_values(value, len, parse_border_width))
			add_four_properties(_border_top_width_, len, n, important);
		break;
	case _border_style_:
		if (int n = parse_1234_values(value, idx, parse_keyword, (string)border_style_strings, 0))
			add_four_properties(_border_top_style_, idx, n, important);
		break;
	case _border_color_:
		if (int n = parse_1234_values(value, clr, parse_color, container))
			add_four_properties(_border_top_color_, clr, n, important);
		break;

	case _border_top_width_:
	case _border_bottom_width_:
	case _border_left_width_:
	case _border_right_width_:
		if (parse_border_width(val, *len))
			add_parsed_property(name, property_value(*len, important));
		break;

	// border-bottom-left-radius = <length-percentage [0,∞]>{1,2}  https://developer.mozilla.org/en-US/docs/Web/CSS/border-bottom-left-radius
	case _border_bottom_left_radius_:
	case _border_bottom_right_radius_:
	case _border_top_right_radius_:
	case _border_top_left_radius_:
		if (parse_two_lengths(value, len, f_length_percentage | f_positive))
		{
			const auto& corners = css_longhands(name);
			add_parsed_property(corners[0], property_value(len[0], important));
			add_parsed_property(corners[1], property_value(len[1], important));
		}
		break;

	case _border_radius_x_:
	case _border_radius_y_:
	{
		string_id top_left = name == _border_radius_x_ ?
			_border_top_left_radius_x_ :
			_border_top_left_radius_y_;

		if (int n = parse_1234_lengths(value, len, f_length_percentage | f_positive))
			add_four_properties(top_left, len, n, important);
		break;
	}

	case _border_radius_:
		parse_border_radius(value, important);
		break;

	// border-spacing = <length>{1,2}  https://developer.mozilla.org/en-US/docs/Web/CSS/border-spacing
	// Lengths may not be negative.  https://drafts.csswg.org/css2/#separated-borders
	case _border_spacing_:
		if (parse_two_lengths(value, len, f_length | f_positive))
		{
			add_parsed_property(__litehtml_border_spacing_x_, property_value(len[0], important));
			add_parsed_property(__litehtml_border_spacing_y_, property_value(len[1], important));
		}
		break;

	//  =============================  LIST  =============================

	case _list_style_image_:
		if (string url; parse_list_style_image(val, url))
		{
			add_parsed_property(_list_style_image_,			property_value(url,     important));
			add_parsed_property(_list_style_image_baseurl_, property_value(baseurl, important));
		}
		break;

	case _list_style_:
		parse_list_style(value, baseurl, important);
		break;
	default:
		break;
	}
}

bool css_value_parser::parse_list_style_image(const css_token& tok, string& url)
{
	if (tok.ident() == "none")
	{
		url = "";
		return true;
	}

	return parse_url(tok, url);
}

// https://drafts.csswg.org/css-lists/#list-style-property
// <'list-style-position'> || <'list-style-image'> || <'list-style-type'>
void css_value_parser::parse_list_style(const css_token_vector& tokens, string baseurl, bool important)
{
	// initial values:  https://developer.mozilla.org/en-US/docs/Web/CSS/list-style#formal_definition
	int type     = list_style_type_disc;
	int position = list_style_position_outside;
	string image = ""; // none

	bool type_found = false;
	bool position_found = false;
	bool image_found = false;
	int none_count = 0;

	for (const auto& token : tokens)
	{
		// "...none is a valid value for both list-style-image and list-style-type. To resolve this ambiguity,
		// a value of none ... must be applied to whichever of the two properties aren’t otherwise set by the shorthand."
		if (token.ident() == "none") {
			none_count++;
			continue;
		}
		if (!type_found && parse_keyword(token, type, list_style_type_strings))
			type_found = true;
		else if (!position_found && parse_keyword(token, position, list_style_position_strings))
			position_found = true;
		else if (!image_found && parse_list_style_image(token, image))
			image_found = true;
		else
			return; // syntax error
	}

	switch (none_count)
	{
	case 0:
		break;
	case 1:
		if (type_found && image_found) return;
		if (!type_found) type = list_style_type_none;
		break;
	case 2:
		if (type_found || image_found) return;
		type = list_style_type_none;
		break;
	default:
		return; // syntax error
	}

	add_parsed_property(_list_style_type_,			property_value(type,	 important));
	add_parsed_property(_list_style_position_,		property_value(position, important));
	add_parsed_property(_list_style_image_,			property_value(image,	 important));
	add_parsed_property(_list_style_image_baseurl_,	property_value(baseurl,	 important));
}

// https://developer.mozilla.org/en-US/docs/Web/CSS/border-radius
// border-radius = <length-percentage [0,∞]>{1,4} [ / <length-percentage [0,∞]>{1,4} ]?
void css_value_parser::parse_border_radius(const css_token_vector& tokens, bool important)
{
	int i;
	for (i = 0; i < (int)tokens.size() && tokens[i].ch != '/'; i++) {}

	if (i == (int)tokens.size()) // no '/'
	{
		css_length len[4];
		if (int n = parse_1234_lengths(tokens, len, f_length_percentage | f_positive))
		{
			add_four_properties(_border_top_left_radius_x_, len, n, important);
			add_four_properties(_border_top_left_radius_y_, len, n, important);
		}
	}
	else
	{
		auto raduis_x = slice(tokens, 0, i);
		auto raduis_y = slice(tokens, i + 1);

		css_length rx[4], ry[4];
		int n = parse_1234_lengths(raduis_x, rx, f_length_percentage | f_positive);
		int m = parse_1234_lengths(raduis_y, ry, f_length_percentage | f_positive);

		if (n && m)
		{
			add_four_properties(_border_top_left_radius_x_, rx, n, important);
			add_four_properties(_border_top_left_radius_y_, ry, m, important);
		}
	}
}

bool parse_border_width(const css_token& token, css_length& w)
{
	css_length width;
	if (!width.from_token(token, f_length | f_positive, border_width_strings))
		return false;

	if (width.is_predefined())
		width.set_value(border_width_values[width.predef()], css_units_px);

	w = width;
	return true;
}

// https://drafts.csswg.org/css-backgrounds/#propdef-border
// <line-width> || <line-style> || <color>
// <line-width> = <length [0,∞]> | thin | medium | thick
// <line-style> = none | hidden | dotted | dashed | solid | double | groove | ridge | inset | outset
bool parse_border_helper(const css_token_vector& tokens, document_container* container,
	css_length& width, border_style& style, web_color& color)
{
	// initial values:  https://developer.mozilla.org/en-US/docs/Web/CSS/border#formal_definition
	css_length   _width = border_width_medium_value;
	border_style _style = border_style_none;
	web_color    _color = web_color::current_color;

	bool width_found = false;
	bool style_found = false;
	bool color_found = false;

	for (const auto& token : tokens)
	{
		if (!width_found && parse_border_width(token, _width))
			width_found = true;
		else if (!style_found && parse_keyword(token, _style, border_style_strings))
			style_found = true;
		else if (!color_found && parse_color(token, _color, container))
			color_found = true;
		else
			return false;
	}

	width = _width;
	style = _style;
	color = _color;
	return true;
}

void css_value_parser::parse_border(const css_token_vector& tokens, bool important, document_container* container)
{
	css_length width;
	border_style style;
	web_color color;

	if (!parse_border_helper(tokens, container, width, style, color))
		return;

	for (auto name : {_border_left_width_, _border_right_width_, _border_top_width_, _border_bottom_width_})
		add_parsed_property(name, property_value(width, important));

	for (auto name : {_border_left_style_, _border_right_style_, _border_top_style_, _border_bottom_style_})
		add_parsed_property(name, property_value(style, important));

	for (auto name : {_border_left_color_, _border_right_color_, _border_top_color_, _border_bottom_color_})
		add_parsed_property(name, property_value(color, important));
}

// https://drafts.csswg.org/css-backgrounds/#border-shorthands
// border-top, border-right, border-bottom, border-left
void css_value_parser::parse_border_side(string_id name, const css_token_vector& tokens, bool important, document_container* container)
{
	css_length width;
	border_style style;
	web_color color;

	if (!parse_border_helper(tokens, container, width, style, color))
		return;

	const auto& side = css_longhands(name);
	add_parsed_property(side[0], property_value(width, important));
	add_parsed_property(side[1], property_value(static_cast<int>(style), important));
	add_parsed_property(side[2], property_value(color, important));
}

void css_value_parser::parse_background(const css_token_vector& tokens, const string& baseurl, bool important, document_container* container)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	web_color color;
	std::vector<image> images;
	length_vector x_positions, y_positions;
	size_vector sizes;
	int_vector repeats, attachments, origins, clips;

	for (size_t i = 0; i < layers.size(); i++)
	{
		background bg;
		if (!parse_bg_layer(layers[i], container, bg, i == layers.size() - 1))
			return;

		color = bg.m_color;
		images.push_back(bg.m_image[0]);
		x_positions.push_back(bg.m_position_x[0]);
		y_positions.push_back(bg.m_position_y[0]);
		sizes.push_back(bg.m_size[0]);
		repeats.push_back(bg.m_repeat[0]);
		attachments.push_back(bg.m_attachment[0]);
		origins.push_back(bg.m_origin[0]);
		clips.push_back(bg.m_clip[0]);
	}

	add_parsed_property(_background_color_,			property_value(color,		important));
	add_parsed_property(_background_image_,			property_value(images,		important));
	add_parsed_property(_background_image_baseurl_, property_value(baseurl,		important));
	add_parsed_property(_background_position_x_,	property_value(x_positions, important));
	add_parsed_property(_background_position_y_,	property_value(y_positions, important));
	add_parsed_property(_background_size_,			property_value(sizes,		important));
	add_parsed_property(_background_repeat_,		property_value(repeats,		important));
	add_parsed_property(_background_attachment_,	property_value(attachments, important));
	add_parsed_property(_background_origin_,		property_value(origins,		important));
	add_parsed_property(_background_clip_,			property_value(clips,		important));
}

// https://drafts.csswg.org/css-backgrounds/#typedef-bg-layer
// <bg-layer> = <bg-image> || <bg-position> [ / <bg-size> ]? || <repeat-style> || <attachment> || <visual-box> || <visual-box>
// <final-bg-layer> =  <bg-layer> || <'background-color'>
bool css_value_parser::parse_bg_layer(const css_token_vector& tokens, document_container* container, background& bg, bool final_layer)
{
	bg.m_color = web_color::transparent;
	bg.m_image = {{}};
	bg.m_position_x = { css_length(0, css_units_percentage) };
	bg.m_position_y = { css_length(0, css_units_percentage) };
	bg.m_size = { css_size(css_length::predef_value(background_size_auto), css_length::predef_value(background_size_auto)) };
	bg.m_repeat = { background_repeat_repeat };
	bg.m_attachment = { background_attachment_scroll };
	bg.m_origin = { background_box_padding };
	bg.m_clip = { background_box_border };

	bool color_found = false;
	bool image_found = false;
	bool position_found = false;
	bool repeat_found = false;
	bool attachment_found = false;
	bool origin_found = false;
	bool clip_found = false;

	if (tokens.empty()) return false;
	for (int i = 0; i < (int)tokens.size(); i++)
	{
		if (!color_found && final_layer && parse_color(tokens[i], bg.m_color, container))
			color_found = true;
		else if (!image_found && parse_bg_image(tokens[i], bg.m_image[0], container))
			image_found = true;
		else if (!position_found && parse_bg_position_size(tokens, i, bg.m_position_x[0], bg.m_position_y[0], bg.m_size[0]))
			position_found = true, i--;
		// Note: double keyword <repeat-style> values are not supported yet.
		else if (!repeat_found && parse_keyword(tokens[i], bg.m_repeat[0], background_repeat_strings))
			repeat_found = true;
		else if (!attachment_found && parse_keyword(tokens[i], bg.m_attachment[0], background_attachment_strings))
			attachment_found = true;
		// If one <visual-box> value is present then it sets both background-origin and background-clip to that value.
		// If two values are present, then the first sets background-origin and the second background-clip.
		else if (!origin_found && parse_keyword(tokens[i], bg.m_origin[0], background_box_strings))
			origin_found = true, bg.m_clip[0] = bg.m_origin[0];
		else if (!clip_found && parse_keyword(tokens[i], bg.m_clip[0], background_box_strings))
			clip_found = true;
		else
			return false;
	}
	return true;
}

// <bg-position> [ / <bg-size> ]?
bool parse_bg_position_size(const css_token_vector& tokens, int& index, css_length& x, css_length& y, css_size& size)
{
	int end = index;
	if (!parse_bg_position(tokens, end, x, y, true))
		return false;

	if (at(tokens, end).ch == '/' && !parse_bg_size(tokens, ++end, size))
		return false;

	index = end;
	return true;
}

// https://drafts.csswg.org/css-backgrounds/#typedef-bg-size
// <bg-size> = [ <length-percentage [0,∞]> | auto ]{1,2} | cover | contain
bool parse_bg_size(const css_token_vector& tokens, int& index, css_size& size)
{
	css_length a, b;

	if (!a.from_token(at(tokens, index), f_length_percentage | f_positive, background_size_strings))
		return false;

	// cover | contain
	if (a.is_predefined() && a.predef() != background_size_auto)
	{
		size.width = size.height = a;
		index++;
		return true;
	}

	if (b.from_token(at(tokens, index + 1), f_length_percentage | f_positive, "auto"))
		index += 2;
	else
	{
		b.predef(background_size_auto); // If only one value is given the second is assumed to be auto.
		index++;
	}

	size.width = a;
	size.height = b;
	return true;
}

bool is_one_of_predef(const css_length& x, int idx1, int idx2)
{
	return x.is_predefined() && is_one_of(x.predef(), idx1, idx2);
}

// https://drafts.csswg.org/css-backgrounds/#typedef-bg-position
// https://www.w3.org/TR/css-values-4/#position
// <bg-position> = [ left | center | right | top | bottom | <length-percentage> ] |
//   [ left | center | right | <length-percentage> ]  [ top | center | bottom | <length-percentage> ]
// Side-relative values (like bottom 10%) are not supported, so only 1 or 2 values are parsed (not 3 or 4).
bool parse_bg_position(const css_token_vector& tokens, int& index, css_length& x, css_length& y, bool convert_keywords_to_percents)
{
	enum {
		left   = background_position_left,
		right  = background_position_right,
		top    = background_position_top,
		bottom = background_position_bottom,
		center = background_position_center
	};

	css_length a, b;

	if (!a.from_token(at(tokens, index), f_length_percentage, background_position_strings))
		return false;

	if (!b.from_token(at(tokens, index + 1), f_length_percentage, background_position_strings))
	{
		// If only one value is specified, the second value is assumed to be center.
		b.predef(center);

		// fix wrong order
		if (is_one_of_predef(a, top, bottom))
			swap(a, b);

		index++;
	}
	else // two values
	{
		// try to fix wrong order
		// A pair of keywords can be reordered, while a combination of keyword and length or percentage cannot.
		if ((is_one_of_predef(a, top, bottom) && b.is_predefined()) ||
			(a.is_predefined() && is_one_of_predef(b, left, right)))
			swap(a, b);

		// check for wrong order
		if (is_one_of_predef(a, top, bottom) || is_one_of_predef(b, left, right))
			return false;

		index += 2;
	}

	if (convert_keywords_to_percents)
	{
		if (a.is_predefined())
			a.set_value(background_position_percentages[a.predef()], css_units_percentage);
		if (b.is_predefined())
			b.set_value(background_position_percentages[b.predef()], css_units_percentage);
	}

	x = a;
	y = b;
	return true;
}

void css_value_parser::parse_background_image(const css_token_vector& tokens, const string& baseurl, bool important, document_container* container)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	std::vector<image> images;

	for (const auto& layer : layers)
	{
		image image;
		if (layer.size() != 1)
			return;
		if (!parse_bg_image(layer[0], image, container))
			return;
		images.push_back(image);
	}

	add_parsed_property(_background_image_,			property_value(images, important));
	add_parsed_property(_background_image_baseurl_, property_value(baseurl, important));
}

// <bg-image> = <image> | none
// <image> = <url> | <gradient>
bool parse_bg_image(const css_token& tok, image& bg_image, document_container* container)
{
	if (tok.ident() == "none")
	{
		bg_image.type = image::type_none;
		return true;
	}

	string url;
	if (parse_url(tok, url))
	{
		bg_image.type = image::type_url;
		bg_image.url = url;
		return true;
	}

	if (parse_gradient(tok, bg_image.m_gradient, container))
	{
		bg_image.type = image::type_gradient;
		return true;
	}

	return false;
}

// https://drafts.csswg.org/css-values-4/#urls
bool parse_url(const css_token& tok, string& url)
{
	if (tok.type == URL) // legacy syntax without quotes: url(x.com)
	{
		url = trim(tok.str);
		return true;
	}

	if (tok.type == CV_FUNCTION && is_one_of(lowcase(tok.name), "url", "src") &&
		// note: relying on whitespace having been removed from tok.value
		tok.value.size() == 1 && tok.value[0].type == STRING)
	{
		url = trim(tok.value[0].str);
		return true;
	}

	return false;
}

void css_value_parser::parse_keyword_comma_list(string_id name, const css_token_vector& tokens, bool important)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	int_vector vec;

	for (const auto& layer : layers)
	{
		int idx;
		if (layer.size() != 1) return;
		if (!parse_keyword(layer[0], idx, css_keywords(name))) return;
		vec.push_back(idx);
	}

	add_parsed_property(name, property_value(vec, important));
}

void css_value_parser::parse_background_position(const css_token_vector& tokens, bool important)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	length_vector x_positions, y_positions;

	for (const auto& layer : layers)
	{
		css_length x, y;
		int index = 0;
		if (!parse_bg_position(layer, index, x, y, true) || index != (int)layer.size())
			return;

		x_positions.push_back(x);
		y_positions.push_back(y);
	}

	add_parsed_property(_background_position_x_, property_value(x_positions, important));
	add_parsed_property(_background_position_y_, property_value(y_positions, important));
}

void css_value_parser::parse_background_size(const css_token_vector& tokens, bool important)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	size_vector sizes;

	for (const auto& layer : layers)
	{
		css_size size;
		int index = 0;
		if (!parse_bg_size(layer, index, size) || index != (int)layer.size())
			return;

		sizes.push_back(size);
	}

	add_parsed_property(_background_size_, property_value(sizes, important));
}

} // namespace litehtml
