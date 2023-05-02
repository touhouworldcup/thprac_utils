#include <Windows.h>
#include "rapidjson/document.h"
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <locale>
#include <codecvt>
#include <unordered_map>
#include "util.h"
#include "window.h"

using rapidjson::Document;

// Collections
using std::vector;
using std::map;
using std::unordered_map;
using std::set;
using std::pair;

// Strings
using std::string;
using std::wstring;
using std::wstring_convert;
using std::codecvt_utf8;

// Function object
using std::function;

int vsprintf_append(std::string& str, const char* format, va_list va) {
	va_list va2;

	va_copy(va2, va);
	int length = vsnprintf(NULL, 0, format, va2);
	va_end(va2);

	size_t prev_size = str.size();
	str.reserve(str.size() + length + 1);
	str.resize(str.size() + length);

	va_copy(va2, va);
	int ret = vsnprintf((char*) str.data() + prev_size, str.size() + 1, format, va2);
	va_end(va2);

	return ret;
}

int sprintf_append(std::string& str, const char* format, ...) {
	va_list va;
	va_start(va, format);
	int ret = vsprintf_append(str, format, va);
	va_end(va);
	return ret;
}

std::string warnings;

int printf_warn(const char* format, ...) {
	va_list va;
	va_start(va, format);
	int ret = vsprintf_append(warnings, format, va);
	va_end(va);
	return ret;
}

#define ENDL "\n" // TODO: Would CRLF be preferable?
#define SKIP_IF(statement, warning, ...) \
if (statement) \
{\
	printf_warn(warning, __VA_ARGS__); \
	printf_warn(ENDL); \
	continue; \
}
#define BREAK_IF(statement, warning, ...) \
if (statement) \
{\
	printf_warn(warning, __VA_ARGS__); \
	printf_warn(ENDL); \
	break; \
}

string g_current_game;
string g_current_section;

constexpr size_t NUM_LANGUAGES = 3;

enum class Language {
	Chinese,
	English,
	Japanese,
};

constexpr Language LANGUAGE_LIST[NUM_LANGUAGES] = {
	Language::Chinese,
	Language::English,
	Language::Japanese,
};

const char* const language_to_iso_639_1(Language language) {
	switch (language) {
		case Language::Chinese:
			return "zh";
		case Language::English:
			return "en";
		case Language::Japanese:
			return "ja";
		default:
			// NOTE: Should never execute.
			printf_warn(
				"Error: Attempt to convert invalid language to ISO 639-1"
			);

			// We need to default to SOMETHING that won't cause a naming
			// conflict, so just use Latin.
			return "la";
	}
}

struct loc_str_t {
	string zh_str;
	string en_str;
	string ja_str;

	loc_str_t() = default;
	loc_str_t(const char* zh, const char* en, const char* ja):
		zh_str(zh), en_str(en), ja_str(ja)
	{

	}

	string& get_language(Language language) {
		switch (language) {
			case Language::Chinese:
				return zh_str;
			case Language::English:
				return en_str;
			case Language::Japanese:
				return ja_str;
			default:
				// NOTE: This should never execute.
				printf_warn(
					"ERROR: Invalid language passed to loc_str_t.get_language()"
					" - defaulting to Chinese"
				);
				return zh_str;
		}
	}
};

constexpr size_t MAX_NUM_DIFFICULTIES = 4;

// NOTE: This isn't an `enum class`, because the Difficulty variants are used
// to directly index arrays.
enum Difficulty {
	DIFFICULTY_EASY = 0,
	DIFFICULTY_NORMAL = 1,
	DIFFICULTY_HARD = 2,
	DIFFICULTY_LUNATIC = 3,
};

constexpr Difficulty DIFFICULTY_LIST[MAX_NUM_DIFFICULTIES] = {
	DIFFICULTY_EASY,
	DIFFICULTY_NORMAL,
	DIFFICULTY_HARD,
	DIFFICULTY_LUNATIC,
};

struct section_t {
	int app_id{ 0 };
	int bgm_id{ 0 };
	int sec_id{ -1 };
	int chap_id{ -1 };
	int spell_id{ 0 };
	int appearance[NUM_LANGUAGES]{ -1, -1, -1 };

	string name;
	string ref;
	loc_str_t loc_str[MAX_NUM_DIFFICULTIES];

	bool FillWith(rapidjson::Value& sec);

