// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

#include "zrp/file.hpp"
#include "zrp/fmt_misc.hpp"

namespace zrp {

template<class T>
void extract( json::object const& obj, T& t, const string& key ) {
	t = value_to<T>( obj.at( key ) );
}

template<class T, class InputType>
    // requires ConvertibleTo<InputType, T>
void extract_with_default(json::object const& obj, T& t, const string& key, const InputType& default_value) {
	if (auto ptr = obj.if_contains(key)) {
		t = value_to<T>(*ptr);
	} else {
		t = default_value;
	}
}

auto inline brace_color(int indent_level) {
	switch (indent_level % 5) {
		case 0:
			return fmt::color::dark_cyan;
		case 1:
			return fmt::color::dark_green;
		case 2:
			return fmt::color::dark_olive_green;
		case 3:
			return fmt::color::dark_blue;
		case 4:
			return fmt::color::dark_salmon;
	}
	return fmt::color::dark_cyan;
}

struct indent_t {
	stack<string> s_;

	int level() const {
		return s_.size();
	}

	string str() const {
		if (s_.empty()) {
			return "";
		}
		return s_.top();
	}

	string brace(const string& br) const {
		return fmt::format(may(fmt::fg(brace_color(level()))), "{}", br);
	}

	void push() {
		s_.push(str() + "    ");
	}

	void pop() {
		s_.pop();
	}
};

inline void pretty_print(const json::value& jv, indent_t indent = {})
{
	switch(jv.kind())
	{
		case json::kind::object:
			{
				fmt::print("{}\n", indent.brace("{"), "\n");
				indent.push();
				auto const& obj = jv.get_object();
				if(! obj.empty())
				{
					auto it = obj.begin();
					for(;;)
					{
						fmt::print("{}", indent.str());
						fmt::print(may(fmt::fg(fmt::terminal_color::blue) | fmt::emphasis::bold), "{}", json::serialize(it->key()));
						fmt::print(" : ");
						pretty_print(it->value(), indent);
						if(++it == obj.end())
							break;
						fmt::print(",\n");
					}
				}
				indent.pop();
				fmt::print("\n{}{}", indent.str(), indent.brace("}"));
				break;
			}

		case json::kind::array:
			{
				fmt::print("{}\n", indent.brace("["), "\n");
				indent.push();
				auto const& arr = jv.get_array();
				if(! arr.empty())
				{
					auto it = arr.begin();
					for(;;)
					{
						fmt::print("{}", indent.str());
						pretty_print(*it, indent);
						if(++it == arr.end())
							break;
						fmt::print(",\n");
					}
				}
				indent.pop();
				fmt::print("\n{}{}", indent.str(), indent.brace("]"));
				break;
			}

		case json::kind::string:
			{
				fmt::print(may(fmt::fg(fmt::terminal_color::green)), "{}", json::serialize(jv.get_string()));
				break;
			}

		case json::kind::uint64:
			fmt::print("{}", jv.get_uint64());
			break;

		case json::kind::int64:
			fmt::print("{}", jv.get_int64());
			break;

		case json::kind::double_:
			fmt::print("{}", jv.get_double());
			break;

		case json::kind::bool_:
			if(jv.get_bool())
				fmt::print(may(fmt::emphasis::italic), "true");
			else
				fmt::print(may(fmt::emphasis::italic), "false");
			break;

		case json::kind::null:
			fmt::print(may(fmt::emphasis::strikethrough), "null");
			break;
	}

	if(indent.level() == 0)
		fmt::print("\n");
}

inline json::value parse_file(const string_view filename)
{
	string filename_s{filename};
	file f(filename_s.c_str(), "r");
	json::stream_parser p;
	while (!f.eof())
	{
		char buf[4096];
		auto const nread = f.read(buf, sizeof(buf));
		p.write(buf, nread);
	}
	p.finish();
	return p.release();
}

};

