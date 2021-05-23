// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

#include "zrp/log.hpp"

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))

#include <sys/time.h>
#include <sys/resource.h>

namespace zrp {

inline void try_set_rlimit_nofile(int configured_rlim_nofile) {
	auto logger = zrp::log::as(zrp::log::tag_main{});
	rlimit r_nofile;
	r_nofile.rlim_cur = static_cast<rlim_t>(configured_rlim_nofile);
	r_nofile.rlim_max = static_cast<rlim_t>(configured_rlim_nofile);
	int ret = setrlimit(RLIMIT_NOFILE, &r_nofile);
	if (ret < 0) {
		logger.warning("setting rlimit failed with error : ").with_exception(system_error{make_error_code(static_cast<errc::errc_t>(errno))});
	} else {
		logger.trace(fmt::format(FMT_COMPILE("setting RLIMIT_NOFILE soft & hard limit to {}"), configured_rlim_nofile));
	}
}

}

#else

namespace zrp {

inline void try_set_rlimit_nofile(int configured_rlim_nofile) {
	auto logger = zrp::log::as(zrp::log::tag_main{});
	logger.trace("skipping rlimit_nofile as in a non-posix system");
}

}

#endif