	section_t() = default;
	section_t(const char* sec_name, rapidjson::Value& sec):
		name(sec_name)
	{
		FillWith(sec);
	}
};

struct game_t {
	string name;
	string namespace_;
	vector<section_t> sections;
	vector<pair<string, rapidjson::Value>> groups;

	static map<string, loc_str_t> glossary;
	static set<wchar_t> glyph_range_zh;
	static set<wchar_t> glyph_range_en;
	static set<wchar_t> glyph_range_ja;

	game_t() = default;

	static set<wchar_t>& get_glyph_range(Language language) {
		switch (language) {
			case Language::Chinese:
				return glyph_range_zh;
			case Language::English:
				return glyph_range_en;
			case Language::Japanese:
				return glyph_range_ja;
			default:
				// NOTE: This should never execute.
				printf_warn(
					"ERROR: Invalid language passed to game_t.get_glyph_range()"
					" - defaulting to Chinese"
				);
				return glyph_range_zh;
		}
	}
};
map<string, loc_str_t> game_t::glossary;
set<wchar_t> game_t::glyph_range_zh;
set<wchar_t> game_t::glyph_range_en;
set<wchar_t> game_t::glyph_range_ja;

void AddGlyphRange(loc_str_t& str) {
	static wstring u16str;
	static wstring_convert<codecvt_utf8<wchar_t>, wchar_t> conv;

	u16str = conv.from_bytes(str.zh_str);
	for (auto c : u16str) game_t::glyph_range_zh.insert(c);

	u16str = conv.from_bytes(str.en_str);
	for (auto c : u16str) game_t::glyph_range_en.insert(c);

	u16str = conv.from_bytes(str.ja_str);
	for (auto c : u16str) game_t::glyph_range_ja.insert(c);
}

bool ValidateGroup(rapidjson::GenericValue<rapidjson::UTF8<>>& value) {
	if (!value.IsArray()) return true;

	for (auto& sub_value : value.GetArray()) {
		if (sub_value.IsArray() && ValidateGroup(sub_value)) return false;
		else if (!sub_value.IsString()) return false;
	}

	return true;
}

void PrintGroupSize(std::string& output, rapidjson::Value& value) {
	vector<rapidjson::SizeType> result;
	function<void(rapidjson::Value&, unsigned int)> getSizeLimit = [&](
		rapidjson::Value& value, unsigned int dim
	) {
		if (value.IsArray()) {
			if (result.size() < dim + 1) result.resize(dim + 1);
			for (auto it = value.Begin(); it != value.End(); ++it)
				getSizeLimit(*it, dim + 1);
			if (result[dim] < value.Size()) result[dim] = value.Size();
		}
	};

	getSizeLimit(value, 0);
	if (result.size() > 0) result.back()++;
	for (auto dim : result)
		sprintf_append(output, "[%d]", dim);

}

void PrintGroup(std::string& output, rapidjson::Value& value, int tab = 0) {
	if (value.IsArray()) {
		for (int i = tab; i > 0; --i) output.push_back(' ');
		sprintf_append(output, "{" ENDL);
		for (auto v_itr = value.Begin(); v_itr != value.End(); ++v_itr)
			PrintGroup(output, *v_itr, tab + 4);
		for (int i = tab; i > 0; --i) output.push_back(' ');
		sprintf_append(output, "}%c" ENDL, !tab ? ';' : ',');
	} else {
		for (int i = tab; i > 0; --i) output.push_back(' ');
		if (!tab)
			sprintf_append(output, "= %s;" ENDL, value.GetString());
		else
			sprintf_append(output, "%s," ENDL, value.GetString());
	}
}

std::string EscapeString(std::string& str) {
	auto escaped_str = str;
	auto length = escaped_str.length();

	for (size_t i = 0; i < length; ++i) {
		if (escaped_str[i] == '\"') {
			escaped_str.insert(i, "\\");
			i++;
			length++;
		} else if (escaped_str[i] == '\\') {
			if (i + 1 != length && escaped_str[i + 1] != '0') {
				escaped_str.insert(i, "\\");
				i++;
				length++;
			}
		} else if (escaped_str[i] == '\n') {

			escaped_str.insert(i, "\\");
			i++;
			length++;
			escaped_str[i] = 'n';
		}
	}
	return escaped_str;
}

