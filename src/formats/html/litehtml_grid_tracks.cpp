#include "litehtml_grid_tracks.h"

#include <litehtml/html.h>

#include <algorithm>
#include <cmath>

namespace litehtml {
	namespace {
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

		struct sized_track {
			double base = 0;
			double limit = 0;
			double factor = 0;
			bool automatic_maximum = false;
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

		void grow_flexible_tracks(std::vector<sized_track>& tracks, double& free_space) {
			std::vector<sized_track*> flexible;
			double factors = 0;
			double space = free_space;
			for (auto& track : tracks) {
				if (track.factor <= 0) continue;
				flexible.push_back(&track);
				factors += track.factor;
				space += track.base;
			}
			std::sort(flexible.begin(), flexible.end(), [](const auto* a, const auto* b) {
				return a->base / a->factor > b->base / b->factor;
				});
			// Freeze the tracks whose minimum exceeds their share, largest ratio first.
			auto first = flexible.begin();
			for (; first != flexible.end(); ++first) {
				const auto& track = **first;
				if (space / std::max(1.0, factors) * track.factor >= track.base) break;
				space -= track.base;
				factors -= track.factor;
			}
			const double fraction = space / std::max(1.0, factors);
			for (; first != flexible.end(); ++first) {
				auto& track = **first;
				const double size = std::max(track.base, track.factor * fraction);
				free_space -= size - track.base;
				track.base = size;
			}
		}
	}

	bool parse_grid_tracks(const css_token_vector& tokens, grid_track_list& tracks) {
		if (tokens.size() == 1 && tokens.front().ident() == "none") return true;
		for (const auto& token : tokens) {
			grid_track track;
			if (token.type == CV_FUNCTION && lowcase(token.name) == "minmax") {
				const auto& args = token.value;
				if (args.size() != 3 || args[1].ch != ',' ||
					!parse_breadth(args[0], false, track.minimum) || !parse_breadth(args[2], true, track.maximum)) return false;
			}
			else {
				if (!parse_breadth(token, true, track.maximum)) return false;
				if (track.maximum.kind != grid_breadth_kind::fraction) track.minimum = track.maximum;
			}
			tracks.push_back(std::move(track));
		}
		return !tracks.empty();
	}

	bool grid_tracks_use_percentage(const grid_track_list& tracks) {
		return std::any_of(tracks.begin(), tracks.end(), [](const auto& track) {
			return uses_percentage(track.minimum) || uses_percentage(track.maximum);
			});
	}

	bool grid_track_needs_content(const grid_track& track, bool definite) {
		return used_kind(track.minimum, definite) == grid_breadth_kind::automatic ||
			used_kind(track.maximum, definite) == grid_breadth_kind::automatic ||
			(!definite && track.maximum.kind == grid_breadth_kind::fraction && track.maximum.fraction > 0);
	}

	grid_track_sizes size_grid_tracks(
		const grid_track_list& tracks,
		const std::vector<grid_track_contribution>& contributions,
		pixel_t gap,
		std::optional<pixel_t> available
	) {
		grid_track_sizes result;
		if (tracks.empty()) return result;
		const double gaps = static_cast<double>(gap) * (tracks.size() - 1);
		const auto basis = available.value_or(0);
		std::vector<sized_track> sizes;
		sizes.reserve(tracks.size());
		double minimum = gaps;
		double intrinsic_fraction = 0;
		for (std::size_t i = 0; i < tracks.size(); ++i) {
			const auto& track = tracks[i];
			const auto& content = contributions[i];
			const auto min_kind = used_kind(track.minimum, available.has_value());
			const auto max_kind = used_kind(track.maximum, available.has_value());
			const double fixed_max = max_kind == grid_breadth_kind::length ? track.maximum.length.calc_percent(basis) : 0;
			sized_track size;
			size.base = min_kind == grid_breadth_kind::length ? track.minimum.length.calc_percent(basis) : content.minimum;
			if (min_kind == grid_breadth_kind::automatic && max_kind == grid_breadth_kind::length)
				size.base = std::min(size.base, fixed_max);
			size.base = std::max(0.0, size.base);
			size.automatic_maximum = max_kind == grid_breadth_kind::automatic;
			size.limit = std::max(size.base, size.automatic_maximum ? static_cast<double>(content.preferred) : fixed_max);
			size.factor = track.maximum.fraction;
			if (!available && size.factor > 0)
				intrinsic_fraction = std::max(intrinsic_fraction, std::max(size.base, static_cast<double>(content.preferred)) /
					std::max(1.0, size.factor));
			minimum += size.base;
			sizes.push_back(size);
		}
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
