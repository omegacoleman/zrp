// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

namespace zrp {

namespace exceptions {

	struct duplicate_client : public exception {
		duplicate_client() {}

		const char * what() const noexcept {
			return "same client already connected";
		}
	};

	struct duplicate_tcp_share : public exception {
		string msg_;

		duplicate_tcp_share(string id)
			: msg_(fmt::format(FMT_COMPILE("tcp share id is taken : {}"), id))
			{}

		const char * what() const noexcept {
			return msg_.c_str();
		}
	};

	struct tcp_share_closed: public exception {
		tcp_share_closed() {}

		const char * what() const noexcept {
			return "tcp share already closed";
		}
	};

}

}

