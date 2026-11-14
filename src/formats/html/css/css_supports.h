#pragma once

#include <litehtml/css_tokenizer.h>

#include <optional>

namespace litehtml {
	// A disengaged result denotes an invalid condition. False denotes a valid condition that fails.
	std::optional<bool> evaluate_css_supports(const css_token_vector& p_tokens, document_mode p_mode,
		bool p_allow_bare_declaration = false);
}
