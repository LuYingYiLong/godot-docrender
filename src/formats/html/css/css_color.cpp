#include "css_color.h"

#include <litehtml/css_parser.h>
#include <litehtml/html.h>
#include <litehtml/web_color.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <variant>

namespace litehtml {
	namespace {
		constexpr double PI = 3.14159265358979323846;
		constexpr unsigned COMPONENTS = 0b0111;
		constexpr unsigned ALPHA = 0b1000;
		constexpr unsigned MAX_MIX_DEPTH = 64;

		using Components = std::array<double, 4>;
		using Channels = std::array<double, 3>;

		struct ColorSample {
			css_color_space space;
			Components values;
			unsigned missing = 0;
		};

		constexpr ColorSample BLACK{ css_color_space::srgb, { 0.0, 0.0, 0.0, 1.0 } };

		enum class HueInterpolation { shorter, longer, increasing, decreasing };

		struct ColorMix {
			css_color_space space = css_color_space::oklab;
			HueInterpolation hue = HueInterpolation::shorter;
			web_color first;
			web_color second;
			double progress = 0.5;
			double alpha_multiplier = 1.0;
		};

		bool parse_component(const css_token& token, double percent_scale, double& value) {
			if (token.ident() == "none") {
				value = 0.0;
				return true;
			}
			if (token.type != NUMBER && token.type != PERCENTAGE) {
				return false;
			}
			value = double(token.n.number) * (token.type == PERCENTAGE ? percent_scale / 100.0 : 1.0);
			return std::isfinite(value);
		}

		bool parse_hue(const css_token& token, double& degrees) {
			if (token.type != DIMENSION) {
				return token.type != PERCENTAGE && parse_component(token, 1.0, degrees);
			}
			const auto unit = lowcase(token.unit);
			const double value = token.n.number;
			if (unit == "deg") degrees = value;
			else if (unit == "grad") degrees = value * 0.9;
			else if (unit == "rad") degrees = value * (180.0 / PI);
			else if (unit == "turn") degrees = value * 360.0;
			else return false;
			return std::isfinite(degrees);
		}

		double normalize_hue(double hue) {
			const double degrees = std::fmod(hue, 360.0);
			return degrees < 0.0 ? degrees + 360.0 : degrees;
		}

		double to_linear(double channel) {
			const double magnitude = std::abs(channel);
			return magnitude <= 0.04045 ? channel / 12.92 :
				std::copysign(std::pow((magnitude + 0.055) / 1.055, 2.4), channel);
		}

		double to_srgb_channel(double channel) {
			const double magnitude = std::abs(channel);
			return magnitude <= 0.0031308 ? channel * 12.92 :
				std::copysign(1.055 * std::pow(magnitude, 1.0 / 2.4) - 0.055, channel);
		}

		Channels to_oklab(const Channels& rgb) {
			// CSS Color 4 conversion from linear sRGB through LMS to Oklab.
			const auto [r, g, b] = rgb;
			const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
			const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
			const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
			return {
				0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
				1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
				0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s,
			};
		}

		Channels oklab_to_linear(const Channels& lab) {
			const auto [lightness, a, b] = lab;
			const auto cube = [](double value) { return value * value * value; };
			const double l = cube(lightness + 0.3963377774 * a + 0.2158037573 * b);
			const double m = cube(lightness - 0.1055613458 * a - 0.0638541728 * b);
			const double s = cube(lightness - 0.0894841775 * a - 1.2914855480 * b);
			return {
				4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
				-1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
				-0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s,
			};
		}

		Channels oklch_to_oklab(const Channels& lch) {
			const auto [lightness, chroma, hue] = lch;
			const double angle = hue * (PI / 180.0);
			return { lightness, chroma * std::cos(angle), chroma * std::sin(angle) };
		}

