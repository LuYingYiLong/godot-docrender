#include "litehtml_css_variables.h"

#include <litehtml/html_tag.h>
#include <litehtml/html.h>

#include <algorithm>

namespace litehtml {
	namespace {
		constexpr int MAX_DEPTH = 128;
		constexpr size_t MAX_EXPANDED_TOKENS = 16384;
		constexpr size_t MAX_EXPANDED_BYTES = 1024 * 1024;

		bool is_var(const css_token& p_token) {
			return p_token.type == CV_FUNCTION && lowcase(p_token.name) == "var";
		}

		string variable_name(const css_token& p_token) {
			if (p_token.value.empty()) return {};
			const auto name = p_token.value.front().ident();
			return name.size() > 2 && name.compare(0, 2, "--") == 0 ? name : string();
		}

		bool valid_syntax(const css_token_vector& p_tokens, int p_depth, bool& r_has_variable) {
			if (p_depth >= MAX_DEPTH) return false;
			for (const auto& token : p_tokens) {
				if (!token.is_component_value()) continue;
				if (is_var(token)) {
					r_has_variable = true;
					if (variable_name(token).empty() || (token.value.size() > 1 && token.value[1].ch != ',')) return false;
				}
				if (!valid_syntax(token.value, p_depth + 1, r_has_variable)) return false;
			}
			return true;
		}

		bool append_tokens(const css_token_vector& p_tokens, css_token_vector& r_output, size_t& r_budget) {
			if (p_tokens.size() > r_budget || p_tokens.size() > MAX_EXPANDED_TOKENS) return false;
			for (const auto& token : p_tokens) {
				const auto cost = 1 + token.repr.size() + token.str.size();
				if (cost > r_budget) return false;
				r_budget -= cost;
				if (token.is_component_value()) {
					css_token component(token.type, token.str);
					if (!append_tokens(token.value, component.value, r_budget)) return false;
					r_output.push_back(component);
				}
				else {
					r_output.push_back(token);
				}
			}
			return true;
		}
	}

	css_variable_syntax inspect_css_variables(const css_token_vector& p_tokens) {
		bool has_variable = false;
		if (!valid_syntax(p_tokens, 0, has_variable)) return css_variable_syntax::invalid;
		return has_variable ? css_variable_syntax::valid : css_variable_syntax::none;
	}

	css_variable_resolver::css_variable_resolver(const html_tag* p_parent, const css_custom_properties& p_declarations) :
		parent(p_parent), declarations(p_declarations) {
		invalidate_cycles();
		for (const auto& declaration : declarations) resolve(declaration.first, 0);
	}

	void css_variable_resolver::invalidate_cycles() {
		struct Node {
			std::vector<string_id> dependencies;
			int index = -1;
			int low = -1;
			bool on_stack = false;
			bool self_reference = false;
		};
		std::map<string_id, Node> graph;
		for (const auto& [name, tokens] : declarations) {
			if (!tokens) continue;
			auto& node = graph[name];
			const auto collect = [&](const auto& p_self, const css_token_vector& p_tokens, int p_depth) -> void {
				if (p_depth >= MAX_DEPTH) {
					computed[name] = std::nullopt;
					return;
				}
				for (const auto& token : p_tokens) {
					if (!token.is_component_value()) continue;
					if (is_var(token)) {
						const auto spelling = variable_name(token);
						if (!spelling.empty()) {
							const auto target = _id(spelling);
							const auto declaration = declarations.find(target);
							if (declaration != declarations.end() && declaration->second) node.dependencies.push_back(target);
							node.self_reference |= name == target;
						}
					}
					// Fallback arguments also contribute edges to the CSS dependency graph.
					p_self(p_self, token.value, p_depth + 1);
				}
			};
			collect(collect, *tokens, 0);
		}

		// Tarjan's algorithm, with an explicit stack so long dependency chains stay off the C++ stack.
		struct Frame { string_id name; size_t next = 0; };
		std::vector<Frame> frames;
		std::vector<string_id> component;
		int index = 0;
		const auto enter = [&](string_id p_name) {
			auto& node = graph.at(p_name);
			node.index = node.low = index++;
			node.on_stack = true;
			component.push_back(p_name);
			frames.push_back({ p_name });
		};
		for (const auto& [name, start] : graph) {
			if (start.index >= 0) continue;
			enter(name);
			while (!frames.empty()) {
				auto& frame = frames.back();
				auto& current = graph.at(frame.name);
				if (frame.next < current.dependencies.size()) {
					const auto child = current.dependencies[frame.next++];
					const auto& target = graph.at(child);
					if (target.index < 0) enter(child);
					else if (target.on_stack) current.low = std::min(current.low, target.index);
					continue;
				}
				if (current.low == current.index) {
					const auto root = frame.name;
					const bool cycle = current.self_reference || component.back() != root;
					string_id member;
					do {
						member = component.back();
						component.pop_back();
						graph.at(member).on_stack = false;
						if (cycle) computed[member] = std::nullopt;
					} while (member != root);
				}
				frames.pop_back();
				if (!frames.empty()) {
					auto& ancestor = graph.at(frames.back().name);
					ancestor.low = std::min(ancestor.low, current.low);
				}
			}
		}
	}

