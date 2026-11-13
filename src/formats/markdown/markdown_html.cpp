#include "markdown_html.h"

#include "formats/html/html_markup.h"

#include <cstdint>
#include <string_view>

using namespace godot;

namespace docrender {
	namespace {
		bool ascii_alpha(char p_char) {
			return (p_char >= 'a' && p_char <= 'z') || (p_char >= 'A' && p_char <= 'Z');
		}

		bool ascii_digit(char p_char) {
			return p_char >= '0' && p_char <= '9';
		}

		bool space(char p_char) {
			return p_char == ' ' || p_char == '\t' || p_char == '\n' || p_char == '\r' || p_char == '\f';
		}

		int32_t parse_image_dimension(const String& p_value) {
			const String value = p_value.strip_edges();
			if (value.is_empty() || value.length() > 10) {
				return 0;
			}
			for (int64_t i = 0; i < value.length(); ++i) {
				if (value[i] < '0' || value[i] > '9') {
					return 0;
				}
			}
			const int64_t dimension = value.to_int();
			return dimension <= INT32_MAX ? static_cast<int32_t>(dimension) : 0;
		}

		std::string lower(std::string p_text) {
			for (char& c : p_text) {
				if (c >= 'A' && c <= 'Z') {
					c += 'a' - 'A';
				}
			}
			return p_text;
		}

		bool prefix(const std::string& p_text, const char* p_prefix) {
			return p_text.compare(0, std::char_traits<char>::length(p_prefix), p_prefix) == 0;
		}

		bool starts_block_tag(const std::string& p_line, std::string_view p_name, bool p_closing, bool p_raw) {
			const size_t start = p_closing ? 2 : 1;
			const size_t end = start + p_name.size();
			if (p_line.size() < end || p_line.compare(start, p_name.size(), p_name) != 0) {
				return false;
			}
			return p_line.size() == end || space(p_line[end]) || p_line[end] == '>' ||
				(!p_raw && p_line.compare(end, 2, "/>") == 0);
		}

		struct HTMLBlockEnd {
			enum Kind { BLANK_LINE, RAW_ELEMENT, SEQUENCE } kind;
			const char* sequence = nullptr;
		};

		std::optional<HTMLBlockEnd> html_block_end(const std::string& p_line, bool p_in_paragraph) {
			if (prefix(p_line, "<!--")) {
				return HTMLBlockEnd{ HTMLBlockEnd::SEQUENCE, "-->" };
			}
			if (prefix(p_line, "<?")) {
				return HTMLBlockEnd{ HTMLBlockEnd::SEQUENCE, "?>" };
			}
			if (prefix(p_line, "<![CDATA[")) {
				return HTMLBlockEnd{ HTMLBlockEnd::SEQUENCE, "]]>" };
			}
			if (p_line.size() > 2 && prefix(p_line, "<!") && ascii_alpha(p_line[2])) {
				return HTMLBlockEnd{ HTMLBlockEnd::SEQUENCE, ">" };
			}
			const std::string line = lower(p_line);
			for (const char* tag : { "script", "pre", "style", "textarea" }) {
				if (starts_block_tag(line, tag, false, true)) {
					return HTMLBlockEnd{ HTMLBlockEnd::RAW_ELEMENT };
				}
			}
			const bool closing = prefix(line, "</");
			constexpr std::string_view tags[] = {
				"address", "article", "aside", "base", "basefont", "blockquote", "body", "caption", "center", "col",
				"colgroup", "dd", "details", "dialog", "dir", "div", "dl", "dt", "fieldset", "figcaption", "figure",
				"footer", "form", "frame", "frameset", "h1", "h2", "h3", "h4", "h5", "h6", "head", "header", "hr",
				"html", "iframe", "legend", "li", "link", "main", "menu", "menuitem", "nav", "noframes", "ol",
				"optgroup", "option", "p", "param", "search", "section", "summary", "table", "tbody", "td", "tfoot",
				"th", "thead", "title", "tr", "track", "ul",
			};
			for (std::string_view tag : tags) {
				if (starts_block_tag(line, tag, closing, false)) {
					return HTMLBlockEnd{ HTMLBlockEnd::BLANK_LINE };
				}
			}
			MarkdownHTMLTag tag;
			if (!p_in_paragraph && scan_markdown_html_tag(p_line, 0, tag)) {
				const bool raw_open = !tag.closing && (tag.name == "pre" || tag.name == "script" ||
					tag.name == "style" || tag.name == "textarea");
				if (!raw_open && p_line.find_first_not_of(" \t\r", tag.end) == std::string::npos) {
					return HTMLBlockEnd{ HTMLBlockEnd::BLANK_LINE };
				}
			}
			return std::nullopt;
		}
	}