bool section_t::FillWith(rapidjson::Value& sec) {
	enum sec_switch {
		SW_BGM,
		SW_APPEARANCE,
		SW_SPELL,
		SW_REF,
	};

	static unordered_map<string, sec_switch> sec_switch_map
	{
		{"bgm", SW_BGM},
		{"appearance", SW_APPEARANCE},
		{"spell", SW_SPELL},
		{"ref", SW_REF},
	};

	for (auto sw_itr = sec.MemberBegin(); sw_itr != sec.MemberEnd(); ++sw_itr) {
		auto sw_key = sw_itr->name.GetString();
		auto& sw_value = sw_itr->value;

		if (sw_key[0] == '!') {
			loc_str_t lstr;
			if (
				sw_value.IsArray() &&
				sw_value.Size() == NUM_LANGUAGES &&
				sw_value[0].IsString() &&
				sw_value[1].IsString() &&
				sw_value[2].IsString()
			) {
				lstr = {
					sw_value[0].GetString(),
					sw_value[1].GetString(),
					sw_value[2].GetString()
				};
				AddGlyphRange(lstr);
			} else if (sw_value.IsString()) {
				auto it = game_t::glossary.find(sw_value.GetString());
				SKIP_IF(
					it == game_t::glossary.end(),
					"Warning: Reference not found: %s, ignoring.",
					sw_value.GetString()
				);
				lstr = it->second;
			} else {
				SKIP_IF(
					true,
					"Warning: Incorrect rank switch value: %s, ignoring.",
					sw_key
				);
			}

			for (size_t i = 1; i < strlen(sw_key); ++i) {
				auto error = false;
				switch (sw_key[i]) {
					case 'E':
						loc_str[DIFFICULTY_EASY] = lstr;
						break;
					case 'N':
						loc_str[DIFFICULTY_NORMAL] = lstr;
						break;
					case 'H':
						loc_str[DIFFICULTY_HARD] = lstr;
						break;
					case 'L':
						loc_str[DIFFICULTY_LUNATIC] = lstr;
						break;
					case 'X':
						for (auto difficulty : DIFFICULTY_LIST) {
							loc_str[difficulty] = lstr;
						}
						break;
					default:
						error = true;
						break;
				}
				BREAK_IF(
					error,
					"Warning: Incorrect rank switch: %s, ignoring.",
					sw_key
				);
			}
		} else {
			switch (sec_switch_map[sw_key]) {
				case SW_BGM:
					BREAK_IF(
						!sw_value.IsInt(),
						"Warning: Incorrect property switch: %s, ignoring.",
						sw_key
					);
					bgm_id = sw_value.GetInt();
					break;
				case SW_APPEARANCE:
					BREAK_IF(
						(
							!sw_value.IsArray() ||
							sw_value.Size() != NUM_LANGUAGES ||
							!sw_value[0].IsInt() ||
							!sw_value[1].IsInt() ||
							!sw_value[2].IsInt()
						),
						"Warning: Incorrect property switch: %s, ignoring.",
						sw_key
					);
					appearance[0] = sw_value[0].GetInt();
					appearance[1] = sw_value[1].GetInt();
					appearance[2] = sw_value[2].GetInt();
					break;
				case SW_SPELL:
					BREAK_IF(
						!sw_value.IsInt(),
						"Warning: Incorrect property switch: %s, ignoring.",
						sw_key
					);
					spell_id = sw_value.GetInt();
					break;
				case SW_REF:
					BREAK_IF(
						!sw_value.IsString(),
						"Warning: Incorrect property switch: %s, ignoring.",
						sw_key
					);
					ref = sw_value.GetString();
					break;
				default:
					BREAK_IF(
						true,
						"Warning: Incorrect property switch: %s, ignoring.",
						sw_key
					);
					break;
			}
		}
	}

	return true;
}

void write_autogenerated_warning(string& output) {
	sprintf_append(
		output,
		"// THIS FILE IS AUTOGENERATED" ENDL
	);
	sprintf_append(
		output,
		"// If you want to edit this file, edit games_def.json" ENDL
	);
	sprintf_append(
		output,
		"// Then, use the thprac devtools to regenerate this file" ENDL
	);
	sprintf_append(
		output,
		"// https://github.com/touhouworldcup/thprac_utils/" ENDL ENDL
	);
}

