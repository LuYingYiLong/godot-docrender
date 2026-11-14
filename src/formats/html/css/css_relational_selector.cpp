#include "css_relational_selector.h"

#include <litehtml/element.h>
#include <litehtml/html.h>

#include <algorithm>
#include <unordered_set>

namespace litehtml {
	namespace {
		bool is_dom_element(const element& p_element) {
			return !is_one_of(p_element.tag(), empty_id, __tag_before_, __tag_after_);
		}

		// The parser stores a complex selector from right to left. Each step here
		// describes how to walk from the anchor or the previous matched element.
		struct RelativeStep {
			const css_element_selector* compound;
			css_combinator combinator;
		};

		using VisitedSteps = std::vector<std::unordered_set<const element*>>;

		struct PendingStep {
			const element* from;
			size_t index;
		};

		bool match_steps(const element& p_anchor, const std::vector<RelativeStep>& p_steps) {
			VisitedSteps visited(p_steps.size());
			std::vector<PendingStep> pending{ { &p_anchor, 0 } };
			while (!pending.empty()) {
				const auto [from, index] = pending.back();
				pending.pop_back();
				if (index == p_steps.size()) return true;
				// Multiple matches along an earlier axis can lead to the same remaining search.
				if (!visited[index].insert(from).second) continue;
				const auto& [compound, combinator] = p_steps[index];
				const auto enqueue_match = [&](element& p_candidate) {
					if (p_candidate.select(*compound) != select_no_match) {
						pending.push_back({ &p_candidate, index + 1 });
					}
				};
				const bool descendants = combinator == combinator_descendant;
				if (descendants || combinator == combinator_child) {
					for (const auto& child : from->children()) {
						if (!is_dom_element(*child)) continue;
						if (descendants) pending.push_back({ child.get(), index });
						enqueue_match(*child);
					}
					continue;
				}
				if (const auto parent = from->parent()) {
					bool after_anchor = false;
					for (const auto& sibling : parent->children()) {
						if (sibling.get() == from) {
							after_anchor = true;
						} else if (after_anchor && is_dom_element(*sibling)) {
							enqueue_match(*sibling);
							if (combinator == combinator_adjacent_sibling) break;
						}
					}
				}
			}
			return false;
		}
	}

	bool matches_relative_selector(const element& p_anchor, const css_selector::vector& p_selectors) {
		for (const auto& selector : p_selectors) {
			std::vector<RelativeStep> steps;
			for (const auto* part = selector.get(); part; part = part->m_left.get()) {
				steps.push_back({ &part->m_right, part->m_left ? part->m_combinator : part->m_anchor_combinator });
			}
			std::reverse(steps.begin(), steps.end());
			if (match_steps(p_anchor, steps)) return true;
		}
		return false;
	}
}
