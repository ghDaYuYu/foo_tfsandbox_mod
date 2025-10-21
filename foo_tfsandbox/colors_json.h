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

private:
	static std::filesystem::path genFilePath(bool dst, bool dark);
};
