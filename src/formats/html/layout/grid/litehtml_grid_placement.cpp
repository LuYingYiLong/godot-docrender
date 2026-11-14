#include "litehtml_grid_placement.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <unordered_map>

namespace litehtml {
	namespace {
		constexpr int first_line = -grid_track_limit;
		constexpr int last_line = grid_track_limit;

		struct line_range {
			int start;
			int end;
		};

		struct area {
			line_range major;
			line_range minor;
		};

		struct axis_position {
			std::optional<line_range> range;
			int span;
		};

		line_range clamp_range(std::int64_t start, std::int64_t end) {
			return { static_cast<int>(std::clamp<std::int64_t>(start, first_line, last_line - 1)),
				static_cast<int>(std::clamp<std::int64_t>(end, first_line + 1, last_line)) };
		}

		std::int64_t find_named_line(const grid_line_indices& names, const std::string& name, std::int64_t count,
			std::int64_t from, int explicit_end, bool forward) {
			const auto found = names.find(name);
			// Only implicit lines on the side we are searching toward match.
			if (forward) {
				if (found != names.end()) {
					const auto& lines = found->second;
					const auto first = std::lower_bound(lines.begin(), lines.end(), from);
					const auto matches = lines.end() - first;
					if (count <= matches) return first[count - 1];
					count -= matches;
				}
				return std::max<std::int64_t>(from, explicit_end + 1) + (count - 1);
			}
			if (found != names.end()) {
				const auto& lines = found->second;
				const auto end = std::upper_bound(lines.begin(), lines.end(), from);
				const auto matches = end - lines.begin();
				if (count <= matches) return *(end - count);
				count -= matches;
			}
			return std::min<std::int64_t>(from, -1) - (count - 1);
		}

		std::optional<std::int64_t> resolve_line(const grid_line& line, int explicit_end, const grid_line_indices& names, bool start) {
			using kind = grid_line::kind;
			if (line.type == kind::automatic || line.type == kind::span) return std::nullopt;
			if (line.type == kind::name) {
				const auto found = names.find(line.name + (start ? "-start" : "-end"));
				if (found != names.end()) return found->second.front();
			}
			const auto value = line.value;
			if (line.name.empty()) return value > 0 ? value - 1 : explicit_end + 1 + value;
			return value > 0 ? find_named_line(names, line.name, value, 0, explicit_end, true) :
				find_named_line(names, line.name, -value, explicit_end, explicit_end, false);
		}

		axis_position resolve_axis(const grid_axis_placement& placement, int explicit_end, const grid_line_indices& names) {
			using kind = grid_line::kind;
			auto start = resolve_line(placement.start, explicit_end, names, true);
			auto end = resolve_line(placement.end, explicit_end, names, false);
			if (start && end) {
				if (*start > *end) std::swap(start, end);
				return { clamp_range(*start, std::max(*end, *start + 1)), 1 };
			}
			const auto& span = placement.start.type == kind::span ? placement.start : placement.end;
			const auto length = span.type == kind::span ? span.value : 1;
			const bool named_span = span.type == kind::span && !span.name.empty();
			if (start) {
				const auto last = named_span ? find_named_line(names, span.name, length, *start + 1, explicit_end, true) :
					*start + length;
				return { clamp_range(*start, last), 1 };
			}
			if (end) {
				const auto first = named_span ? find_named_line(names, span.name, length, *end - 1, explicit_end, false) :
					*end - length;
				return { clamp_range(first, *end), 1 };
			}
			const auto auto_span = named_span ? 1 : std::min<std::int64_t>(length, last_line - first_line);
			return { std::nullopt, static_cast<int>(auto_span) };
		}

		bool overlaps(line_range a, line_range b) {
			return a.start < b.end && b.start < a.end;
		}

		// Each rectangle lives at the first major-axis split it crosses. Queries
		// visit only branches that can intersect it, with no per-cell allocation.
		struct occupied_areas {
			std::vector<area> crossing;
			std::unique_ptr<occupied_areas> before;
			std::unique_ptr<occupied_areas> after;

			void add(const area& box, int low = first_line, int high = last_line) {
				const int mid = low + (high - low) / 2;
				if (high - low <= 1 || (box.major.start < mid && box.major.end > mid)) {
					crossing.push_back(box);
					return;
				}
				const bool left = box.major.end <= mid;
				auto& branch = left ? before : after;
				if (!branch) branch = std::make_unique<occupied_areas>();
				branch->add(box, left ? low : mid, left ? mid : high);
			}

			template <typename Visitor>
			void visit_intersections(const area& box, const Visitor& visit, int low = first_line, int high = last_line) const {
				for (const auto& occupied : crossing)
					if (overlaps(box.major, occupied.major) && overlaps(box.minor, occupied.minor)) visit(occupied);
				const int mid = low + (high - low) / 2;
				if (before && box.major.start < mid) before->visit_intersections(box, visit, low, mid);
				if (after && box.major.end > mid) after->visit_intersections(box, visit, mid, high);
			}

			int next_start(const area& box, line_range area::* axis) const {
				int next = (box.*axis).start;
				visit_intersections(box, [&](const area& occupied) { next = std::max(next, (occupied.*axis).end); });
				return next;
			}
		};

		grid_track_range normalize_range(line_range range, int origin) {
			return { static_cast<std::size_t>(range.start - origin), static_cast<std::size_t>(range.end - origin) };
		}
	}

