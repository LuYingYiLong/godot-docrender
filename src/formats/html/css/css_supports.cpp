#include "css_supports.h"

#include "css_declarations.h"

#include <litehtml/css_parser.h>
#include <litehtml/css_selector.h>
#include <litehtml/html.h>

namespace litehtml {
	namespace {
		constexpr int MAX_CONDITION_DEPTH = 64;

		// Empty blocks and functions are valid in <general-enclosed>.
		bool valid_enclosed_tokens(const css_token_vector& p_tokens, int p_depth) {
			if (p_depth >= MAX_CONDITION_DEPTH) return false;
			for (const auto& token : p_tokens) {
				if (is_one_of(token.type, BAD_STRING, BAD_URL, ')', ']', '}')) return false;
				if (token.is_component_value() && !valid_enclosed_tokens(token.value, p_depth + 1)) return false;
			}
			return true;
		}

		bool begins_with_declaration(const css_token_vector& p_tokens) {
			int index = 0;
			skip_whitespace(p_tokens, index);
			if (index == static_cast<int>(p_tokens.size()) || p_tokens[index].type != IDENT) return false;
			++index;
			skip_whitespace(p_tokens, index);
			return index < static_cast<int>(p_tokens.size()) && p_tokens[index].type == COLON;
		}

		std::optional<bool> evaluate_condition(const css_token_vector& p_tokens, document_mode p_mode, int p_depth);

		std::optional<bool> evaluate_test(const css_token& p_test, document_mode p_mode, int p_depth) {
			if (p_test.type == CV_FUNCTION) {
				if (lowcase(p_test.name) == "selector") return supports_css_selector(p_test.value, p_mode);
				return false;
			}
			if (p_test.type != ROUND_BLOCK) return {};
			if (begins_with_declaration(p_test.value)) return supports_css_declaration(p_test.value);
			if (const auto nested = evaluate_condition(p_test.value, p_mode, p_depth + 1)) return nested;
			return false;
		}

		std::optional<bool> evaluate_condition(const css_token_vector& p_tokens, document_mode p_mode, int p_depth) {
			if (p_depth >= MAX_CONDITION_DEPTH) return {};
			const int size = static_cast<int>(p_tokens.size());
			int index = 0;
			skip_whitespace(p_tokens, index);
			if (index == size) return {};

			if (p_tokens[index].ident() == "not") {
				skip_whitespace(p_tokens, ++index);
				if (index == size) return {};
				const auto value = evaluate_test(p_tokens[index++], p_mode, p_depth);
				skip_whitespace(p_tokens, index);
				return value && index == size ? std::optional<bool>(!*value) : std::nullopt;
			}

			auto value = evaluate_test(p_tokens[index++], p_mode, p_depth);
			if (!value) return {};
			string operation;
			while (index < size) {
				skip_whitespace(p_tokens, index);
				if (index == size) break;
				const auto keyword = p_tokens[index++].ident();
				if (!is_one_of(keyword, "and", "or") ||
					(!operation.empty() && operation != keyword)) return {};
				operation = keyword;
				skip_whitespace(p_tokens, index);
				if (index == size) return {};
				const auto operand = evaluate_test(p_tokens[index++], p_mode, p_depth);
				if (!operand) return {};
				value = operation == "and" ? *value && *operand : *value || *operand;
			}
			return value;
		}
	}

	std::optional<bool> evaluate_css_supports(const css_token_vector& p_tokens, document_mode p_mode,
		bool p_allow_bare_declaration) {
		if (!valid_enclosed_tokens(p_tokens, 0)) return {};
		if (p_allow_bare_declaration && begins_with_declaration(p_tokens)) return supports_css_declaration(p_tokens);
		return evaluate_condition(p_tokens, p_mode, 0);
	}
}
