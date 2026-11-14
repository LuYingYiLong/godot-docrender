#include "css_color.h"

#include <litehtml/css_tokenizer.h>
#include <litehtml/html.h>
#include <litehtml/web_color.h>

#include <algorithm>
#include <cmath>

namespace litehtml {
	namespace {
		constexpr double PI = 3.14159265358979323846;

		bool parse_component(const css_token& token, double percent_scale, double& value) {
			if (token.ident() == "none") {
				value = 0;
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

		byte to_srgb_byte(double linear_channel) {
			const double linear = std::clamp(linear_channel, 0.0, 1.0);
			const double srgb = linear <= 0.0031308 ? linear * 12.92 :
				1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
			return static_cast<byte>(std::lround(srgb * 255.0));
		}
	}

	bool parse_oklch(const css_token& token, web_color& color) {
		if (token.type != CV_FUNCTION || lowcase(token.name) != "oklch") {
			return false;
		}
		const auto& values = token.value;
		if (values.size() != 3 && (values.size() != 5 || values[3].ch != '/')) {
			return false;
		}
		if (!values[1].preceded_by_space || !values[2].preceded_by_space) {
			return false;
		}
		double lightness, chroma, hue;
		double alpha = 1.0;
		if (!parse_component(values[0], 1.0, lightness) ||
			!parse_component(values[1], 0.4, chroma) ||
			!parse_hue(values[2], hue) ||
			(values.size() == 5 && !parse_component(values[4], 1.0, alpha))) {
			return false;
		}

		lightness = std::clamp(lightness, 0.0, 1.0);
		chroma = std::max(chroma, 0.0);
		const auto opacity = static_cast<byte>(std::lround(std::clamp(alpha, 0.0, 1.0) * 255.0));
		if (lightness == 0.0 || lightness == 1.0) {
			const byte gray = lightness == 0.0 ? 0 : 255;
			color = web_color(gray, gray, gray, opacity);
			return true;
		}

		const double angle = std::fmod(hue, 360.0) * (PI / 180.0);
		const double a = chroma * std::cos(angle);
		const double b = chroma * std::sin(angle);
		// CSS Color 4 sample conversion from Oklab through XYZ D65 to linear sRGB.
		const auto cube = [](double v) { return v * v * v; };
		const double l = cube(lightness + 0.3963377774 * a + 0.2158037573 * b);
		const double m = cube(lightness - 0.1055613458 * a - 0.0638541728 * b);
		const double s = cube(lightness - 0.0894841775 * a - 1.2914855480 * b);
		color = web_color(
			to_srgb_byte(4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s),
			to_srgb_byte(-1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s),
			to_srgb_byte(-0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s),
			opacity);
		return true;
	}
}
