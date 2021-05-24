// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdio>
#include <atomic>
#include <deque>
#include <chrono>
#include <variant>
#include <tuple>
#include <functional>
#include <utility>
#include <string>
#include <string_view>
#include <optional>
#include <memory>
#include <type_traits>
#include <span>
#include <bit>
#include <stack>

#include "boost/asio.hpp"
#include "boost/json.hpp"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/uuid_generators.hpp"
#include "boost/uuid/uuid_io.hpp"

#include "fmt/core.h"
#include "fmt/color.h"
#include "fmt/chrono.h"
#include "fmt/compile.h"

namespace zrp {

namespace json = boost::json;
namespace uuids = boost::uuids;
using std::atomic;
using std::span;
using std::endian;
using std::decay_t;
using std::unique_ptr;
using std::shared_ptr;
using std::weak_ptr;
using std::make_shared;
using std::enable_shared_from_this;
using std::make_unique;
using boost::asio::buffer;
using boost::asio::async_read;
using boost::asio::async_write;
using std::move;
using std::forward;
using std::function;
using std::exception;
using boost::system::system_error;
using boost::system::error_code;
namespace errc = boost::system::errc;
using boost::asio::executor;
using boost::asio::ip::tcp;
using std::optional;
using boost::asio::awaitable;
using std::string;
using std::string_view;
using std::ostringstream;
using boost::asio::co_spawn;
using boost::asio::steady_timer;
using std::deque;
using std::tuple;
using std::make_tuple;
using std::is_void_v;
using std::array;
using std::vector;
using std::variant;
using std::map;
using std::stack;
using std::exception_ptr;
namespace chrono = std::chrono;
namespace asio = boost::asio;
namespace this_coro = boost::asio::this_coro;

}


