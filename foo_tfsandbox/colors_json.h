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

private:
	std::filesystem::path genFilePath(bool dark);
};
