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
			line_range rows;
			line_range columns;
		};

		struct axis_position {
			std::optional<line_range> range;
			int span;
		};

		line_range clamp_range(std::int64_t start, std::int64_t end) {
			return { static_cast<int>(std::clamp<std::int64_t>(start, first_line, last_line - 1)),
				static_cast<int>(std::clamp<std::int64_t>(end, first_line + 1, last_line)) };
		}

		axis_position resolve_axis(const grid_axis_placement& placement, std::size_t explicit_tracks) {
			using kind = grid_line::kind;
			const auto resolve_line = [explicit_tracks](std::int64_t value) {
				return value > 0 ? value - 1 : static_cast<std::int64_t>(explicit_tracks) + 1 + value;
			};
			const bool has_start = placement.start.type == kind::number;
			const bool has_end = placement.end.type == kind::number;
			if (has_start && has_end) {
				auto start = resolve_line(placement.start.value);
				auto end = resolve_line(placement.end.value);
				if (start > end) std::swap(start, end);
				return { clamp_range(start, std::max(end, start + 1)), 1 };
			}
			const auto span = placement.start.type == kind::span ? placement.start.value :
				placement.end.type == kind::span ? placement.end.value : 1;
			if (has_start) {
				const auto start = resolve_line(placement.start.value);
				return { clamp_range(start, start + span), 1 };
			}
			if (has_end) {
				const auto end = resolve_line(placement.end.value);
				return { clamp_range(end - span, end), 1 };
			}
			return { std::nullopt, static_cast<int>(std::min<std::int64_t>(span, last_line - first_line)) };
		}

		bool overlaps(line_range a, line_range b) {
			return a.start < b.end && b.start < a.end;
		}

		// Each rectangle lives at the first row split it crosses. Queries visit
		// only branches that can intersect their rows, with no per-cell allocation.
		struct occupied_areas {
			std::vector<area> crossing;
			std::unique_ptr<occupied_areas> before;
			std::unique_ptr<occupied_areas> after;

			void add(const area& box, int low = first_line, int high = last_line) {
				const int mid = low + (high - low) / 2;
				if (high - low <= 1 || (box.rows.start < mid && box.rows.end > mid)) {
					crossing.push_back(box);
					return;
				}
				const bool left = box.rows.end <= mid;
				auto& branch = left ? before : after;
				if (!branch) branch = std::make_unique<occupied_areas>();
				branch->add(box, left ? low : mid, left ? mid : high);
			}

			template <typename Visitor>
			void visit_intersections(const area& box, const Visitor& visit, int low = first_line, int high = last_line) const {
				for (const auto& occupied : crossing)
					if (overlaps(box.rows, occupied.rows) && overlaps(box.columns, occupied.columns)) visit(occupied);
				const int mid = low + (high - low) / 2;
				if (before && box.rows.start < mid) before->visit_intersections(box, visit, low, mid);
				if (after && box.rows.end > mid) after->visit_intersections(box, visit, mid, high);
			}

			int next_column(const area& box) const {
				int next = box.columns.start;
				visit_intersections(box, [&](const area& occupied) { next = std::max(next, occupied.columns.end); });
				return next;
			}

			int next_row(const area& box) const {
				int next = box.rows.start;
				visit_intersections(box, [&](const area& occupied) { next = std::max(next, occupied.rows.end); });
				return next;
			}
		};

		grid_track_range normalize_range(line_range range, int origin) {
			return { static_cast<std::size_t>(range.start - origin), static_cast<std::size_t>(range.end - origin) };
		}
	}

	grid_placement place_grid_items(const std::vector<grid_item_lines>& items, std::size_t explicit_rows, std::size_t explicit_columns) {
		grid_placement result{ {}, explicit_rows, explicit_columns };
		if (items.empty()) return result;

		struct pending_item {
			axis_position rows;
			axis_position columns;
		};
		std::vector<pending_item> resolved;
		std::vector<area> placed(items.size());
		resolved.reserve(items.size());
		int row_start = 0;
		int column_start = 0;
		int row_end = static_cast<int>(explicit_rows);
		int column_end = static_cast<int>(explicit_columns);
		occupied_areas occupied;
		bool all_single_auto = true;

		const auto commit = [&](std::size_t index, const area& box) {
			placed[index] = box;
			occupied.add(box);
			row_end = std::max(row_end, box.rows.end);
			column_end = std::max(column_end, box.columns.end);
		};
		for (std::size_t index = 0; index < items.size(); ++index) {
			const auto& item = items[index];
			const auto rows = resolve_axis(item.rows, explicit_rows);
			const auto columns = resolve_axis(item.columns, explicit_columns);
			if (rows.range) row_start = std::min(row_start, rows.range->start);
			if (columns.range) column_start = std::min(column_start, columns.range->start);
			resolved.push_back({ rows, columns });
			all_single_auto &= !rows.range && !columns.range && rows.span == 1 && columns.span == 1;
			if (rows.range && columns.range) commit(index, { *rows.range, *columns.range });
		}
		if (all_single_auto) {
			result.column_count = std::max<std::size_t>(1, explicit_columns);
			result.row_count = std::max(explicit_rows, std::min<std::size_t>(last_line,
				(items.size() - 1) / result.column_count + 1));
			result.items.reserve(items.size());
			for (std::size_t index = 0; index < items.size(); ++index) {
				const auto row = std::min(result.row_count - 1, index / result.column_count);
				const auto column = index % result.column_count;
				result.items.push_back({ { row, row + 1 }, { column, column + 1 } });
			}
			return result;
		}

		std::unordered_map<int, int> row_cursors;
		for (std::size_t index = 0; index < resolved.size(); ++index) {
			const auto& item = resolved[index];
			if (!item.rows.range || item.columns.range) continue;
			const auto rows = *item.rows.range;
			auto& column = row_cursors.try_emplace(rows.start, column_start).first->second;
			for (;;) {
				const auto columns = clamp_range(column, column + item.columns.span);
				const int next = occupied.next_column({ rows, columns });
				if (next == columns.start || column >= last_line) {
					commit(index, { rows, columns });
					column = columns.end;
					break;
				}
				column = next;
			}
		}

		for (const auto& item : resolved) {
			const int end = item.columns.range ? item.columns.range->end : std::min(last_line, column_start + item.columns.span);
			column_end = std::max(column_end, end);
		}

		int row = row_start;
		int column = column_start;
		for (std::size_t index = 0; index < resolved.size(); ++index) {
			const auto& item = resolved[index];
			if (item.rows.range) continue;

			if (item.columns.range) {
				if (item.columns.range->start < column) ++row;
				column = item.columns.range->start;
				for (;;) {
					const area box{ clamp_range(row, row + item.rows.span), *item.columns.range };
					const int next = occupied.next_row(box);
					if (next == box.rows.start || row >= last_line) {
						commit(index, box);
						break;
					}
					row = next;
				}
				continue;
			}

			const int span = std::min(item.columns.span, column_end - column_start);
			for (;;) {
				if (column > column_end - span) {
					column = column_start;
					++row;
				}
				const area box{ clamp_range(row, row + item.rows.span), { column, column + span } };
				const int next = occupied.next_column(box);
				if (next == column || row >= last_line) {
					commit(index, box);
					break;
				}
				column = next;
			}
		}

		result.row_offset = static_cast<std::size_t>(-row_start);
		result.column_offset = static_cast<std::size_t>(-column_start);
		result.row_count = static_cast<std::size_t>(row_end - row_start);
		result.column_count = static_cast<std::size_t>(column_end - column_start);
		result.items.reserve(items.size());
		for (const auto& box : placed) {
			result.items.push_back({ normalize_range(box.rows, row_start), normalize_range(box.columns, column_start) });
		}
		return result;
	}
}
