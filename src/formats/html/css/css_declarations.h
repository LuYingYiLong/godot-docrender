#pragma once

#include <litehtml/style.h>

namespace litehtml {
	enum class css_declaration_status { unknown, invalid, deferred, parsed };

	struct css_parsed_declaration {
		css_declaration_status status;
		props_map properties;
	};

	css_parsed_declaration parse_css_declaration(string_id name, const css_token_vector& tokens,
		const string& baseurl, bool important, document_container* container, const css_source& source);
	bool supports_css_declaration(const css_token_vector& tokens);
	void resolve_css_declarations(const props_map& specified, const props_map& defaults, const html_tag& element,
		css_computed_variables& computed_variables, props_map& computed, std::vector<css_diagnostic>& diagnostics);
	std::optional<css_diagnostic> check_css_declaration(css_declaration_status status, string_id name,
		const css_token_vector& tokens, const css_source& source);
}
