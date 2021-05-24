// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdlib>

#include "zrp/bindings.hpp"

namespace zrp {

namespace exceptions {

	struct bad_args: public exception {
		const char * what() const noexcept {
			return "Args given not valid";
		}
	};

}

enum class program_action_t {
	run,
	dump_config,
	help
};

static inline program_action_t program_action = program_action_t::help;
static inline string program_name = "";
static inline string config_file_path = "config.json";
static inline bool dump_full = false;
#ifdef _MSC_VER
static inline bool with_color = false; // avoid writing garbage to windows cmd
#else
static inline bool with_color = true;
#endif
static inline bool show_trace = false;
static inline bool show_debug = false;

inline void parse_env() {
#ifdef _MSC_VER
	if (std::getenv("ZRP_FORCECOLOR")) {
		with_color = false;
	}
#else
	if (std::getenv("ZRP_NOCOLOR")) {
		with_color = false;
	}
#endif
	if (std::getenv("ZRP_TRACE")) {
		show_trace = true;
	}
	if (std::getenv("ZRP_DEBUG")) {
		show_trace = true;
		show_debug = true;
	}
}

inline void parse_run_args(span<const std::string> args) {
	if (args.size() > 1) {
		throw exceptions::bad_args();
	} else if (args.size() == 1) {
		config_file_path = args[0];
	}
}

void parse_dump_config_args(span<const std::string> args) {
	for (auto& it : args) {
		if (it == "--full") {
			dump_full = true;
		} else {
			throw exceptions::bad_args();
		}
	}
}

inline void parse_args(span<const std::string> args)
{
	if (args.size() < 1) {
		throw exceptions::bad_args();
	}
	program_name = args[0];
	if (args.size() < 2) {
		throw exceptions::bad_args();
	}
	if (args[1] == "run") {
		program_action = program_action_t::run;
		parse_run_args(args.subspan(2));
	} else if (args[1] == "dump_config") {
		program_action = program_action_t::dump_config;
		parse_dump_config_args(args.subspan(2));
	} else if (args[1] == "help") {
		program_action = program_action_t::help;
	} else {
		throw exceptions::bad_args();
	}
}

inline void print_usage() noexcept {
	fmt::print(
			"usage : {} run/dump_config/help [...]\n"
			"\n"
			"    run [path/to/config.json]    run the program\n"
			"    dump_config [--full]         dump the example config\n"
			"    help                         show this message\n"
			"\n"
			"supported envs:\n"
#ifdef _MSC_VER
			"    ZRP_FORCECOLOR               output ANSI escape sequences even on windows\n"
#else
			"    ZRP_NOCOLOR                  not to add color or styling escape sequences\n"
#endif
			"    ZRP_TRACE                    show logs at level TRAC\n"
			"    ZRP_DEBUG                    show logs at level DEBG and TRAC\n"
			"\n",
			program_name);
}

};

