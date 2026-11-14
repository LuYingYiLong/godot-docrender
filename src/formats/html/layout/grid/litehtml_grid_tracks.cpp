#include "litehtml_grid_tracks.h"

#include <litehtml/html.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace litehtml {
	namespace {
		bool read_integer(const css_token& token, std::int64_t& integer) {
			if (token.type != NUMBER || token.n.number_type != css_number_integer) return false;
			// Keep the sign and exact digits independently of the tokenizer's float.
			const auto& repr = token.repr;
			if (repr.empty()) return false;
			const bool negative = repr.front() == '-';
			const std::size_t first = negative || repr.front() == '+' ? 1 : 0;
			constexpr auto bound = std::numeric_limits<std::int64_t>::max() / 4;
			std::int64_t value = 0;
			for (std::size_t i = first; i < repr.size(); ++i) {
				const int digit = repr[i] - '0';
				if (digit < 0 || digit > 9) return false;
				value = value > (bound - digit) / 10 ? bound : value * 10 + digit;
			}
			integer = negative ? -value : value;
			return value != 0;
		}

		bool parse_line(const css_token* first, const css_token* end, grid_line& line) {
			const auto count = end - first;
			if (count == 1 && first[0].ident() == "auto") {
				line = {};
				return true;
			}
			std::int64_t value;
			if (count == 1 && read_integer(first[0], value)) {
				line = { grid_line::kind::number, value };
				return true;
			}
			if (count == 2 && ((first[0].ident() == "span" && read_integer(first[1], value)) ||
				(first[1].ident() == "span" && read_integer(first[0], value))) && value > 0) {
				line = { grid_line::kind::span, value };
				return true;
			}
			return false;
		}

		bool parse_breadth(const css_token& token, bool allow_fraction, grid_breadth& breadth) {
			if (token.ident() == "auto") return true;
			if (token.type == DIMENSION && lowcase(token.unit) == "fr") {
				if (!allow_fraction || !std::isfinite(token.n.number) || token.n.number < 0) return false;
				breadth.kind = grid_breadth_kind::fraction;
				breadth.fraction = token.n.number;
				return true;
			}
			if (is_one_of(token.type, NUMBER, DIMENSION, PERCENTAGE) && !std::isfinite(token.n.number)) return false;
			if (!breadth.length.from_token(token, f_length_percentage | f_positive | f_math)) return false;
			breadth.kind = grid_breadth_kind::length;
			return true;
		}

		bool uses_percentage(const grid_breadth& breadth) {
			return breadth.kind == grid_breadth_kind::length && breadth.length.units() == css_units_percentage;
		}

		grid_breadth_kind used_kind(const grid_breadth& breadth, bool definite) {
			return !definite && uses_percentage(breadth) ? grid_breadth_kind::automatic : breadth.kind;
		}

		bool parse_track(const css_token& token, grid_track& track) {
			if (token.type == CV_FUNCTION && lowcase(token.name) == "minmax") {
				const auto& args = token.value;
				return args.size() == 3 && args[1].ch == ',' &&
					parse_breadth(args[0], false, track.minimum) && parse_breadth(args[2], true, track.maximum);
			}
			if (!parse_breadth(token, true, track.maximum)) return false;
			if (track.maximum.kind != grid_breadth_kind::fraction) track.minimum = track.maximum;
			return true;
		}

		bool has_fixed_breadth(const grid_track& track) {
			return track.minimum.kind == grid_breadth_kind::length || track.maximum.kind == grid_breadth_kind::length;
		}

		std::size_t auto_repeat_count(const grid_track_template& axis, const css_length& gap,
			std::optional<pixel_t> definite, std::optional<pixel_t> minimum, std::optional<pixel_t> maximum) {
			const auto upper = definite ? definite : maximum;
			const auto bound = upper ? upper : minimum;
			if (!bound) return 1;

			const auto basis = std::max(*bound, minimum.value_or(0));
			const auto track_breadth = [basis](const grid_track& track) {
				const auto resolve = [basis](const grid_breadth& breadth) {
					return breadth.kind == grid_breadth_kind::length ? static_cast<double>(breadth.length.calc_percent(basis)) : 0.0;
				};
				return std::max({ 0.0, resolve(track.minimum), resolve(track.maximum) });
			};
			const double gap_size = std::max<pixel_t>(0, gap.calc_percent(basis));
			// Charge one gutter per track and then remove the trailing gutter.
			double fixed_extent = -gap_size;
			for (const auto& track : axis.tracks) fixed_extent += track_breadth(track) + gap_size;
			const auto& pattern = axis.repeat->pattern;
			double pattern_extent = 0;
			for (const auto& track : pattern) pattern_extent += std::max(1.0, track_breadth(track)) + gap_size;
			if (!std::isfinite(basis) || !std::isfinite(fixed_extent) || !std::isfinite(pattern_extent)) return 1;
			const double remaining = std::max(0.0, basis - fixed_extent);
			const double repetitions = remaining / pattern_extent;
			// Track and gap lengths can round before this division (e.g. two 45% tracks and a 10% gap).
			const double tolerance = std::numeric_limits<pixel_t>::epsilon() * std::max(1.0, repetitions);
			const double count = upper ? std::floor(repetitions + tolerance) : std::ceil(repetitions - tolerance);
			const auto limit = (grid_track_limit - axis.tracks.size()) / pattern.size();
			return static_cast<std::size_t>(std::clamp(count, 1.0, static_cast<double>(limit)));
		}

		std::size_t span_of(grid_track_range range) {
			return range.end - range.start;
		}

		struct sized_track {
			double base = 0;
			double limit = 0;
			double factor = 0;
			bool automatic_maximum = false;
			bool automatic_minimum = false;
			bool flexible = false;
			bool unbounded = false;
		};

		void grow_to_limits(std::vector<sized_track>& tracks, double& free_space) {
			std::vector<sized_track*> growable;
			for (auto& track : tracks)
				if (track.limit > track.base) growable.push_back(&track);
			std::sort(growable.begin(), growable.end(), [](const auto* a, const auto* b) {
				return a->limit - a->base < b->limit - b->base;
				});
			for (std::size_t index = 0; index < growable.size() && free_space > 0; ++index) {
				auto& track = *growable[index];
				const double growth = std::min(track.limit - track.base,
					free_space / static_cast<double>(growable.size() - index));
				track.base += growth;
				free_space -= growth;
			}
		}

		double find_fraction(std::vector<const sized_track*>& flexible, double space) {
			double factors = 0;
			for (const auto* track : flexible) factors += track->factor;
			std::sort(flexible.begin(), flexible.end(), [](const auto* a, const auto* b) {
				return a->base / a->factor > b->base / b->factor;
				});
			// Freeze the tracks whose minimum exceeds their share, largest ratio first.
			for (const auto* candidate : flexible) {
				const auto& track = *candidate;
				if (space / std::max(1.0, factors) * track.factor >= track.base) break;
				space -= track.base;
				factors -= track.factor;
			}
			return std::max(0.0, space) / std::max(1.0, factors);
		}

		void grow_flexible_tracks(std::vector<sized_track>& tracks, double& free_space) {
			std::vector<const sized_track*> flexible;
			double space = free_space;
			for (const auto& track : tracks) {
				if (track.factor <= 0) continue;
				flexible.push_back(&track);
				space += track.base;
			}
			const double fraction = find_fraction(flexible, space);
			for (auto& track : tracks) {
				if (track.factor <= 0) continue;
				const double size = std::max(track.base, track.factor * fraction);
				free_space -= size - track.base;
				track.base = size;
			}
		}

		struct track_increase {
			std::size_t index;
			double capacity;
			double factor;
			double increase = 0;
			bool intrinsic_maximum;
		};

		enum class span_sizing { base, minimum_limit, preferred_limit };

		// Equal distribution up to each limit, with saturated tracks frozen.
		void distribute_within_limits(std::vector<track_increase>& entries, double& space) {
			std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.capacity < b.capacity; });
			for (std::size_t i = 0; i < entries.size() && space > 0; ++i) {
				auto& entry = entries[i];
				entry.increase = std::min(entry.capacity, space / (entries.size() - i));
				space -= entry.increase;
			}
		}

		void span_increase(const std::vector<sized_track>& sizes, const grid_track_contribution& item, double span_gaps,
			bool flexible, span_sizing step, std::vector<double>& planned, std::vector<track_increase>& eligible,
			std::vector<track_increase>& others) {
			const bool grow_limits = step != span_sizing::base;
			double space = (step == span_sizing::preferred_limit ? item.preferred : item.minimum) - span_gaps;
			eligible.clear();
			others.clear();
			for (auto i = item.range.start; i < item.range.end; ++i) {
				const auto& track = sizes[i];
				space -= grow_limits ? track.limit : track.base;
				const bool applies = grow_limits ? track.automatic_maximum :
					track.automatic_minimum && (!flexible || track.flexible);
				const double capacity = track.unbounded || track.flexible ? std::numeric_limits<double>::infinity() :
					grow_limits ? 0 : track.limit - track.base;
				if (applies) {
					eligible.push_back({ i, capacity, track.factor, 0, track.automatic_maximum });
				} else if (!flexible && capacity > 0) {
					others.push_back({ i, capacity, 0, 0, false });
				}
			}
			if (space <= 0 || eligible.empty()) return;

			if (flexible) {
				const double factors = std::accumulate(eligible.begin(), eligible.end(), 0.0,
					[](double sum, const auto& entry) { return sum + entry.factor; });
				const double equal = space * std::max(0.0, 1.0 - factors) / eligible.size();
				for (auto& entry : eligible) entry.increase = space * entry.factor / std::max(1.0, factors) + equal;
			} else {
				distribute_within_limits(eligible, space);
				distribute_within_limits(others, space);
				// If intrinsic minima still need space, prefer tracks with an
				// intrinsic maximum for the beyond-limit part of the increase.
				const auto intrinsic = std::count_if(eligible.begin(), eligible.end(),
					[](const auto& entry) { return entry.intrinsic_maximum; });
				const double extra = space / (intrinsic ? intrinsic : eligible.size());
				for (auto& entry : eligible)
					if (!intrinsic || entry.intrinsic_maximum) entry.increase += extra;
			}
			for (const auto& entry : eligible)
				planned[entry.index] = std::max(planned[entry.index], entry.increase);
			for (const auto& entry : others)
				planned[entry.index] = std::max(planned[entry.index], entry.increase);
		}

		void resolve_spans(std::vector<sized_track>& sizes, const std::vector<const grid_track_contribution*>& spans,
			const grid_track_gutters& gutters, double gap, bool flexible) {
			if (spans.empty()) return;
			std::vector<double> planned(sizes.size());
			std::vector<track_increase> eligible;
			std::vector<track_increase> others;
			for (auto group = spans.begin(); group != spans.end();) {
				const auto group_end = flexible ? spans.end() : std::find_if(group + 1, spans.end(), [&](const auto* item) {
					return span_of(item->range) != span_of((*group)->range);
				});
				const auto plan_increases = [&](span_sizing step) {
					std::fill(planned.begin(), planned.end(), 0);
					for (auto it = group; it != group_end; ++it) {
						const auto& item = **it;
						span_increase(sizes, item, gap * gutters.count_inside(item.range), flexible, step, planned, eligible, others);
					}
				};
				plan_increases(span_sizing::base);
				for (std::size_t i = 0; i < sizes.size(); ++i) {
					sizes[i].base += planned[i];
					sizes[i].limit = std::max(sizes[i].limit, sizes[i].base);
				}
				if (!flexible) {
					plan_increases(span_sizing::minimum_limit);
					for (std::size_t i = 0; i < sizes.size(); ++i) sizes[i].limit += planned[i];
					plan_increases(span_sizing::preferred_limit);
					for (std::size_t i = 0; i < sizes.size(); ++i) {
						sizes[i].limit += planned[i];
					}
					// The unbounded tracks can absorb both limit increases for this span length.
					for (auto it = group; it != group_end; ++it)
						for (auto i = (*it)->range.start; i < (*it)->range.end; ++i) sizes[i].unbounded = false;
				}
				group = group_end;
			}
		}
	}

	bool parse_grid_tracks(const css_token_vector& tokens, grid_track_template& tracks) {
		grid_track_template parsed;
		if (tokens.size() == 1 && tokens.front().ident() == "none") {
			tracks = {};
			return true;
		}
		std::size_t available_tracks = grid_track_limit;
		for (const auto& token : tokens) {
			if (token.type == CV_FUNCTION && lowcase(token.name) == "repeat") {
				const auto& args = token.value;
				if (args.size() < 3 || args[1].ch != ',') return false;
				const auto keyword = args[0].ident();
				const bool automatic = keyword == "auto-fill" || keyword == "auto-fit";
				std::int64_t count = 1;
				if (!automatic && (!read_integer(args[0], count) || count <= 0)) return false;
				const std::size_t length = args.size() - 2;
				if (static_cast<std::uint64_t>(count) > available_tracks / length) return false;
				grid_track_list pattern(length);
				for (std::size_t i = 0; i < length; ++i)
					if (!parse_track(args[i + 2], pattern[i])) return false;
				available_tracks -= static_cast<std::size_t>(count) * length;
				if (automatic) {
					if (parsed.repeat || !std::all_of(pattern.begin(), pattern.end(), has_fixed_breadth)) return false;
					parsed.repeat = grid_auto_repeat{ parsed.tracks.size(), std::move(pattern), keyword == "auto-fit" };
				} else {
					for (std::int64_t i = 0; i < count; ++i)
						parsed.tracks.insert(parsed.tracks.end(), pattern.begin(), pattern.end());
				}
			}
			else {
				grid_track track;
				if (available_tracks == 0 || !parse_track(token, track)) return false;
				parsed.tracks.push_back(std::move(track));
				--available_tracks;
			}
		}
		if (parsed.repeat) {
			if (!std::all_of(parsed.tracks.begin(), parsed.tracks.end(), has_fixed_breadth)) return false;
		} else if (parsed.tracks.empty()) return false;
		tracks = std::move(parsed);
		return true;
	}

	expanded_grid_tracks expand_grid_tracks(const grid_track_template& axis, const css_length& gap,
		std::optional<pixel_t> definite, std::optional<pixel_t> minimum, std::optional<pixel_t> maximum) {
		if (!axis.repeat) return { axis.tracks, std::nullopt };

		const auto& repeat = *axis.repeat;
		const auto count = auto_repeat_count(axis, gap, definite, minimum, maximum);
		const auto repeated = count * repeat.pattern.size();
		const auto insertion = axis.tracks.begin() + repeat.insertion;
		expanded_grid_tracks expanded;
		expanded.tracks.reserve(axis.tracks.size() + repeated);
		expanded.tracks.insert(expanded.tracks.end(), axis.tracks.begin(), insertion);
		for (std::size_t i = 0; i < count; ++i)
			expanded.tracks.insert(expanded.tracks.end(), repeat.pattern.begin(), repeat.pattern.end());
		expanded.tracks.insert(expanded.tracks.end(), insertion, axis.tracks.end());
		if (repeat.fit) expanded.fit_range = grid_track_range{ repeat.insertion, repeat.insertion + repeated };
		return expanded;
	}

	grid_track_gutters finish_grid_tracks(expanded_grid_tracks& axis, std::size_t offset, std::size_t count,
		const std::vector<grid_item_placement>& items, grid_track_range grid_item_placement::* dimension) {
		axis.tracks.insert(axis.tracks.begin(), offset, grid_track{});
		axis.tracks.resize(count);
		grid_track_gutters gutters{ count, {} };
		if (!axis.fit_range) return gutters;

		const auto start = axis.fit_range->start + offset;
		const auto end = axis.fit_range->end + offset;
		std::vector<std::ptrdiff_t> coverage(end - start + 1);
		for (const auto& item : items) {
			const auto& range = item.*dimension;
			const auto first = std::max(start, range.start);
			const auto last = std::min(end, range.end);
			if (first < last) {
				++coverage[first - start];
				--coverage[last - start];
			}
		}
		const css_length zero(0);
		const grid_breadth zero_breadth{ grid_breadth_kind::length, zero };
		gutters.active_before.resize(count + 1);
		std::ptrdiff_t occupied = 0;
		for (std::size_t i = 0; i < count; ++i) {
			bool active = true;
			if (i >= start && i < end) {
				occupied += coverage[i - start];
				active = occupied > 0;
				if (!active) axis.tracks[i] = { zero_breadth, zero_breadth };
			}
			gutters.active_before[i + 1] = gutters.active_before[i] + active;
		}
		if (gutters.active_before.back() == count) gutters.active_before.clear();
		return gutters;
	}

	std::size_t grid_track_gutters::count_inside(grid_track_range range) const {
		const auto active = active_before.empty() ? span_of(range) : active_before[range.end] - active_before[range.start];
		return active > 0 ? active - 1 : 0;
	}

	bool grid_track_gutters::after(std::size_t track) const {
		if (active_before.empty()) return track + 1 < track_count;
		return active_before[track] < active_before[track + 1] && active_before[track + 1] < active_before.back();
	}

	bool parse_grid_line(const css_token_vector& tokens, grid_line& line) {
		return !tokens.empty() && parse_line(tokens.data(), tokens.data() + tokens.size(), line);
	}

	bool parse_grid_axis_placement(const css_token_vector& tokens, grid_axis_placement& placement) {
		if (tokens.empty()) return false;
		const auto* begin = tokens.data();
		const auto* end = begin + tokens.size();
		const auto* slash = std::find_if(begin, end, [](const auto& token) { return token.ch == '/'; });
		return parse_line(begin, slash, placement.start) &&
			(slash == end || parse_line(slash + 1, end, placement.end));
	}

	bool grid_tracks_use_percentage(const grid_track_list& tracks) {
		return std::any_of(tracks.begin(), tracks.end(), [](const auto& track) {
			return uses_percentage(track.minimum) || uses_percentage(track.maximum);
			});
	}

	bool grid_range_needs_content(const grid_track_list& tracks, grid_track_range range, bool definite) {
		for (auto i = range.start; i < range.end; ++i) {
			const auto& track = tracks[i];
			if (used_kind(track.minimum, definite) == grid_breadth_kind::automatic ||
				used_kind(track.maximum, definite) == grid_breadth_kind::automatic ||
				(!definite && track.maximum.kind == grid_breadth_kind::fraction && track.maximum.fraction > 0)) return true;
		}
		return false;
	}

	bool grid_range_has_auto_minimum(const grid_track_list& tracks, grid_track_range range, bool definite) {
		if (span_of(range) == 1) return true;
		bool automatic = false;
		for (auto i = range.start; i < range.end; ++i) {
			if (tracks[i].maximum.kind == grid_breadth_kind::fraction) return false;
			automatic |= used_kind(tracks[i].minimum, definite) == grid_breadth_kind::automatic;
		}
		return automatic;
	}

	grid_track_sizes size_grid_tracks(
		const grid_track_list& tracks,
		const std::vector<grid_track_contribution>& contributions,
		const grid_track_gutters& gutters,
		pixel_t gap,
		std::optional<pixel_t> available
	) {
		grid_track_sizes result;
		if (tracks.empty()) return result;
		const double gaps = static_cast<double>(gap) * gutters.count_inside({ 0, tracks.size() });
		const auto basis = available.value_or(0);
		std::vector<sized_track> sizes;
		sizes.reserve(tracks.size());
		std::vector<grid_track_contribution> singles(tracks.size());
		std::vector<const grid_track_contribution*> spans;
		std::vector<const grid_track_contribution*> flexible_spans;
		for (const auto& item : contributions) {
			if (span_of(item.range) == 1) {
				auto& single = singles[item.range.start];
				single.minimum = std::max(single.minimum, item.minimum);
				single.preferred = std::max(single.preferred, item.preferred);
			} else {
				bool crosses_flexible = false;
				for (auto i = item.range.start; i < item.range.end; ++i)
					if (tracks[i].maximum.kind == grid_breadth_kind::fraction) {
						crosses_flexible = true;
						break;
					}
				(crosses_flexible ? flexible_spans : spans).push_back(&item);
			}
		}
		double intrinsic_fraction = 0;
		for (std::size_t i = 0; i < tracks.size(); ++i) {
			const auto& track = tracks[i];
			const auto& content = singles[i];
			const auto min_kind = used_kind(track.minimum, available.has_value());
			const auto max_kind = used_kind(track.maximum, available.has_value());
			const double fixed_max = max_kind == grid_breadth_kind::length ? track.maximum.length.calc_percent(basis) : 0;
			sized_track size;
			size.base = min_kind == grid_breadth_kind::length ? track.minimum.length.calc_percent(basis) : content.minimum;
			if (min_kind == grid_breadth_kind::automatic && max_kind == grid_breadth_kind::length)
				size.base = std::min(size.base, fixed_max);
			size.base = std::max(0.0, size.base);
			size.automatic_maximum = max_kind == grid_breadth_kind::automatic;
			size.automatic_minimum = min_kind == grid_breadth_kind::automatic;
			size.limit = std::max(size.base, size.automatic_maximum ? static_cast<double>(content.preferred) : fixed_max);
			size.factor = track.maximum.fraction;
			size.flexible = max_kind == grid_breadth_kind::fraction;
			size.unbounded = size.automatic_maximum && content.preferred == 0;
			sizes.push_back(size);
		}
		std::sort(spans.begin(), spans.end(), [](const auto* a, const auto* b) { return span_of(a->range) < span_of(b->range); });
		resolve_spans(sizes, spans, gutters, gap, false);
		resolve_spans(sizes, flexible_spans, gutters, gap, true);
		const double minimum = std::accumulate(sizes.begin(), sizes.end(), gaps, [](double sum, const auto& track) {
			return sum + track.base;
		});
		result.minimum = static_cast<pixel_t>(minimum);
		if (available) {
			double free_space = std::max(0.0, *available - minimum);
			grow_to_limits(sizes, free_space);
			grow_flexible_tracks(sizes, free_space);
			const auto automatic = std::count_if(sizes.begin(), sizes.end(), [](const auto& track) { return track.automatic_maximum; });
			if (automatic && free_space > 0)
				for (auto& track : sizes)
					if (track.automatic_maximum) track.base += free_space / static_cast<double>(automatic);
		}
		else {
			for (std::size_t i = 0; i < sizes.size(); ++i)
				if (sizes[i].factor > 0)
					intrinsic_fraction = std::max(intrinsic_fraction, std::max(sizes[i].base, static_cast<double>(singles[i].preferred)) /
						std::max(1.0, sizes[i].factor));
			std::vector<const sized_track*> flexible;
			for (const auto* item : flexible_spans) {
				flexible.clear();
				double space = item->preferred - static_cast<double>(gap) * gutters.count_inside(item->range);
				for (auto i = item->range.start; i < item->range.end; ++i) {
					const auto& track = sizes[i];
					if (track.factor > 0) flexible.push_back(&track);
					else space -= track.limit;
				}
				intrinsic_fraction = std::max(intrinsic_fraction, find_fraction(flexible, space));
			}
			for (auto& track : sizes) track.base = std::max(track.limit, track.factor * intrinsic_fraction);
		}
		double extent = gaps;
		result.tracks.reserve(sizes.size());
		for (const auto& size : sizes) {
			result.tracks.push_back(static_cast<pixel_t>(size.base));
			extent += size.base;
		}
		result.extent = static_cast<pixel_t>(extent);
		return result;
	}
}