	bool scan_markdown_html_tag(const std::string& p_text, size_t p_start, MarkdownHTMLTag& r_tag) {
		const size_t length = p_text.size();
		if (p_start >= length || p_text[p_start] != '<') {
			return false;
		}
		MarkdownHTMLTag tag;
		size_t pos = p_start + 1;
		if (pos < length && p_text[pos] == '/') {
			tag.closing = true;
			++pos;
		}
		if (pos >= length || !ascii_alpha(p_text[pos])) {
			return false;
		}
		const size_t name_start = pos++;
		while (pos < length && (ascii_alpha(p_text[pos]) || ascii_digit(p_text[pos]) || p_text[pos] == '-')) {
			++pos;
		}
		tag.name = lower(p_text.substr(name_start, pos - name_start));
		bool href_seen = false;
		bool style_seen = false;
		bool image_src_seen = false;
		while (pos < length) {
			const size_t before_space = pos;
			while (pos < length && space(p_text[pos])) {
				++pos;
			}
			if (pos < length && p_text[pos] == '>') {
				tag.end = pos + 1;
				r_tag = std::move(tag);
				return true;
			}
			if (!tag.closing && p_text.compare(pos, 2, "/>") == 0) {
				tag.self_closing = true;
				tag.end = pos + 2;
				r_tag = std::move(tag);
				return true;
			}
			if (tag.closing || pos == before_space || pos == length ||
				!(ascii_alpha(p_text[pos]) || p_text[pos] == '_' || p_text[pos] == ':')) {
				return false;
			}
			const size_t attribute_start = pos++;
			while (pos < length && (ascii_alpha(p_text[pos]) || ascii_digit(p_text[pos]) ||
				p_text[pos] == '_' || p_text[pos] == ':' || p_text[pos] == '.' || p_text[pos] == '-')) {
				++pos;
			}
			const size_t attribute_end = pos;
			const std::string attribute = lower(p_text.substr(attribute_start, pos - attribute_start));
			while (pos < length && space(p_text[pos])) {
				++pos;
			}
			String value;
			if (pos < length && p_text[pos] == '=') {
				++pos;
				while (pos < length && space(p_text[pos])) {
					++pos;
				}
				if (pos >= length) {
					return false;
				}
				if (p_text[pos] == '\'' || p_text[pos] == '"') {
					const char quote = p_text[pos++];
					const size_t end = p_text.find(quote, pos);
					if (end == std::string::npos) {
						return false;
					}
					value = String::utf8(p_text.data() + pos, end - pos);
					pos = end + 1;
				}
				else {
					const size_t start = pos;
					while (pos < length && !space(p_text[pos]) && p_text[pos] != '>') {
						if (std::string_view("\"'=<>\x60").find(p_text[pos]) != std::string_view::npos) {
							return false;
						}
						++pos;
					}
					if (pos == start) {
						return false;
					}
					value = String::utf8(p_text.data() + start, pos - start);
				}
			}
			else {
				pos = attribute_end;
			}
			if (attribute == "href" && !href_seen) {
				tag.href = value;
				href_seen = true;
			}
			else if (attribute == "style" && !style_seen) {
				tag.style = value;
				style_seen = true;
			}
			else if (tag.name == "img") {
				if (attribute == "src" && !image_src_seen) {
					tag.image.src = value;
					image_src_seen = true;
				}
				else if (attribute == "alt") {
					tag.image.alt = value;
				}
				else if (attribute == "title") {
					tag.image.title = value;
				}
				else if (attribute == "width") {
					tag.image.size.x = parse_image_dimension(decode_html_entities(value, true));
				}
				else if (attribute == "height") {
					tag.image.size.y = parse_image_dimension(decode_html_entities(value, true));
				}
			}
		}
		return false;
	}

	std::optional<MarkdownHTMLBlockRange> scan_markdown_html_block(const std::vector<String>& p_lines,
		int32_t p_start, int32_t p_limit, bool p_in_paragraph) {
		const String& first = p_lines[p_start];
		int32_t indent = 0;
		while (indent < first.length() && first[indent] == ' ') {
			++indent;
		}
		if (indent > 3 || indent >= first.length() || first[indent] != '<') {
			return std::nullopt;
		}
		const auto end = html_block_end(first.substr(indent).utf8().get_data(), p_in_paragraph);
		if (!end) {
			return std::nullopt;
		}
		for (int32_t i = p_start; i < p_limit; ++i) {
			if (end->kind == HTMLBlockEnd::BLANK_LINE) {
				if (p_lines[i].strip_edges().is_empty()) {
					return MarkdownHTMLBlockRange{ i, false };
				}
			}
			else if (end->kind == HTMLBlockEnd::SEQUENCE && p_lines[i].contains(end->sequence)) {
				return MarkdownHTMLBlockRange{ i + 1, false };
			}
			else if (end->kind == HTMLBlockEnd::RAW_ELEMENT) {
				const std::string line = lower(p_lines[i].utf8().get_data());
				for (const char* closing : { "</pre>", "</script>", "</style>", "</textarea>" }) {
					if (line.find(closing) != std::string::npos) {
						return MarkdownHTMLBlockRange{ i + 1, false };
					}
				}
			}
		}
		return MarkdownHTMLBlockRange{ p_limit, true };
	}

	size_t find_markdown_html_close(const std::string& p_text, const MarkdownHTMLTag& p_opening, size_t& r_end) {
		int32_t depth = 1;
		for (size_t i = p_opening.end; i < p_text.size(); ++i) {
			if (p_opening.name != "code" && p_text[i] == '\\' && i + 1 < p_text.size()) {
				++i;
				continue;
			}
			if (p_opening.name != "code" && p_text[i] == '`') {
				const size_t start = i;
				while (i + 1 < p_text.size() && p_text[i + 1] == '`') {
					++i;
				}
				const size_t count = i - start + 1;
				for (size_t next = p_text.find('`', i + 1); next != std::string::npos; next = p_text.find('`', next)) {
					const size_t end_start = next++;
					while (next < p_text.size() && p_text[next] == '`') {
						++next;
					}
					if (next - end_start == count) {
						i = next - 1;
						break;
					}
				}
				continue;
			}
			MarkdownHTMLTag tag;
			if (p_text[i] != '<' || !scan_markdown_html_tag(p_text, i, tag)) {
				continue;
			}
			if (tag.name == p_opening.name && !tag.self_closing) {
				depth += tag.closing ? -1 : 1;
				if (depth == 0) {
					r_end = tag.end;
					return i;
				}
			}
			i = tag.end - 1;
		}
		return std::string::npos;
	}
}