void write_glyph_range_declaration(
	string& output,
	set<wchar_t>& glyph_range,
	Language language
) {
	// Filter and count the glyph ranges
	size_t glyph_range_filtered_size = 0;
	for (auto code : glyph_range) {
		if (code > 0xFF) glyph_range_filtered_size += 2;
	}

	// Make room for ASCII range + null character
	glyph_range_filtered_size += 3;

	sprintf_append(
		output,
		"extern const wchar_t __thprac_loc_range_%s[%d];" ENDL ENDL,
		language_to_iso_639_1(language),
		glyph_range_filtered_size
	);
}

void write_glyph_range_definition(
	string& output,
	set<wchar_t>& glyph_range,
	Language language
) {
	// Do the glyph ranges first, so we can find how many there are
	size_t glyph_range_filtered_size = 0;
	string definition_body;
	for (auto code : glyph_range) {
		if (code <= 0xFF) continue;
		sprintf_append(definition_body, "    " "%#x, %#x," ENDL, code, code);
		glyph_range_filtered_size += 2;
	}

	// Make room for ASCII range + null character
	glyph_range_filtered_size += 3;

	sprintf_append(
		output,
		"const wchar_t __thprac_loc_range_%s[%d] {" ENDL,
		language_to_iso_639_1(language),
		glyph_range_filtered_size
	);
	sprintf_append(output, "    " "0x0020, 0x00FF," ENDL); // ASCII range
	sprintf_append(output, definition_body.c_str()); // Other ranges
	sprintf_append(output, "    0" ENDL); // Null character
	sprintf_append(output, "};" ENDL ENDL);
};

void generate_header_file(string& output, vector<game_t>& games) {
	// Header
	write_autogenerated_warning(output);
	sprintf_append(output, "#pragma once" ENDL);
	sprintf_append(output, "#include <cstdint>" ENDL ENDL);
	sprintf_append(output, "namespace THPrac {" ENDL ENDL);

	// Glossary enum
	if (game_t::glossary.size() < 256)
		sprintf_append(
			output,
			"enum th_glossary_t : uint8_t" ENDL
			"{" ENDL
			"    A0000ERROR_C," ENDL
		);
	else
		sprintf_append(
			output,
			"enum th_glossary_t" ENDL
			"{" ENDL
			"    A0000ERROR_C," ENDL
		);
	for (auto& glossary_entry : game_t::glossary)
		sprintf_append(output, "    %s," ENDL, glossary_entry.first.c_str());
	sprintf_append(output, "};" ENDL ENDL);

	// Glossary string declaration
	sprintf_append(
		output,
		"extern const char* th_glossary_str[%d][%d];" ENDL ENDL,
		NUM_LANGUAGES,
		game_t::glossary.size() + 1
	);

	// Per-"game" output
	// (NOTE: Not every entry is actually a game. However, every actual game
	// will have a non-empty namespace string.)
	for (auto& game : games) {
		bool has_namespace = game.namespace_.length() > 0;
		if (has_namespace) {
			// Namespace start
			sprintf_append(
				output,
				"namespace %s {" ENDL ENDL,
				game.namespace_.c_str()
			);

			// Sections enum
			if (game.sections.size() < 256)
				sprintf_append(
					output,
					"enum th_sections_t : uint8_t" ENDL
					"{" ENDL
					"    A0000ERROR," ENDL
				);
			else
				sprintf_append(
					output,
					"enum th_sections_t" ENDL
					"{" ENDL
					"    A0000ERROR," ENDL
				);
			for (auto& section : game.sections)
				sprintf_append(output, "    %s," ENDL, section.name.c_str());
			sprintf_append(output, "};" ENDL ENDL);

			// Sections string array declaration
			sprintf_append(
				output,
				"extern const char* th_sections_str[%d][%d][%d];" ENDL ENDL,
				NUM_LANGUAGES,
				MAX_NUM_DIFFICULTIES,
				game.sections.size() + 1
			);

			// Sections BGM id array declaration
			sprintf_append(
				output,
				"extern const uint8_t th_sections_bgm[%d];" ENDL ENDL,
				game.sections.size() + 1
			);

			// Sections by appearance - get array sizes
			int dimension_zero = 1;
			int dimension_one = 1;
			int dimension_two = 1;
			for (auto& section : game.sections) {
				if (section.appearance[0] > dimension_zero)
					dimension_zero = section.appearance[0];
				if (section.appearance[1] > dimension_one)
					dimension_one = section.appearance[1];
				if (section.appearance[2] > dimension_two)
					dimension_two = section.appearance[2];
			}

			// Sections by appearance - declaration
			sprintf_append(
				output,
				"extern const th_sections_t th_sections_cba[%d][%d][%d];" ENDL
				ENDL,
				dimension_zero, dimension_one, dimension_two + 1
			);

			// Sections by type - get array sizes
			// NOTE: This also builds a "cbt" array, which is only used here to
			// calculate the size of the declared array.
			// TODO: Remove the "cbt" vector, as it shouldn't be necessary here.
			constexpr size_t CBT_DIMENSION_ONE = 2;
			size_t cbt_dimension_two = 0;
			vector<vector<vector<string>>> cbt;
			cbt.resize(dimension_zero);
			for (auto& i1 : cbt)
				i1.resize(CBT_DIMENSION_ONE);
			for (auto& section : game.sections) {
				// TODO: Figure out what this index is actually doing.
				size_t unknown_index = section.spell_id ? 1 : 0;
				cbt[section.appearance[0] - 1][unknown_index].emplace_back(
					section.name
				);
				auto sss = cbt[section.appearance[0] - 1][unknown_index].size();
				if (sss > cbt_dimension_two) cbt_dimension_two = sss;
			}

			// Sections by type - declaration
			sprintf_append(
				output,
				"extern const th_sections_t th_sections_cbt[%d][%d][%d];" ENDL
				ENDL,
				dimension_zero, CBT_DIMENSION_ONE, cbt_dimension_two + 1
			);
		}

		// Groups array declarations
		for (auto& group : game.groups) {
			sprintf_append(
				output,
				"extern const th_glossary_t %s",
				group.first.c_str()
			);
			PrintGroupSize(output, group.second);
			sprintf_append(output, ";" ENDL ENDL);
		}


		// Namespace end
		if (has_namespace) sprintf_append(output, "}" ENDL ENDL);

	}

	// Glyph Range
	for (auto language : LANGUAGE_LIST) {
		write_glyph_range_declaration(
			output,
			game_t::get_glyph_range(language),
			language
		);
	}

	// `namespace THPrac` end
	sprintf_append(output, "}" ENDL);
	return;
}

