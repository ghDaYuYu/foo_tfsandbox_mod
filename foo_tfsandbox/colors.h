#pragma once
#include <string>
#include <vector>
#include <map>

struct tfRGB {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

inline std::map<std::string, size_t> mlex_colors{
	{"default",        0}, // "SCE_TITLEFORMAT_DEFAULT"
	{"comment",        1}, // "SCE_TITLEFORMAT_COMMENT"
	{"operator",       2}, //"SCE_TITLEFORMAT_OPERATOR"
	{"identifier",     3}, //"SCE_TITLEFORMAT_IDENTIFIER"
	{"string",         4}, //"SCE_TITLEFORMAT_STRING"
	{"literal string", 5}, //"SCE_TITLEFORMAT_LITERALSTRING"
	{"special string", 6}, //"SCE_TITLEFORMAT_SPECIALSTRING"
	{"field",          7}, //"SCE_TITLEFORMAT_FIELD"
	{"Keyword1",       8}, //"SCE_TITLEFORMAT_KEYWORD1"
	{"Keyword2",       9}, //"SCE_TITLEFORMAT_KEYWORD2"
	{"Keyword3",      10}, //"SCE_TITLEFORMAT_KEYWORD3"
	{"Keyword4",      11}, //"SCE_TITLEFORMAT_KEYWORD4"
};

inline const std::vector < std::pair<size_t, tfRGB>> vlex_colors_defaults = {
	/*default*/          { 0,{   0,   0,   0}},
	/*comment*/          { 1,{   0, 128,   0}},
	/*operator*/         { 2,{   0,   0,   0}},
	/*identifier*/       { 3,{ 192,   0, 192}},
	/*string*/           { 4,{   0,   0,   0}},
	/*literal string*/   { 5,{   0,   0,   0}},
	/*special string*/   { 6,{   0,   0,   0}},
	/*field*/            { 7,{   0,   0, 192}},
	/*Keyword1*/         { 8,{   0,   0,   0}},
	/*Keyword2*/         { 9,{   0,   0,   0}},
	/*Keyword3*/         {10,{   0,   0,   0}},
	/*Keyword4*/         {11,{   0,   0,   0}}
};

inline std::vector < std::pair<size_t, tfRGB>> vlex_colors = vlex_colors_defaults;

inline std::map<std::string, size_t> mindicalors_colors{
	{"inactive code",  0},
	{"fragment",       1},
	{"error",          2},
};

inline const std::vector < std::pair<size_t, tfRGB>> vindicator_colors_defaults = {
	/*inactive code*/  { 0,{   0,   0,   0}},
	/*fragment*/       { 1,{  64, 128, 255}},
	/*error*/          { 2,{ 255,   0,   0}},
};

inline std::vector < std::pair<size_t, tfRGB>> vindicator_colors = vindicator_colors_defaults;

inline std::map<std::string, size_t> mgen_colors{
	{"foreground",                   0},
	{"background",                   1},
	{"selection foreground",         2},
	{"selection background",         3},
	{"calltip foreground",           4},
	{"calltip background",           5},
	{"marker foreground",            6},
	{"marker background",            7},
	{"marker selected background",   8},
};

inline const std::vector < std::pair<size_t, tfRGB>> vgen_colors_defaults = {
	/*foreground*/                   { 0,{   0,   0,   0}},  //
	/*background*/                   { 1,{ 255, 255, 255}},  //
	/*selection foreground*/         { 2,{ 255, 255, 255}},  //
	/*selection background*/         { 3,{  51, 153, 255}},  //
	/*calltip foreground*/           { 4,{   0,   0,   0}},  // ::GetSysColor(COLOR_INFOTEXT));
	/*calltip background*/           { 5,{ 255, 255, 255}},  // ::GetSysColor(COLOR_INFOBK));
	/*marker foreground*/            { 6,{ 255, 255, 255}},  //
	/*marker background*/            { 7,{ 128, 128, 128}},  //
	/*marker selected background*/   { 8,{   0,   0, 255}},  //
};

inline std::vector < std::pair<size_t, tfRGB>> vgen_colors = vgen_colors_defaults;

inline COLORREF get_lex_color(size_t ndx) {
	tfRGB rgb = { 0 };
	auto it = std::find_if(vlex_colors.begin(), vlex_colors.end(),
		[ndx](std::pair<size_t, tfRGB> p) {
			return p.first == ndx;
		});
	if (it != vlex_colors.end()) {
		rgb = it->second;
	}
	return RGB(rgb.r, rgb.g, rgb.b);
}

inline COLORREF get_gen_color(size_t ndx) {
	tfRGB rgb = { 0 };
	auto it = std::find_if(vgen_colors.begin(), vgen_colors.end(),
		[ndx](std::pair<size_t, tfRGB> p) {
			return p.first == ndx;
		});
	if (it != vgen_colors.end()) {
		rgb = it->second;
	}
	return RGB(rgb.r, rgb.g, rgb.b);
}