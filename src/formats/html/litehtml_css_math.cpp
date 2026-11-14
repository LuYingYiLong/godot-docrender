#include "litehtml_css_math.h"

#include <litehtml/document.h>
#include <litehtml/html.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace litehtml {
	namespace {
		enum class MathOp { LITERAL, ADD, SUBTRACT, MULTIPLY, DIVIDE, MIN, MAX, CLAMP };
		enum MathType { NUMBER_TYPE = 0, LENGTH_TYPE = 1, PERCENT_TYPE = 2 };
		constexpr int MAX_DEPTH = 64;
		constexpr int MAX_COMPONENTS = 256;
		constexpr double CSS_LENGTH_LIMIT = 2147483520.0; // Largest float below INT32_MAX.
	}

	struct css_math_node {
		MathOp op;
		int type;
		double number = 0.0;
		css_units unit = css_units_none;
		std::vector<css_math_node_ptr> args;
	};

	namespace {
		css_math_node_ptr literal(double p_number, css_units p_unit = css_units_none) {
			const int type = p_unit == css_units_none ? NUMBER_TYPE : (p_unit == css_units_percentage ? PERCENT_TYPE : LENGTH_TYPE);
			return std::make_shared<css_math_node>(css_math_node{ MathOp::LITERAL, type, p_number, p_unit, {} });
		}

		// A percentage combines with a length only in a length-percentage context.
		int merge_types(int p_a, int p_b, int p_options) {
			if (p_a == p_b) return p_a;
			if (p_a == NUMBER_TYPE || p_b == NUMBER_TYPE || (p_options & f_length_percentage) != f_length_percentage) return -1;
			return p_a | p_b;
		}

		struct MathParser {
			const css_token_vector& tokens;
			int options;
			int depth;
			int& budget;
			size_t cursor = 0;

			css_math_node_ptr sum() { return parse_binary(false); }

			css_math_node_ptr parse_binary(bool p_product) {
				auto left = p_product ? term() : parse_binary(true);
				while (left && cursor < tokens.size()) {
					const auto& token = tokens[cursor];
					const int op = token.ch;
					if (!(p_product ? op == '*' || op == '/' : op == '+' || op == '-')) break;
					if ((!p_product && !token.spaced_operator) || --budget < 0) return {};
					++cursor;
					auto right = p_product ? term() : parse_binary(true);
					if (!right) return {};
					int type = -1;
					MathOp operation;
					if (op == '+' || op == '-') {
						type = merge_types(left->type, right->type, options);
						operation = op == '+' ? MathOp::ADD : MathOp::SUBTRACT;
					}
					else if (op == '*') {
						if (left->type == NUMBER_TYPE || right->type == NUMBER_TYPE) type = left->type | right->type;
						operation = MathOp::MULTIPLY;
					}
					else {
						if (right->type == NUMBER_TYPE) type = left->type;
						operation = MathOp::DIVIDE;
					}
					if (type < 0) return {};
					left = std::make_shared<css_math_node>(css_math_node{ operation, type, 0.0, css_units_none, { left, right } });
				}
				return left;
			}

			css_math_node_ptr term() {
				if (cursor >= tokens.size() || --budget < 0) return {};
				const auto& token = tokens[cursor++];
				if (token.type == CV_FUNCTION || token.type == ROUND_BLOCK) return function(token, options, depth + 1, budget);
				if (token.type == NUMBER) return std::isfinite(token.n.number) ? literal(token.n.number) : nullptr;
				if (token.type == PERCENTAGE && std::isfinite(token.n.number)) return literal(token.n.number, css_units_percentage);
				css_length length;
				if (token.type == DIMENSION && length.from_token(token, f_length) && std::isfinite(length.val())) {
					return literal(length.val(), length.units());
				}
				return {};
			}

			static css_math_node_ptr function(const css_token& p_token, int p_options, int p_depth, int& r_budget) {
				if (p_depth > MAX_DEPTH || p_token.value.empty()) return {};
				MathParser parser{ p_token.value, p_options, p_depth, r_budget };
				const auto name = p_token.type == ROUND_BLOCK ? "calc" : lowcase(p_token.name);
				if (name == "calc") {
					auto result = parser.sum();
					return parser.cursor == parser.tokens.size() ? result : nullptr;
				}
				if (!is_one_of(name, "min", "max", "clamp")) return {};
				const auto operation = name == "min" ? MathOp::MIN : (name == "max" ? MathOp::MAX : MathOp::CLAMP);
				std::vector<css_math_node_ptr> args;
				int type = -1;
				for (;;) {
					if (operation == MathOp::CLAMP && args.size() == 3) return {};
					const bool unbounded = operation == MathOp::CLAMP && args.size() != 1 && parser.cursor < parser.tokens.size() &&
						parser.tokens[parser.cursor].ident() == "none";
					if (unbounded) {
						++parser.cursor;
						args.emplace_back();
					}
					else {
						auto arg = parser.sum();
						if (!arg) return {};
						type = type < 0 ? arg->type : merge_types(type, arg->type, p_options);
						if (type < 0) return {};
						args.push_back(std::move(arg));
					}
					if (parser.cursor == parser.tokens.size()) break;
					if (parser.tokens[parser.cursor++].ch != ',') return {};
				}
				if (operation == MathOp::CLAMP && args.size() != 3) return {};
				return std::make_shared<css_math_node>(css_math_node{ operation, type, 0.0, css_units_none, std::move(args) });
			}
		};

		double evaluate_node(const css_math_node& p_node, double p_basis) {
			const auto at = [&](size_t p_index) { return evaluate_node(*p_node.args[p_index], p_basis); };
			switch (p_node.op) {
				case MathOp::LITERAL: return p_node.unit == css_units_percentage ? p_node.number * p_basis / 100.0 : p_node.number;
				case MathOp::ADD: return at(0) + at(1);
				case MathOp::SUBTRACT: return at(0) - at(1);
				case MathOp::MULTIPLY: return at(0) * at(1);
				case MathOp::DIVIDE: return at(0) / at(1);
				case MathOp::MIN:
				case MathOp::MAX: {
					double result = at(0);
					if (std::isnan(result)) return result;
					for (size_t index = 1; index < p_node.args.size(); ++index) {
						const double next = at(index);
						if (std::isnan(next)) return next;
						result = p_node.op == MathOp::MIN ? std::min(result, next) : std::max(result, next);
					}
					return result;
				}
				case MathOp::CLAMP: {
					const double lower = p_node.args[0] ? at(0) : -std::numeric_limits<double>::infinity();
					const double middle = at(1);
					const double upper = p_node.args[2] ? at(2) : std::numeric_limits<double>::infinity();
					if (std::isnan(lower) || std::isnan(middle) || std::isnan(upper)) return std::numeric_limits<double>::quiet_NaN();
					return std::max(lower, std::min(middle, upper));
				}
			}
			return 0.0;
		}

		css_math_node_ptr compute_node(const css_math_node_ptr& p_node, const document& p_document, const font_metrics& p_metrics) {
			if (p_node->op == MathOp::LITERAL) {
				if (p_node->type != LENGTH_TYPE || p_node->unit == css_units_px) return p_node;
				return literal(p_document.to_pixels(css_length(static_cast<float>(p_node->number), p_node->unit), p_metrics, 0), css_units_px);
			}
			css_math_node computed = *p_node;
			for (auto& arg : computed.args) {
				if (arg) arg = compute_node(arg, p_document, p_metrics);
			}
			if (!(computed.type & PERCENT_TYPE)) {
			return literal(evaluate_node(computed, 0), computed.type == NUMBER_TYPE ? css_units_none : css_units_px);
			}
			return std::make_shared<css_math_node>(std::move(computed));
		}

		string serialize_node(const css_math_node& p_node) {
			if (p_node.op == MathOp::LITERAL) {
				return std::to_string(p_node.number) + (p_node.unit == css_units_none ? "" : index_value(p_node.unit, css_units_strings));
			}
			if (p_node.op <= MathOp::DIVIDE) {
				static constexpr const char* operators[] = { "", " + ", " - ", " * ", " / " };
				return "(" + serialize_node(*p_node.args[0]) + operators[static_cast<int>(p_node.op)] + serialize_node(*p_node.args[1]) + ")";
			}
			string result = p_node.op == MathOp::MIN ? "min(" : (p_node.op == MathOp::MAX ? "max(" : "clamp(");
			for (size_t index = 0; index < p_node.args.size(); ++index) {
				if (index) result += ", ";
				result += p_node.args[index] ? serialize_node(*p_node.args[index]) : "none";
			}
			return result + ')';
		}
	}

	std::shared_ptr<const css_math_expression> parse_css_math(const css_token& p_token, int p_options) {
		if (p_token.type != CV_FUNCTION) return {};
		int budget = MAX_COMPONENTS;
		const auto root = MathParser::function(p_token, p_options, 0, budget);
		if (!root || root->type == NUMBER_TYPE || ((root->type & LENGTH_TYPE) && !(p_options & f_length)) ||
			((root->type & PERCENT_TYPE) && !(p_options & f_percentage))) return {};
		return std::make_shared<css_math_expression>(css_math_expression{ root, bool(root->type & PERCENT_TYPE), bool(p_options & f_positive) });
	}

	std::shared_ptr<const css_math_expression> compute_css_math(const css_math_expression& p_value,
		const document& p_document, const font_metrics& p_metrics) {
		return std::make_shared<css_math_expression>(css_math_expression{
			compute_node(p_value.root, p_document, p_metrics), p_value.has_percentage, p_value.non_negative,
		});
	}

	double css_math_expression::evaluate(double p_basis) const {
		const double value = evaluate_node(*root, p_basis);
		// Keep non-finite values and unrepresentable geometry out of the layout engine.
		return std::isnan(value) ? 0.0 : std::clamp(value, non_negative ? 0.0 : -CSS_LENGTH_LIMIT, CSS_LENGTH_LIMIT);
	}

	string css_math_expression::to_string() const {
		return serialize_node(*root);
	}
}
