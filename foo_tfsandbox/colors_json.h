#pragma once
#include <filesystem>
#include <vector>
#include "colors.h"

class colors_json {
private:
	//..
public:
	colors_json();
	~colors_json();

	bool read_colors_json(bool dark);
	static bool copy_installation_theme_files();

	static bool is_dark_luminance(COLORREF fore, COLORREF back);

private:
	static std::filesystem::path genFilePath(bool dst, bool dark);
};

inline bool colors_json::is_dark_luminance(COLORREF fore, COLORREF back) {

	const uint8_t Rf = GetRValue(fore);
	const uint8_t Gf = GetGValue(fore);
	const uint8_t Bf = GetBValue(fore);

	const uint8_t Rb = GetRValue(back);
	const uint8_t Gb = GetGValue(back);
	const uint8_t Bb = GetBValue(back);

	const double rgf = Rf <= 10 ? Rf / 3294.0 : std::pow((Rf / 269.0) + 0.0513, 2.4);
	const double ggf = Gf <= 10 ? Gf / 3294.0 : std::pow((Gf / 269.0) + 0.0513, 2.4);
	const double bgf = Bf <= 10 ? Bf / 3294.0 : std::pow((Bf / 269.0) + 0.0513, 2.4);

	double lum_fore = (1 - 0.2126 * rgf) + (1 - 0.7152 * ggf) + (1 - 0.0722 * bgf);

	const double rgb = Rb <= 10 ? Rb / 3294.0 : std::pow((Rb / 269.0) + 0.0513, 2.4);
	const double ggb = Gb <= 10 ? Gb / 3294.0 : std::pow((Gb / 269.0) + 0.0513, 2.4);
	const double bgb = Bb <= 10 ? Bb / 3294.0 : std::pow((Bb / 269.0) + 0.0513, 2.4);

	double lum_back = (1 - 0.2126 * rgb) + (1 - 0.7152 * ggb) + (1 - 0.0722 * bgb);
	return lum_fore < lum_back;
}

using Tokens = std::vector<std::string>;

inline Tokens StringSplit(const std::string& text, int separator) {
	Tokens vs(text.empty() ? 0 : 1);
	for (const char ch : text) {
		if (ch == separator) {
			vs.emplace_back();
		}
		else {
			vs.back() += ch;
		}
	}
	return vs;
}