		Channels hsl_to_srgb(const Channels& hsl) {
			const auto [hue, saturation, lightness] = hsl;
			const double amplitude = saturation * std::min(lightness, 1.0 - lightness);
			const auto channel = [&](double offset) {
				const double k = std::fmod(offset + hue / 30.0, 12.0);
				return lightness - amplitude * std::max(-1.0, std::min({ k - 3.0, 9.0 - k, 1.0 }));
			};
			return { channel(0.0), channel(8.0), channel(4.0) };
		}

		bool is_ok_space(css_color_space space) {
			return space == css_color_space::oklab || space == css_color_space::oklch;
		}

		unsigned carry_missing(css_color_space from, css_color_space to, unsigned missing) {
			// CSS Color 4 carries analogous components and complete analogous sets.
			if ((missing & COMPONENTS) == COMPONENTS ||
				((from == css_color_space::srgb || from == css_color_space::srgb_linear) &&
				 (to == css_color_space::srgb || to == css_color_space::srgb_linear))) {
				return missing;
			}
			unsigned carried = missing & ALPHA;
			if (is_ok_space(from) && is_ok_space(to)) {
				carried |= missing & 1;
				if ((missing & 0b0110) == 0b0110) carried |= 0b0110;
			} else if (from == css_color_space::hsl && is_ok_space(to)) {
				carried |= (missing & 4) >> 2;
				if (to == css_color_space::oklch) {
					carried |= ((missing & 1) << 2) | (missing & 2);
				} else if ((missing & 0b0011) == 0b0011) {
					carried |= 0b0110;
				}
			}
			return carried;
		}

		ColorSample convert(ColorSample sample, css_color_space target) {
			if (sample.space == target) return sample;

			const auto from = sample.space;
			Channels channels{ sample.values[0], sample.values[1], sample.values[2] };
			if (from == css_color_space::oklch) channels = oklch_to_oklab(channels);
			if (from == css_color_space::hsl) channels = hsl_to_srgb(channels);

			if (from != css_color_space::hsl || target != css_color_space::srgb) {
				if (is_ok_space(from) && !is_ok_space(target)) channels = oklab_to_linear(channels);
				if (from == css_color_space::srgb || from == css_color_space::hsl) {
					for (double& channel : channels) channel = to_linear(channel);
				}
				if (is_ok_space(target) && !is_ok_space(from)) channels = to_oklab(channels);
				if (target == css_color_space::srgb) {
					for (double& channel : channels) channel = to_srgb_channel(channel);
				}
			}

			sample.missing = carry_missing(from, target, sample.missing);
			if (target == css_color_space::oklch) {
				const double chroma = std::hypot(channels[1], channels[2]);
				const double hue = normalize_hue(std::atan2(channels[2], channels[1]) * (180.0 / PI));
				channels = { channels[0], chroma, hue };
				if (chroma <= 0.000004) {
					channels[1] = 0.0;
					sample.missing |= 4;
				}
			}
			sample.space = target;
			for (unsigned i = 0; i < channels.size(); ++i) {
				sample.values[i] = sample.missing & (1u << i) ? 0.0 : channels[i];
			}
			return sample;
		}

		double mix_hue(double first, double second, double progress, HueInterpolation method) {
			const double difference = second - first;
			switch (method) {
				case HueInterpolation::shorter:
					if (difference > 180.0) first += 360.0;
					if (difference < -180.0) second += 360.0;
					break;
				case HueInterpolation::longer:
					if (0.0 < difference && difference < 180.0) first += 360.0;
					if (-180.0 < difference && difference <= 0.0) second += 360.0;
					break;
				case HueInterpolation::increasing:
					if (second < first) second += 360.0;
					break;
				case HueInterpolation::decreasing:
					if (first < second) first += 360.0;
					break;
			}
			return normalize_hue(first + (second - first) * progress);
		}

