// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

#include "zrp/config.hpp"

namespace zrp {

template <class Config>
void general_dump_config(json::value jv, bool dump_full) {
	if (dump_full) {
		Config cfg = json::value_to<Config>(jv);
		pretty_print(json::value_from(cfg));
	} else {
		pretty_print(jv);
	}
}

namespace server {
	inline void dump_example_config(bool dump_full) {
		general_dump_config<config_t>(example_config(), dump_full);
	}
}

namespace client {
	inline void dump_example_config(bool dump_full) {
		general_dump_config<config_t>(example_config(), dump_full);
	}
}

}
