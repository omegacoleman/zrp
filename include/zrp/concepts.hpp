// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

namespace zrp {

// polyfill
namespace detail {
template <class T, class U> concept SameHelper = std::is_same_v<T, U>;
}

template <class T, class U>
concept same_as = detail::SameHelper<T, U> &&detail::SameHelper<U, T>;

template <class T>
	concept IsUpstream = requires(T a) {
		{ a.get_socket() } -> same_as<awaitable<tcp::socket>>;
	};

template <class T>
	concept IsDownstream = requires(T a) {
		{ a.get_socket() } -> same_as<awaitable<tcp::socket>>;
	};

template <class T>
	concept IsTryStoppable = requires(T a) {
		{ a.try_stop() };
	};

}

