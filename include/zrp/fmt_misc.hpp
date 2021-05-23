// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

#include "zrp/args.hpp"

namespace zrp {

	fmt::text_style may(fmt::text_style s) {
		if (with_color) {
			return move(s);
		} else {
			return {};
		}
	}

};

