#include "litehtml_css_transform.h"

#include <litehtml/html.h>

#include <cmath>

namespace litehtml {
	namespace {
		using kind = css_transform_function::kind;

		bool parse_length(const css_token& p_token, css_length& r_value, bool p_percentage) {
			if (is_one_of(p_token.type, NUMBER, PERCENTAGE, DIMENSION) && !std::isfinite(p_token.n.number)) {
				return false;
			}
			return r_value.from_token(p_token, (p_percentage ? f_length_percentage : f_length) | f_math);
		}

		bool parse_angle(const css_token& p_token, double& r_angle) {
			if (p_token.type == NUMBER) {
				return p_token.n.number == 0;
			}
			if (p_token.type != DIMENSION || !std::isfinite(p_token.n.number)) {
				return false;
			}
			const auto unit = lowcase(p_token.unit);
			constexpr double pi = 3.14159265358979323846;
			const double angle = p_token.n.number;
			if (unit == "deg") r_angle = std::remainder(angle, 360.0) * pi / 180.0;
			else if (unit == "grad") r_angle = std::remainder(angle, 400.0) * pi / 200.0;
			else if (unit == "rad") r_angle = std::remainder(angle, 2 * pi);
			else if (unit == "turn") r_angle = std::remainder(angle, 1.0) * 2 * pi;
			else return false;
			return true;
		}

		bool parse_number(const css_token& p_token, double& r_number) {
			if (p_token.type != NUMBER || !std::isfinite(p_token.n.number)) {
				return false;
			}
			r_number = p_token.n.number;
			return true;
		}

		enum class origin_axis { either, horizontal, vertical, length, invalid };

		origin_axis parse_origin_component(const css_token& p_token, css_length& r_length) {
			const auto name = p_token.ident();
			if (name == "center") return origin_axis::either;
			if (is_one_of(name, "left", "right")) {
				r_length.set_value(name == "left" ? 0 : 100, css_units_percentage);
				return origin_axis::horizontal;
			}
			if (is_one_of(name, "top", "bottom")) {
				r_length.set_value(name == "top" ? 0 : 100, css_units_percentage);
				return origin_axis::vertical;
			}
			return parse_length(p_token, r_length, true) ? origin_axis::length : origin_axis::invalid;
		}
	}

	bool parse_css_transform(const css_token_vector& p_tokens, css_transform_list& r_value) {
		if (p_tokens.size() == 1 && p_tokens.front().ident() == "none") {
			return true;
		}
		for (const auto& token : p_tokens) {
			if (token.type != CV_FUNCTION) return false;
			const auto name = lowcase(token.name);
			const auto& args = token.value;
			css_transform_function function{};
			if (name == "matrix") {
				if (args.size() != 11) return false;
				function.type = kind::matrix;
				for (size_t i = 0; i < function.matrix.size(); ++i) {
					if ((i != 0 && args[2 * i - 1].type != COMMA) ||
						!parse_number(args[2 * i], function.matrix[i])) return false;
				}
				r_value.push_back(std::move(function));
				continue;
			}
			const bool two = args.size() == 3 && args[1].ch == ',';
			if (args.size() != 1 && !two) return false;
			if (two && !is_one_of(name, "translate", "scale", "skew")) return false;

			if (is_one_of(name, "translate", "translatex", "translatey")) {
				function.type = kind::translate;
				if (!parse_length(args[0], name == "translatey" ? function.y : function.x, true) ||
					(two && !parse_length(args[2], function.y, true))) return false;
			} else if (name == "rotate") {
				function.type = kind::rotate;
				if (!parse_angle(args[0], function.first)) return false;
			} else if (is_one_of(name, "skew", "skewx", "skewy")) {
				function.type = kind::skew;
				if (!parse_angle(args[0], name == "skewy" ? function.second : function.first) ||
					(two && !parse_angle(args[2], function.second))) return false;
			} else if (is_one_of(name, "scale", "scalex", "scaley")) {
				function.type = kind::scale;
				function.first = function.second = 1;
				if (!parse_number(args[0], name == "scaley" ? function.second : function.first)) return false;
				if (name == "scale") {
					function.second = function.first;
					if (two && !parse_number(args[2], function.second)) return false;
				}
			} else {
				return false;
			}
			r_value.push_back(std::move(function));
		}
		return !r_value.empty();
	}

	bool parse_css_transform_origin(const css_token_vector& p_tokens, css_transform_origin& r_value) {
		if (p_tokens.empty() || p_tokens.size() > 2) return false;
		const auto first = parse_origin_component(p_tokens[0], r_value.x);
		if (first == origin_axis::invalid) return false;
		if (p_tokens.size() == 1) {
			if (first == origin_axis::vertical) std::swap(r_value.x, r_value.y);
			return true;
		}
		const auto second = parse_origin_component(p_tokens[1], r_value.y);
		if (second == origin_axis::invalid) return false;
		if (first == origin_axis::vertical || second == origin_axis::horizontal) {
			if (first == origin_axis::horizontal || second == origin_axis::vertical ||
				first == origin_axis::length || second == origin_axis::length) return false;
			std::swap(r_value.x, r_value.y);
		}
		return true;
	}
}