		ColorSample interpolate(const ColorMix& mix, ColorSample first, ColorSample second) {
			first = convert(first, mix.space);
			second = convert(second, mix.space);
			ColorSample result{ mix.space, {}, first.missing & second.missing };
			for (unsigned i = 0; i < result.values.size(); ++i) {
				const unsigned bit = 1u << i;
				if (first.missing & bit) first.values[i] = second.values[i];
				if (second.missing & bit) second.values[i] = first.values[i];
			}

			const double first_weight = 1.0 - mix.progress;
			const double second_weight = mix.progress;
			const double first_alpha = first.values[3] * first_weight;
			const double second_alpha = second.values[3] * second_weight;
			const double alpha = first_alpha + second_alpha;
			for (unsigned i = 0; i < 3; ++i) {
				if (result.missing & (1u << i)) continue;
				if (i == 2 && mix.space == css_color_space::oklch) {
					result.values[i] = mix_hue(first.values[i], second.values[i], second_weight, mix.hue);
				} else if (result.missing & ALPHA) {
					result.values[i] = first.values[i] * first_weight + second.values[i] * second_weight;
				} else if (alpha > 0.0) {
					result.values[i] = (first.values[i] * first_alpha + second.values[i] * second_alpha) / alpha;
				}
			}
			result.values[3] = alpha * mix.alpha_multiplier;
			return result;
		}

		bool parse_mix_space(const css_token_vector& tokens, ColorMix& mix) {
			if ((tokens.size() != 2 && tokens.size() != 4) || tokens[0].ident() != "in") return false;
			const auto space = tokens[1].ident();
			if (space == "srgb") mix.space = css_color_space::srgb;
			else if (space == "srgb-linear") mix.space = css_color_space::srgb_linear;
			else if (space == "oklab") mix.space = css_color_space::oklab;
			else if (space == "oklch") mix.space = css_color_space::oklch;
			else return false;

			if (tokens.size() == 2) return true;
			if (mix.space != css_color_space::oklch || tokens[3].ident() != "hue") return false;
			const auto method = tokens[2].ident();
			if (method == "shorter") mix.hue = HueInterpolation::shorter;
			else if (method == "longer") mix.hue = HueInterpolation::longer;
			else if (method == "increasing") mix.hue = HueInterpolation::increasing;
			else if (method == "decreasing") mix.hue = HueInterpolation::decreasing;
			else return false;
			return true;
		}

		bool parse_mix_item(const css_token_vector& tokens, web_color& color, double& percent,
			document_container* container, unsigned depth) {
			if (tokens.empty() || tokens.size() > 2) return false;
			size_t color_index = 0;
			if (tokens.size() == 2) {
				const size_t weight_index = tokens[0].type == PERCENTAGE ? 0 : 1;
				const auto& weight = tokens[weight_index];
				if (weight.type != PERCENTAGE || !std::isfinite(weight.n.number) ||
					weight.n.number < 0.0 || weight.n.number > 100.0) return false;
				percent = weight.n.number;
				color_index = 1 - weight_index;
			}
			return parse_color_at_depth(tokens[color_index], color, container, depth);
		}

		byte to_byte(double value) {
			return static_cast<byte>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
		}
	}

	struct css_color_data {
		const std::variant<ColorSample, ColorMix> value;
	};

	namespace {
		const ColorSample* get_sample(const web_color& color) {
			return color.css_data ? std::get_if<ColorSample>(&color.css_data->value) : nullptr;
		}

		ColorSample evaluate(const web_color& color, const ColorSample& current) {
			if (color.is_current_color) return current;
			if (const auto* sample = get_sample(color)) return *sample;
			if (color.css_data) {
				const auto& mix = std::get<ColorMix>(color.css_data->value);
				return interpolate(mix, evaluate(mix.first, current), evaluate(mix.second, current));
			}
			return { css_color_space::srgb, { color.red / 255.0, color.green / 255.0, color.blue / 255.0, color.alpha / 255.0 } };
		}
	}

	web_color make_css_color(css_color_space space, Components components, unsigned missing) {
		web_color color;
		color.css_data = std::make_shared<css_color_data>(css_color_data{ ColorSample{ space, components, missing } });
		return color;
	}

	bool web_color::needs_current_color() const {
		return is_current_color || (css_data && std::holds_alternative<ColorMix>(css_data->value));
	}

