#pragma once

#include <litehtml/html.h>
#include <litehtml/style.h>

namespace litehtml {
	const char* css_keywords(string_id name);

	class css_value_parser {
		props_map properties;

	public:
		void add_parsed_property(string_id name, property_value value) {
			properties.insert_or_assign(name, std::move(value));
		}
		void parse_box(string_id name, const css_token_vector& value, bool important);
		void parse_background_and_border(string_id name, const css_token_vector& value, const string& baseurl,
			bool important, document_container* container);
		void parse_text(string_id name, const css_token_vector& value, bool important, document_container* container);
		void parse_layout(string_id name, const css_token_vector& value, bool important);
		props_map take_properties() { return std::move(properties); }

	private:
		void parse_gap(string_id name, const css_token_vector& value, bool important);
		void parse_background(const css_token_vector& tokens, const string& baseurl, bool important, document_container* container);
		static bool parse_bg_layer(const css_token_vector& tokens, document_container* container, background& bg, bool final_layer);
		// parse the value of background-image property, which is comma-separated list of <bg-image>s
		void parse_background_image(const css_token_vector& tokens, const string& baseurl, bool important, document_container* container);

		// parse comma-separated list of keywords
		void parse_keyword_comma_list(string_id name, const css_token_vector& tokens, bool important);
		void parse_background_position(const css_token_vector& tokens, bool important);
		void parse_background_size(const css_token_vector& tokens, bool important);

		void parse_border(const css_token_vector& tokens, bool important, document_container* container);
		void parse_border_side(string_id name, const css_token_vector& tokens, bool important, document_container* container);
		void parse_border_radius(const css_token_vector& tokens, bool important);

		static bool parse_list_style_image(const css_token& tok, string& url);
		void parse_list_style(const css_token_vector& tokens, string baseurl, bool important);

		void parse_font(css_token_vector tokens, bool important);
		void parse_text_decoration(const css_token_vector& tokens, bool important, document_container* container);
		void parse_text_decoration_color(const css_token& token, bool important, document_container* container);
		void parse_text_decoration_line(const css_token_vector& tokens, bool important);

		void parse_text_emphasis(const css_token_vector& tokens, bool important, document_container* container);
		void parse_text_emphasis_color(const css_token& token, bool important, document_container* container);
		void parse_text_emphasis_position(const css_token_vector& tokens, bool important);

		void parse_flex_flow(const css_token_vector& tokens, bool important);
		void parse_flex(const css_token_vector& tokens, bool important);

		void add_length_property(string_id name, css_token val, string keywords, int options, bool important);
		template<class T> void add_four_properties(string_id top_name, T val[4], int n, bool important);
	};

	bool parse_two_lengths(const css_token_vector& tokens, css_length lengths[2], int options);

	template<class T, class... Args>
	int parse_1234_values(const css_token_vector& tokens, T result[4],
		bool (*parse)(const css_token&, T&, Args...), Args... args) {
		if (tokens.size() > 4) {
			return 0;
		}
		for (size_t index = 0; index < tokens.size(); ++index) {
			if (!parse(tokens[index], result[index], args...)) {
				return 0;
			}
		}
		return static_cast<int>(tokens.size());
	}

	int parse_1234_lengths(const css_token_vector& tokens, css_length lengths[4], int options, string keywords = "");

	template<class T>
	void css_value_parser::add_four_properties(string_id top, T values[4], int count, bool important) {
		// Side and corner IDs are contiguous in clockwise order.
		add_parsed_property(top, property_value(values[0], important));
		add_parsed_property(string_id(top + 1), property_value(values[count > 1], important));
		add_parsed_property(string_id(top + 2), property_value(values[count >= 3 ? 2 : 0], important));
		add_parsed_property(string_id(top + 3), property_value(values[count == 4 ? 3 : count > 1 ? 1 : 0], important));
	}

	const vector<string_id>& css_longhands(string_id name);
}