void generate_source_file(string& output, vector<game_t>& games) {
	// Header
	write_autogenerated_warning(output);
	sprintf_append(output, "#include \"locale_def.h\"" ENDL ENDL);
	sprintf_append(output, "namespace THPrac {" ENDL ENDL);

	// Glossary string definition
	sprintf_append(
		output,
		"const char* th_glossary_str[%d][%d]" ENDL
		"{" ENDL,
		NUM_LANGUAGES,
		game_t::glossary.size() + 1
	);
	for (auto language : LANGUAGE_LIST) {
		sprintf_append(output, "    {" ENDL "        \"\"," ENDL);
		for (auto& glossary_entry : game_t::glossary)
			sprintf_append(
				output,
				"        \"%s\"," ENDL,
				EscapeString(
					glossary_entry.second.get_language(language)
				).c_str()
			);
		sprintf_append(output, "    }," ENDL);
	}
	sprintf_append(output, "};" ENDL ENDL);

	// Per-"game" output
	// (NOTE: Not every entry is actually a game. However, every actual game
	// will have a non-empty namespace string.)
	for (auto& game : games) {
		bool has_namespace = game.namespace_.length() > 0;
		if (has_namespace) {
			// Namespace start
			sprintf_append(
				output,
				"namespace %s {" ENDL ENDL,
				game.namespace_.c_str()
			);

			// Sections string array definition
			sprintf_append(
				output,
				"const char* th_sections_str[%d][%d][%d]" ENDL
				"{" ENDL,
				NUM_LANGUAGES,
				MAX_NUM_DIFFICULTIES,
				game.sections.size() + 1
			);
			for (auto language : LANGUAGE_LIST) {
				sprintf_append(output, "    {" ENDL);
				for (auto difficulty : DIFFICULTY_LIST) {
					sprintf_append(
						output,
						"        {" ENDL
						"            \"\"," ENDL
					);
					for (auto& section : game.sections) {
						string escaped_str = EscapeString(
							section.loc_str[difficulty].get_language(language)
						);
						sprintf_append(
							output,
							"            \"%s\"," ENDL,
							escaped_str.c_str()
						);
					}
					sprintf_append(output, "        }," ENDL);
				}
				sprintf_append(output, "    }," ENDL);
			}
			sprintf_append(output, "};" ENDL ENDL);

			// Sections BGM id array definition
			sprintf_append(
				output,
				"const uint8_t th_sections_bgm[%d]" ENDL
				"{" ENDL
				"    0," ENDL,
				game.sections.size() + 1
			);
			for (auto& section : game.sections)
				sprintf_append(output, "    %d," ENDL, section.bgm_id);
			sprintf_append(output, "};" ENDL ENDL);

			// Sections by appearance - get array sizes
			int dimension_zero = 1;
			int dimension_one = 1;
			int dimension_two = 1;
			for (auto& section : game.sections) {
				if (section.appearance[0] > dimension_zero)
					dimension_zero = section.appearance[0];
				if (section.appearance[1] > dimension_one)
					dimension_one = section.appearance[1];
				if (section.appearance[2] > dimension_two)
					dimension_two = section.appearance[2];
			}

			// Sections by appearance - definition
			vector<vector<vector<string>>> cba; // TODO: Find a better name.
			cba.resize(dimension_zero);
			for (auto& item1 : cba) {
				item1.resize(dimension_one);
				for (auto& item2 : item1)
					item2.resize(dimension_two);
			}
			for (auto& section : game.sections) {
				// NOTE: This code is indexing into three arrays.
				// TODO: It's also disgusting. Find a better way to do this.
				cba
					[section.appearance[0] - 1]
					[section.appearance[1] - 1]
					[section.appearance[2] - 1]
				= section.name;
			}
			// TODO: Since the "A0000ERROR" is never printed, is adding 1
			// to dimension_two correct?
			sprintf_append(
				output,
				"const th_sections_t th_sections_cba[%d][%d][%d]" ENDL
				"{" ENDL,
				dimension_zero, dimension_one, dimension_two + 1
			);
			// TODO: i1, i2, and i3 are terrible names. Plus, this nested loop
			// is terrible. Find a better way to do this. (Maybe a recursive
			// helper function?)
			for (auto& i1 : cba) {
				sprintf_append(output, "    {" ENDL);
				for (auto& i2 : i1) {
					sprintf_append(output, "        { ");
					for (auto& i3 : i2) {
						if (i3 == "") {
							// TODO: Why is this commented out? (See also the
							// TODO about adding 1 to dimension_two, above.)
							// sprintf_append(output, "A0000ERROR, ");
							break;
						} else
							sprintf_append(output, "%s, ", i3.c_str());
					}
					sprintf_append(output, "}," ENDL);
				}
				sprintf_append(output, "    }," ENDL);
			}
			sprintf_append(output, "};" ENDL ENDL);

			// Sections by type - get array sizes (and build "cbt" array)
			constexpr size_t CBT_DIMENSION_ONE = 2;
			size_t cbt_dimension_two = 0;
			vector<vector<vector<string>>> cbt; // TODO: Find a better name.
			cbt.resize(dimension_zero);
			for (auto& i1 : cbt)
				i1.resize(CBT_DIMENSION_ONE);
			for (auto& section : game.sections) {
				// TODO: Figure out what this index is actually doing.
				size_t unknown_index = section.spell_id ? 1 : 0;
				cbt[section.appearance[0] - 1][unknown_index].emplace_back(
					section.name
				);
				auto sss = cbt[section.appearance[0] - 1][unknown_index].size();
				if (sss > cbt_dimension_two) cbt_dimension_two = sss;
			}

			// Sections by type - definition
			sprintf_append(
				output,
				"const th_sections_t th_sections_cbt[%d][%d][%d]" ENDL
				"{" ENDL,
				dimension_zero, CBT_DIMENSION_ONE, cbt_dimension_two + 1
			);
			for (auto& i1 : cbt) {
				sprintf_append(output, "    {" ENDL);
				for (auto& i2 : i1) {
					sprintf_append(output, "        { ");
					for (auto& i3 : i2) {
						if (i3 == "")
							sprintf_append(output, "A0000ERROR, ");
						else
							sprintf_append(output, "%s, ", i3.c_str());
					}
					sprintf_append(output, "}," ENDL);
				}
				sprintf_append(output, "    }," ENDL);
			}
			sprintf_append(output, "};" ENDL ENDL);
		}


		// Groups array definitions
		for (auto& group : game.groups) {
			sprintf_append(
				output,
				"const th_glossary_t %s",
				group.first.c_str()
			);
			PrintGroupSize(output, group.second);
			sprintf_append(output, ENDL);
			PrintGroup(output, group.second);
			sprintf_append(output, ENDL);
		}

		// Namespace end
		if (has_namespace) sprintf_append(output, "}" ENDL ENDL);

	}

	// Glyph Range
	for (auto language : LANGUAGE_LIST) {
		write_glyph_range_definition(
			output,
			game_t::get_glyph_range(language),
			language
		);
	}

	// `namespace THPrac` end
	sprintf_append(output, "}" ENDL);
	return;
}

