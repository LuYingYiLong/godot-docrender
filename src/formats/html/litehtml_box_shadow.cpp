#include "litehtml_box_shadow.h"

#include <litehtml/html.h>

#include <cmath>

namespace litehtml {
	bool parse_box_shadows(const css_token_vector& p_tokens, document_container* p_container, std::vector<box_shadow>& r_shadows) {
		if (p_tokens.size() == 1) {
			const auto keyword = lowcase(p_tokens.front().ident());
			if (keyword == "none" || keyword == "initial" || keyword == "unset") {
				return true;
			}
		}

		std::size_t index = 0;
		do {
			box_shadow shadow;
			css_length* lengths[] = { &shadow.x, &shadow.y, &shadow.blur, &shadow.spread };
			std::size_t count = 0;
			bool has_color = false;
			bool lengths_finished = false;
			for (; index < p_tokens.size() && p_tokens[index].type != COMMA; ++index) {
				const auto& token = p_tokens[index];
				css_length length;
				if (length.from_token(token, f_length)) {
					if (lengths_finished || count == 4 || !std::isfinite(length.val()) || (count == 2 && length.val() < 0.0f)) {
						return false;
					}
					*lengths[count++] = length;
					continue;
				}
				lengths_finished |= count != 0;
				if (!shadow.inset && lowcase(token.ident()) == "inset") {
					shadow.inset = true;
				}
				else if (!has_color && parse_color(token, shadow.color, p_container)) {
					has_color = true;
				}
				else {
					return false;
				}
			}
			if (count < 2) {
				return false;
			}
			r_shadows.push_back(shadow);
		} while (index++ < p_tokens.size());
		return true;
	}
}