	const std::optional<css_token_vector>& css_variable_resolver::resolve(string_id p_name, int p_depth) {
		const auto [entry, inserted] = computed.try_emplace(p_name);
		if (!inserted || p_depth >= MAX_DEPTH) return entry->second;
		const auto declaration = declarations.find(p_name);
		if (declaration == declarations.end()) {
			css_token_vector inherited;
			if (parent && parent->get_custom_property(p_name, inherited)) entry->second = std::move(inherited);
		}
		else if (declaration->second) {
			css_token_vector output;
			size_t budget = MAX_EXPANDED_BYTES;
			if (substitute(*declaration->second, 0, output, budget, p_depth + 1)) entry->second = std::move(output);
		}
		return entry->second;
	}

	bool css_variable_resolver::substitute(const css_token_vector& p_tokens, size_t p_start, css_token_vector& r_output,
		size_t& r_budget, int p_depth) {
		if (p_depth >= MAX_DEPTH) return false;
		for (size_t index = p_start; index < p_tokens.size(); ++index) {
			const auto& token = p_tokens[index];
			const auto cost = 1 + token.repr.size() + token.str.size();
			if (cost > r_budget || r_output.size() >= MAX_EXPANDED_TOKENS) return false;
			r_budget -= cost;
			if (is_var(token)) {
				const auto name = variable_name(token);
				const auto& args = token.value;
				if (name.empty() || (args.size() > 1 && args[1].ch != ',')) return false;
				const auto& resolved = resolve(_id(name), p_depth + 1);
				if (resolved) {
					if (resolved->size() > MAX_EXPANDED_TOKENS - r_output.size() || !append_tokens(*resolved, r_output, r_budget)) return false;
				}
				else if (args.size() < 2 || !substitute(args, 2, r_output, r_budget, p_depth + 1)) {
					return false;
				}
			}
			else if (token.is_component_value()) {
				css_token component(token.type, token.str);
				if (!substitute(token.value, 0, component.value, r_budget, p_depth + 1)) return false;
				r_output.push_back(component);
			}
			else {
				r_output.push_back(token);
			}
		}
		return true;
	}

	std::optional<css_token_vector> css_variable_resolver::substitute(const css_token_vector& p_tokens) {
		css_token_vector output;
		size_t budget = MAX_EXPANDED_BYTES;
		if (!substitute(p_tokens, 0, output, budget, 0)) return std::nullopt;
		return output;
	}

	css_computed_variables css_variable_resolver::take_computed() {
		// Inherited cache entries can be looked up through the parent if another child needs them.
		for (auto it = computed.begin(); it != computed.end();) {
			if (!declarations.count(it->first)) it = computed.erase(it);
			else ++it;
		}
		return std::move(computed);
	}
}