	web_color web_color::resolve(const web_color& current) const {
		if (!needs_current_color()) return *this;
		if (is_current_color) return current;
		const auto result = evaluate(*this, evaluate(current, BLACK));
		return make_css_color(result.space, result.values, result.missing);
	}

	web_color web_color::to_srgb(const web_color& current) const {
		if (!css_data && !is_current_color) return *this;
		const auto sample = evaluate(*this, needs_current_color() ? evaluate(current, BLACK) : BLACK);
		// Preserve the existing black/white output for OkLCh lightness endpoints.
		if (is_ok_space(sample.space) && (sample.values[0] <= 0.0 || sample.values[0] >= 1.0)) {
			const byte gray = sample.values[0] <= 0.0 ? 0 : 255;
			return web_color(gray, gray, gray, to_byte(sample.values[3]));
		}
		const auto srgb = convert(sample, css_color_space::srgb).values;
		return web_color(to_byte(srgb[0]), to_byte(srgb[1]), to_byte(srgb[2]), to_byte(srgb[3]));
	}

	bool parse_oklch(const css_token& token, web_color& color) {
		if (token.type != CV_FUNCTION || lowcase(token.name) != "oklch") return false;
		const auto& values = token.value;
		if (values.size() != 3 && (values.size() != 5 || values[3].ch != '/')) return false;
		if (!values[1].preceded_by_space || !values[2].preceded_by_space) return false;
		Components components{ 0.0, 0.0, 0.0, 1.0 };
		if (!parse_component(values[0], 1.0, components[0]) ||
			!parse_component(values[1], 0.4, components[1]) ||
			!parse_hue(values[2], components[2]) ||
			(values.size() == 5 && !parse_component(values[4], 1.0, components[3]))) return false;

		unsigned missing = 0;
		for (unsigned i = 0; i < 3; ++i) {
			if (values[i].ident() == "none") missing |= 1u << i;
		}
		if (values.size() == 5 && values[4].ident() == "none") missing |= ALPHA;
		components[0] = std::clamp(components[0], 0.0, 1.0);
		components[1] = std::max(components[1], 0.0);
		components[2] = normalize_hue(components[2]);
		components[3] = std::clamp(components[3], 0.0, 1.0);
		color = make_css_color(css_color_space::oklch, components, missing);
		return true;
	}

	bool parse_color_mix(const css_token& token, web_color& color, document_container* container, unsigned depth) {
		if (token.type != CV_FUNCTION || lowcase(token.name) != "color-mix" || depth >= MAX_MIX_DEPTH) return false;
		const auto arguments = parse_comma_separated_list(token.value);
		if (arguments.size() != 2 && arguments.size() != 3) return false;

		ColorMix mix;
		if (arguments.size() == 3 && !parse_mix_space(arguments.front(), mix)) return false;
		double first_percent = -1.0;
		double second_percent = -1.0;
		if (!parse_mix_item(arguments[arguments.size() - 2], mix.first, first_percent, container, depth + 1) ||
			!parse_mix_item(arguments.back(), mix.second, second_percent, container, depth + 1)) return false;

		if (first_percent < 0.0) first_percent = second_percent < 0.0 ? 50.0 : 100.0 - second_percent;
		if (second_percent < 0.0) second_percent = 100.0 - first_percent;
		const double total = first_percent + second_percent;
		if (total == 0.0) {
			color = web_color::transparent;
			return true;
		}
		mix.progress = second_percent / total;
		mix.alpha_multiplier = std::min(total / 100.0, 1.0);
		if (mix.first.needs_current_color() || mix.second.needs_current_color()) {
			color = web_color();
			color.css_data = std::make_shared<css_color_data>(css_color_data{ std::move(mix) });
		} else {
			const auto result = interpolate(mix, evaluate(mix.first, BLACK), evaluate(mix.second, BLACK));
			color = make_css_color(result.space, result.values, result.missing);
		}
		return true;
	}
}