enum class CppFileType {
	Header,
	Source,
};

void loc_json(
	rapidjson::Document& doc,
	std::string& output,
	CppFileType file_type
) {

	// Iterate through games
	vector<game_t> games;
	for (
		auto game_itr = doc.MemberBegin();
		game_itr != doc.MemberEnd();
		++game_itr
		) {
		games.emplace_back();
		game_t& game_obj = games.back();
		game_obj.name = game_itr->name.GetString();
		g_current_game = game_itr->name.GetString();

		auto& game = game_itr->value;
		SKIP_IF(
			!game.IsObject(),
			"Warning: A non-object value for a game has detected, ignoring."
		);

		// Check namespace
		if (game.HasMember("namespace")) {
			if (game["namespace"].IsString()) {
				game_obj.namespace_ = game["namespace"].GetString();
			} else {
				printf_warn(
					"Warning: In game \"%s\": "
					"Invalid namespace value, ignoring." ENDL,
					g_current_game.c_str()
				);
			}
		}

		// Parsing Glossary
		if (game.HasMember("glossary")) {
			auto& glossary = game["glossary"];

			if (glossary.IsObject()) {
				for (
					auto item_itr = glossary.MemberBegin();
					item_itr != glossary.MemberEnd();
					++item_itr
					) {
					auto& item = item_itr->value;
					SKIP_IF(
						(
							!item.IsArray() ||
							item.Size() != NUM_LANGUAGES ||
							!item[0].IsString() ||
							!item[1].IsString() ||
							!item[2].IsString()
							),
						"Warning: In game \"%s\": Invalid glossary item: "
						"\"%s\", ignoring.",
						g_current_game.c_str(),
						item_itr->name.GetString()
					);

					auto glossary_key = item_itr->name.GetString();
					game_obj.glossary[glossary_key] = {
						item[0].GetString(),
						item[1].GetString(),
						item[2].GetString()
					};
					AddGlyphRange(game_obj.glossary[glossary_key]);
				}
			} else {
				printf_warn(
					"Warning: In game \"%s\": "
					"Invalid glossary value, ignoring." ENDL,
					g_current_game.c_str()
				);
			}
		}

		// Parsing sections
		if (game.HasMember("sections")) {
			auto& sections = game["sections"];

			if (sections.IsObject()) {
				// Iterate through sections
				for (
					auto section_itr = sections.MemberBegin();
					section_itr != sections.MemberEnd();
					++section_itr
					) {
					SKIP_IF(
						!section_itr->value.IsObject(),
						"Warning: In game \"%s\": Incorrect section: %s",
						g_current_game.c_str(),
						section_itr->name.GetString()
					);
					game_obj.sections.emplace_back(
						section_itr->name.GetString(),
						section_itr->value
					);
				}
			} else {
				printf_warn(
					"Warning: In game \"%s\": "
					"Invalid sections value, ignoring." ENDL,
					g_current_game.c_str()
				);
			}
		}

		// Parsing groups
		if (game.HasMember("groups")) {
			auto& groups = game["groups"];

			if (groups.IsObject()) {
				// Iterate through sections
				for (
					auto group_itr = groups.MemberBegin();
					group_itr != groups.MemberEnd();
					++group_itr
					) {
					// Validate group
					if (ValidateGroup(group_itr->value)) {
						game_obj.groups.emplace_back();
						game_obj.groups.back().first =
							group_itr->name.GetString();
						game_obj.groups.back().second = group_itr->value;
					} else {
						SKIP_IF(
							true,
							"Warning: In game \"%s\": Incorrect group: %s",
							g_current_game.c_str(),
							group_itr->name.GetString()
						);
					}
				}
			} else {
				printf_warn(
					"Warning: In game \"%s\": "
					"Invalid groups value, ignoring." ENDL,
					g_current_game.c_str()
				);
			}
		}

	}

	if (file_type == CppFileType::Header) {
		generate_header_file(output, games);
	} else {
		generate_source_file(output, games);
	}

	return;
}

