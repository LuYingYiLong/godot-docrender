#pragma once

#include <litehtml/css_tokenizer.h>
#include <litehtml/string_id.h>

#include <map>
#include <optional>

namespace litehtml {
	class html_tag;
	using css_custom_properties = std::map<string_id, const css_token_vector*>;
	using css_computed_variables = std::map<string_id, std::optional<css_token_vector>>;
	enum class css_variable_syntax { none, valid, invalid };
	css_variable_syntax inspect_css_variables(const css_token_vector& p_tokens);

	class css_variable_resolver {
		const html_tag* parent;
		const css_custom_properties& declarations;
		css_computed_variables computed;

		void invalidate_cycles();
		const std::optional<css_token_vector>& resolve(string_id p_name, int p_depth);
		bool substitute(const css_token_vector& p_tokens, size_t p_start, css_token_vector& r_output,
			size_t& r_budget, int p_depth);

	public:
		css_variable_resolver(const html_tag* p_parent, const css_custom_properties& p_declarations);
		std::optional<css_token_vector> substitute(const css_token_vector& p_tokens);
		css_computed_variables take_computed();
	};
}