	grid_placement place_grid_items(const std::vector<grid_item_lines>& items, std::size_t explicit_rows, std::size_t explicit_columns,
		const grid_line_indices& row_names, const grid_line_indices& column_names, grid_auto_flow flow) {
		grid_placement result{ {}, explicit_rows, explicit_columns };
		if (items.empty()) return result;

		// Move along the minor axis before advancing to the next major track.
		const auto explicit_major = flow.by_column ? explicit_columns : explicit_rows;
		const auto explicit_minor = flow.by_column ? explicit_rows : explicit_columns;
		const auto& major_names = flow.by_column ? column_names : row_names;
		const auto& minor_names = flow.by_column ? row_names : column_names;
		const auto record = [&](grid_track_range major, grid_track_range minor) {
			result.items.push_back(flow.by_column ? grid_item_placement{ minor, major } : grid_item_placement{ major, minor });
		};
		const auto set_extents = [&](std::size_t major_extent, std::size_t minor_extent,
			std::size_t major_offset, std::size_t minor_offset) {
			result.row_count = flow.by_column ? minor_extent : major_extent;
			result.column_count = flow.by_column ? major_extent : minor_extent;
			result.row_offset = flow.by_column ? minor_offset : major_offset;
			result.column_offset = flow.by_column ? major_offset : minor_offset;
		};

		struct pending_item {
			axis_position major;
			axis_position minor;
		};
		std::vector<pending_item> resolved;
		std::vector<area> placed(items.size());
		resolved.reserve(items.size());
		int major_start = 0;
		int minor_start = 0;
		int major_end = static_cast<int>(explicit_major);
		int minor_end = static_cast<int>(explicit_minor);
		occupied_areas occupied;
		bool all_single_auto = true;

		const auto commit = [&](std::size_t index, const area& box) {
			placed[index] = box;
			occupied.add(box);
			major_end = std::max(major_end, box.major.end);
			minor_end = std::max(minor_end, box.minor.end);
		};
		for (std::size_t index = 0; index < items.size(); ++index) {
			const auto& item = items[index];
			const auto major = resolve_axis(flow.by_column ? item.columns : item.rows, static_cast<int>(explicit_major), major_names);
			const auto minor = resolve_axis(flow.by_column ? item.rows : item.columns, static_cast<int>(explicit_minor), minor_names);
			if (major.range) major_start = std::min(major_start, major.range->start);
			if (minor.range) minor_start = std::min(minor_start, minor.range->start);
			resolved.push_back({ major, minor });
			all_single_auto &= !major.range && !minor.range && major.span == 1 && minor.span == 1;
			if (major.range && minor.range) commit(index, { *major.range, *minor.range });
		}
		result.items.reserve(items.size());
		if (all_single_auto) {
			const auto minor_count = std::max<std::size_t>(1, explicit_minor);
			const auto major_count = std::max(explicit_major, std::min<std::size_t>(last_line, (items.size() - 1) / minor_count + 1));
			set_extents(major_count, minor_count, 0, 0);
			for (std::size_t index = 0; index < items.size(); ++index) {
				const auto major = std::min(major_count - 1, index / minor_count);
				const auto minor = index % minor_count;
				record({ major, major + 1 }, { minor, minor + 1 });
			}
			return result;
		}

		std::unordered_map<int, int> locked_cursors;
		for (std::size_t index = 0; index < resolved.size(); ++index) {
			const auto& item = resolved[index];
			if (!item.major.range || item.minor.range) continue;
			const auto major = *item.major.range;
			int minor = flow.dense ? minor_start : locked_cursors.try_emplace(major.start, minor_start).first->second;
			for (;;) {
				const auto range = clamp_range(minor, minor + item.minor.span);
				const int next = occupied.next_start({ major, range }, &area::minor);
				if (next == range.start || minor >= last_line) {
					commit(index, { major, range });
					if (!flow.dense) locked_cursors[major.start] = range.end;
					break;
				}
				minor = next;
			}
		}

		for (const auto& item : resolved) {
			const int end = item.minor.range ? item.minor.range->end : std::min(last_line, minor_start + item.minor.span);
			minor_end = std::max(minor_end, end);
		}

		int major = major_start;
		int minor = minor_start;
		for (std::size_t index = 0; index < resolved.size(); ++index) {
			const auto& item = resolved[index];
			if (item.major.range) continue;
			if (flow.dense) {
				major = major_start;
				minor = minor_start;
			}

			if (item.minor.range) {
				if (item.minor.range->start < minor) ++major;
				minor = item.minor.range->start;
				for (;;) {
					const area box{ clamp_range(major, major + item.major.span), *item.minor.range };
					const int next = occupied.next_start(box, &area::major);
					if (next == box.major.start || major >= last_line) {
						commit(index, box);
						break;
					}
					major = next;
				}
				continue;
			}

			const int span = std::min(item.minor.span, minor_end - minor_start);
			for (;;) {
				if (minor > minor_end - span) {
					minor = minor_start;
					++major;
				}
				const area box{ clamp_range(major, major + item.major.span), { minor, minor + span } };
				const int next = occupied.next_start(box, &area::minor);
				if (next == minor || major >= last_line) {
					commit(index, box);
					break;
				}
				minor = next;
			}
		}

		set_extents(major_end - major_start, minor_end - minor_start, -major_start, -minor_start);
		for (const auto& box : placed) {
			record(normalize_range(box.major, major_start), normalize_range(box.minor, minor_start));
		}
		return result;
	}
}
