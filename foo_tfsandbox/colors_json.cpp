#include "stdafx.h"

#include <fcntl.h>
#include <io.h>
#include <string>
#include "pfc/string-conv-lite.h"
#include "jansson.h"
#include "version.h"
#include "colors_json.h"

colors_json::colors_json() {
	//..
}

colors_json::~colors_json()
{
	//..
}

std::filesystem::path colors_json::genFilePath(bool dark) {

	pfc::string8 n8_path = core_api::pathInProfile("configuration");
	extract_native_path(n8_path, n8_path);

	std::filesystem::path os_dst = std::filesystem::u8path(n8_path.c_str());

	pfc::string8 filename = core_api::get_my_file_name();
	filename << ".dll.dat";
	if (dark) { filename << "_dark"; }
	os_dst.append(filename.c_str());
	bool b_dst_exists = std::filesystem::exists(os_dst);

	return os_dst;
}

using Tokens = std::vector<std::string>;

Tokens StringSplit(const std::string& text, int separator) {
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


bool read_group(json_t* json, pfc::string8 groupname, const std::map<std::string, size_t> vdefs, std::vector<std::pair<size_t, tfRGB>>& vcolors) {

	bool res = false;
	std::pair<size_t, tfRGB> elem = {};

	size_t index;
	json_t* js_wobj;

	try {

		json_array_foreach(json, index, js_wobj) {

		if (!json_is_object(js_wobj)) return false;

		json_t* js_fld;

		js_fld = json_object_get(js_wobj, groupname);

		if (!js_fld) {
			return false;
		}

		if (json_is_array(js_fld)) {
			size_t n = 0; // json_array_size(js_fld);
			json_t* js;
			json_array_foreach(js_fld, n, js) {
				auto js_att = json_object_get(js, "id");
				const char* dmp_str_id = json_string_value(js_att);
				js_att = json_object_get(js, "rgb");
				const char* dmp_str_rgb = json_string_value(js_att);

				using Tokens = std::vector<std::string>;
				auto tokens = StringSplit(dmp_str_rgb, ',');

				if (tokens.size() == 3) {

					size_t chv;
					size_t c = 0;

					//red

					pfc::string8 w = tokens.at(0).c_str(); w = w.trim(' ');
					bool is_n = pfc::string_is_numeric(w);
					if (is_n && (chv = atoi(w)) < 256) {
						elem.second.r = static_cast<uint8_t>(chv);
						c++;
					}

					//green

					w = tokens.at(1).c_str(); w = w.trim(' ');
					is_n = pfc::string_is_numeric(w);
					if (is_n && (chv = atoi(w)) < 256) {
						elem.second.g = static_cast<uint8_t>(chv);
						c++;
					}

					//blue

					w = tokens.at(2).c_str(); w = w.trim(' ');
					is_n = pfc::string_is_numeric(w);
					if (is_n && (chv = atoi(w)) < 256) {
						elem.second.b = static_cast<uint8_t>(chv);
						c++;
					}
					if (c == 3) {
						auto it = vdefs.find(dmp_str_id);
						if (it != vdefs.end()) {
							vcolors.emplace_back(std::pair(it->second, elem.second));
						}
					}
				}
			}
		} // for each
		return true;
	}
}
	catch (...) {
		throw exception_io();
	}
	return res;
}

bool colors_json::read_colors_json(bool dark) {

	bool bres = false;

	try {

		std::vector<std::pair<size_t, tfRGB>> vtempcolors;

		int jf = -1;
		std::filesystem::path os_file;

		try {

			os_file = genFilePath(dark);

			jf = _wopen(os_file.wstring().c_str(), _O_RDONLY);

			if (jf == -1) {
				foobar2000_io::exception_io e("Open failed on input file");
				throw e;
			}

			json_error_t error;
			auto json = json_loadfd(jf, JSON_DECODE_ANY, &error);
			_close(jf); jf = -1;

			if (strlen(error.text) && error.line != -1) {
				console::formatter() << "[" << COMPONENT_NAME << "] : " <<
				"JSON error: " << error.text << ", in line : " << error.line << ", column: " << error.column <<
					", position: " << error.position << ", src: " << error.source;

			}

			//todo
			std::vector<std::pair<size_t, tfRGB>> vtmp_gen_colors;
			bool res = read_group(json, "gen colors", mgen_colors, vtmp_gen_colors);
			if (!res) throw exception_io();
			std::vector<std::pair<size_t, tfRGB>> vtmp_lex_colors;
			res = read_group(json, "lex colors", mlex_colors, vtmp_lex_colors);
			if (!res) throw exception_io();
			std::vector<std::pair<size_t, tfRGB>> vtmp_ind_colors;
			res = read_group(json, "ind colors", mindicalors_colors, vtmp_ind_colors);
			if (!res) throw exception_io();

			//todo
			bool bsame_sizes = (vtmp_gen_colors.size() == vgen_colors.size()) && (vtmp_lex_colors.size() == vlex_colors.size()) && (vtmp_ind_colors.size() == vindicator_colors.size());
			if (bsame_sizes) {
				for (auto c : vtmp_gen_colors) {
					vgen_colors.at(c.first).second = c.second;
				}
				for (auto c : vtmp_lex_colors) {
					vlex_colors.at(c.first).second = c.second;
				}
				for (auto c : vtmp_ind_colors) {
					vindicator_colors.at(c.first).second = c.second;
				}

				console::formatter() << "[" << COMPONENT_NAME << "] : Lexer theme loaded.";
				//..
				return true;
				//..
			}
			else {
				throw exception_io();
			}
		}
		catch (foobar2000_io::exception_io e) {
			if (jf != -1) {
				_close(jf);
			}
			console::formatter() << "[" << COMPONENT_NAME << "] : Lexer theme not loaded."; // << os_file.c_str();
			bres = false;
		}
		catch (...) {
			if (jf != -1) {
				_close(jf);
			}
			console::formatter() << "[" << COMPONENT_NAME << "] :Reading data from file failed:", " Unhandled Exception";
			bres = false;
		}
	}
	catch (...) {
		//..
		bres = false;
	}

	return bres;
}