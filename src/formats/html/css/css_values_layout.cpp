// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/docrender/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_value_parser.h"

#include <litehtml/css_parser.h>

#include <cmath>
#include <limits>

namespace litehtml {

namespace {
	std::optional<string_vector> parse_counters(const css_token_vector& tokens) {
		if (tokens.size() == 1 && tokens.front().ident() == "none") {
			return string_vector();
		}
		string_vector result;
		for (std::size_t index = 0; index < tokens.size();) {
			const auto& name = tokens[index++];
			if (name.type != IDENT || is_one_of(name.ident(), "none", "inherit", "initial", "unset", "revert", "revert-layer", "default")) {
				return std::nullopt;
			}
			result.push_back(name.name);
			if (index < tokens.size() && tokens[index].type == NUMBER) {
				const auto& count = tokens[index++];
				const double number = count.n.number;
				if (count.n.number_type != css_number_integer ||
					!std::isfinite(number) || number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
					return std::nullopt;
				}
				result.push_back(std::to_string(static_cast<int>(number)));
			}
		}
		return result;
	}

	bool is_content(const css_token_vector& tokens) {
		if (tokens.size() == 1 && is_one_of(tokens.front().ident(), "none", "normal")) {
			return true;
		}
		return !tokens.empty() && std::all_of(tokens.begin(), tokens.end(), [](const css_token& token) {
			if (token.type == STRING || is_one_of(token.ident(), "open-quote", "close-quote", "no-open-quote", "no-close-quote")) {
				return true;
			}
			if (token.type == URL) {
				return true;
			}
			return token.type == CV_FUNCTION && !token.value.empty() &&
				is_one_of(lowcase(token.name), "attr", "counter", "counters", "url");
		});
	}
}

void css_value_parser::parse_layout(string_id name, const css_token_vector& value, bool important)
{
	if (is_one_of(name, _gap_, _row_gap_, _column_gap_)) {
		parse_gap(name, value, important);
		return;
	}
	const css_token val = value.size() == 1 ? value.front() : css_token();
	switch (name)
	{
	case _flex_:
		parse_flex(value, important);
		break;

	case _flex_grow_:   // <number [0,∞]>
	case _flex_shrink_:
		if (val.type == NUMBER && val.n.number >= 0 && std::isfinite(val.n.number))
			add_parsed_property(name, property_value(val.n.number, important));
		break;

	case _flex_basis_:
		add_length_property(name, val, flex_basis_strings, f_length_percentage|f_positive|f_math, important);
		break;

	case _flex_flow_:
		parse_flex_flow(value, important);
		break;

	case _justify_items_:
	case _align_items_:
	case _justify_self_:
	case _align_self_:
	case _justify_content_:
	case _align_content_:
	{
		auto alignment = initial_alignment(name);
		if (parse_alignment(name, value, alignment))
			add_parsed_property(name, property_value(alignment, important));
		break;
	}
	case _place_items_:
	case _place_self_:
	case _place_content_:
	{
		const auto& longhands = css_longhands(name);
		auto align = initial_alignment(longhands[0]);
		auto justify = initial_alignment(longhands[1]);
		if (!parse_place_alignment(longhands[0], longhands[1], value, align, justify)) break;
		add_parsed_property(longhands[0], property_value(align, important));
		add_parsed_property(longhands[1], property_value(justify, important));
		break;
	}

	case _transform_:
	{
		css_transform_list transforms;
		if (parse_css_transform(value, transforms))
			add_parsed_property(name, property_value(std::move(transforms), important));
		break;
	}
	case _transform_origin_:
	{
		css_transform_origin origin;
		if (parse_css_transform_origin(value, origin))
			add_parsed_property(name, property_value(origin, important));
		break;
	}
	case _grid_template_:
	case _grid_:
	{
		grid_definition definition;
		if (!parse_grid_definition(value, name == _grid_, definition)) break;
		add_parsed_property(_grid_template_rows_, property_value(std::move(definition.rows), important));
		add_parsed_property(_grid_template_columns_, property_value(std::move(definition.columns), important));
		add_parsed_property(_grid_template_areas_, property_value(std::move(definition.areas), important));
		if (name == _grid_)
		{
			add_parsed_property(_grid_auto_rows_, property_value(std::move(definition.auto_rows), important));
			add_parsed_property(_grid_auto_columns_, property_value(std::move(definition.auto_columns), important));
			add_parsed_property(_grid_auto_flow_, property_value(definition.flow, important));
		}
		break;
	}
	case _grid_row_:
	case _grid_column_:
	{
		grid_axis_placement placement;
		if (!parse_grid_axis_placement(value, placement)) break;
		const auto& longhands = css_longhands(name);
		add_parsed_property(longhands[0], property_value(placement.start, important));
		add_parsed_property(longhands[1], property_value(placement.end, important));
		break;
	}
	case _grid_area_:
	{
		std::array<grid_line, 4> lines;
		if (!parse_grid_area(value, lines)) break;
		const auto& longhands = css_longhands(name);
		for (std::size_t index = 0; index < lines.size(); ++index)
			add_parsed_property(longhands[index], property_value(std::move(lines[index]), important));
		break;
	}
	case _grid_row_start_:
	case _grid_row_end_:
	case _grid_column_start_:
	case _grid_column_end_:
	{
		grid_line line;
		if (parse_grid_line(value, line))
			add_parsed_property(name, property_value(line, important));
		break;
	}
	case _grid_template_columns_:
	case _grid_template_rows_:
	{
		grid_track_template tracks;
		if (parse_grid_tracks(value, tracks))
			add_parsed_property(name, property_value(std::move(tracks), important));
		break;
	}
	case _grid_template_areas_:
	{
		grid_area_template areas;
		if (parse_grid_template_areas(value, areas))
			add_parsed_property(name, property_value(std::move(areas), important));
		break;
	}
	case _grid_auto_rows_:
	case _grid_auto_columns_:
	{
		grid_track_list tracks;
		if (parse_grid_auto_tracks(value, tracks))
			add_parsed_property(name, property_value(std::move(tracks), important));
		break;
	}
	case _grid_auto_flow_:
	{
		grid_auto_flow flow;
		if (parse_grid_auto_flow(value, flow))
			add_parsed_property(name, property_value(flow, important));
		break;
	}
	case _order_: // <integer>
		if (val.type == NUMBER && val.n.number_type == css_number_integer &&
			static_cast<double>(val.n.number) >= std::numeric_limits<int>::min() &&
			static_cast<double>(val.n.number) <= std::numeric_limits<int>::max())
			add_parsed_property(name, property_value((int)val.n.number, important));
		break;

	//  =============================  COUNTER, CONTENT  =============================

	case _counter_increment_:
	case _counter_reset_:
		if (const auto counters = parse_counters(value))
			add_parsed_property(name, property_value(*counters, important));
		break;

	case _content_:
		if (is_content(value))
			add_parsed_property(name, property_value(get_repr(value, 0, -1, true), important));
		break;

	//  ==================================  OTHER  ==================================

	case _cursor_:
		add_parsed_property(name, property_value(get_repr(value, 0, -1, true), important));
		break;
	default:
		break;
	}
}

// https://developer.mozilla.org/en-US/docs/Web/CSS/flex
// https://drafts.csswg.org/css-flexbox/#flex-property
// flex = none | [ <'flex-grow'> <'flex-shrink'>? || <'flex-basis'> ]
void css_value_parser::parse_flex(const css_token_vector& tokens, bool important)
{
	auto n = tokens.size();
	if (n > 3) return;

	const auto& a = at(tokens, 0);
	const auto& b = at(tokens, 1);
	const auto& c = at(tokens, 2);

	struct flex
	{
		float m_grow = 1; // flex-grow is set to 1 when omitted
		float m_shrink = 1;
		css_length m_basis = 0;

		bool grow(const css_token& tok)
		{
			if (tok.type != NUMBER || tok.n.number < 0 || !std::isfinite(tok.n.number)) return false;
			m_grow = tok.n.number;
			return true;
		}
		bool shrink(const css_token& tok)
		{
			if (tok.type != NUMBER || tok.n.number < 0 || !std::isfinite(tok.n.number)) return false;
			m_shrink = tok.n.number;
			return true;
		}
		// unitless_zero_allowed enforces the rule "A unitless zero that is not already preceded by two flex factors must be interpreted as a flex factor."
		bool basis(const css_token& tok, bool unitless_zero_allowed = false)
		{
			if (!unitless_zero_allowed and tok.type == NUMBER and tok.n.number == 0)
				return false;
			return m_basis.from_token(tok, f_length_percentage | f_positive | f_math, flex_basis_strings);
		}
	};
	flex flex;

	if (n == 1)
	{
		string_id ident = _id(a.ident());
		if (is_one_of(ident, _auto_, _none_))
		{
			css_length _auto = css_length::predef_value(flex_basis_auto);

			switch (ident)
			{
			case _auto_:    flex = {1, 1, _auto}; break; // can be handled by else
			case _none_:    flex = {0, 0, _auto}; break;
			default:;
			}
		}
		else
		{
			bool ok = flex.grow(a) || flex.basis(a);
			if (!ok) return;
		}
	}
	else if (n == 2)
	{
		// <number> <number>
		// <number> <basis>
		// <basis> <number>
		bool ok =
			(flex.grow(a) && (flex.shrink(b) || flex.basis(b))) ||
			(flex.basis(a) && flex.grow(b));

		if (!ok) return;
	}
	else // n == 3
	{
		// <number> <number> <basis>
		// <basis> <number> <number>
		bool ok =
			(flex.grow(a) && flex.shrink(b) && flex.basis(c, true)) ||
			(flex.basis(a) && flex.grow(b) && flex.shrink(c));

		if (!ok) return;
	}

	add_parsed_property(_flex_grow_,	property_value(flex.m_grow, important));
	add_parsed_property(_flex_shrink_,	property_value(flex.m_shrink, important));
	add_parsed_property(_flex_basis_,	property_value(flex.m_basis, important));
}

// flex-flow = <'flex-direction'> || <'flex-wrap'>
void css_value_parser::parse_flex_flow(const css_token_vector& tokens, bool important)
{
	// initial values:  https://developer.mozilla.org/en-US/docs/Web/CSS/flex-flow#formal_definition
	int flex_direction = flex_direction_row;
	int flex_wrap      = flex_wrap_nowrap;

	bool direction_found = false;
	bool wrap_found = false;

	for (const auto& token : tokens)
	{
		if (!direction_found && parse_keyword(token, flex_direction, flex_direction_strings))
			direction_found = true;
		else if (!wrap_found && parse_keyword(token, flex_wrap, flex_wrap_strings))
			wrap_found = true;
		else
			return;
	}

	add_parsed_property(_flex_direction_, property_value(flex_direction, important));
	add_parsed_property(_flex_wrap_,      property_value(flex_wrap,      important));
}

} // namespace litehtml

void litehtml::css_value_parser::parse_gap(string_id name, const css_token_vector& value, bool important) {
	if (value.empty() || value.size() > (name == _gap_ ? 2u : 1u)) {
		return;
	}
	const auto parse = [](const css_token& token, css_length& gap) {
		return gap.from_token(token, f_length_percentage | f_positive | f_math, "normal") &&
			(gap.is_predefined() || std::isfinite(gap.val()));
	};
	css_length row;
	css_length column;
	if (!parse(value.front(), row) || !parse(value.back(), column)) {
		return;
	}
	if (name != _column_gap_) add_parsed_property(_row_gap_, property_value(row, important));
	if (name != _row_gap_) add_parsed_property(_column_gap_, property_value(column, important));
}