#include <imgui.h>
#include <imgui_stdlib.h>

void loc_json_gui() {
	static std::string output_file_text = "";
	static const char* input_filename = NULL;
	if (ImGui::Button("Load input JSON")) {
		if (auto temp = OpenFileDialog(L"JSON file (*.json)\0*.json\0")) {
			if (input_filename)
				free((void*) input_filename);
			input_filename = temp;
		}
	}

	ImGui::SameLine();
	ImGui::TextUnformatted(input_filename ? input_filename : "");
	ImGui::NewLine();

	// NOTE: Modified on button click (see below)
	static CppFileType selected_file_type = CppFileType::Header;

	if (ImGui::Button("Generate header file")) {
		warnings = "";
		output_file_text = "";
		Document doc;
		MappedFile file(utf8_to_utf16(input_filename).c_str());
		if (file.fileMapView) {
			if (
				doc.Parse(
					(char*) file.fileMapView,
					file.fileSize
				).HasParseError()
			) {
				printf_warn(
					"Error: Parse error: %d at %d.",
					doc.GetParseError(),
					doc.GetErrorOffset()
				);
				goto after_generate;
			}

			selected_file_type = CppFileType::Header;
			loc_json(doc, output_file_text, selected_file_type);
		}
	}
	if (ImGui::Button("Generate source file")) {
		warnings = "";
		output_file_text = "";
		Document doc;
		MappedFile file(utf8_to_utf16(input_filename).c_str());
		if (file.fileMapView) {
			if (
				doc.Parse(
					(char*) file.fileMapView,
					file.fileSize
				).HasParseError()
			) {
				printf_warn(
					"Error: Parse error: %d at %d.",
					doc.GetParseError(),
					doc.GetErrorOffset()
				);
				goto after_generate;
			}

			selected_file_type = CppFileType::Source;
			loc_json(doc, output_file_text, selected_file_type);
		}
	}
after_generate:
	ImGui::NewLine();
	if (warnings != "") {
		ImGui::TextColored({ 1, 0, 0, 1 }, warnings.c_str());
		ImGui::NewLine();
	}

	if (ImGui::Button("Save")) {
		constexpr auto HEADER_FILE_NAME = L"locale_def.h";
		constexpr auto SOURCE_FILE_NAME = L"locale_def.cpp";

		wchar_t file_name[MAX_PATH];
		wcscpy_s(
			file_name,
			(selected_file_type == CppFileType::Header
				? HEADER_FILE_NAME
				: SOURCE_FILE_NAME)
		);
		LPCWSTR file_type_hint =
			selected_file_type == CppFileType::Header
			? L"C(++) Header File\0*.h\0"
			: L"C++ Source File\0*.cpp\0";

		OPENFILENAMEW ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = GuiGetWindow();
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = file_type_hint;
		ofn.nFilterIndex = 1;
		ofn.lpstrDefExt = L".h";
		ofn.Flags = OFN_OVERWRITEPROMPT;
		ofn.lpstrFile = file_name;
		ofn.nMaxFile = MAX_PATH;

		if (GetSaveFileNameW(&ofn)) {
			HANDLE hOut = CreateFileW(
				file_name,
				GENERIC_WRITE,
				NULL,
				NULL,
				OPEN_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL
			);

			SetFilePointer(hOut, 0, NULL, FILE_BEGIN);
			SetEndOfFile(hOut);

			DWORD byteRet;
			WriteFile(
				hOut,
				output_file_text.c_str(),
				output_file_text.size(),
				&byteRet,
				NULL
			);

			CloseHandle(hOut);
		}
	}

	ImGui::BeginChild(static_cast<unsigned int>(-69));
	ImVec2 wndSize = ImGui::GetWindowSize();
	ImGui::InputTextMultiline(
		"locDef",
		&output_file_text,
		{ wndSize.x, wndSize.y }
	);
	ImGui::EndChild();
}
