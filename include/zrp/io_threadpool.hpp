// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

namespace zrp {

	struct io_threadpool : asio::io_context {
		vector<thread> pool_;

		void start_in_parallel(size_t thread_count, function<void(int)> on_started) {
			for (int i = 0; i < thread_count; i++) {
				pool_.emplace_back([this, i, on_started]() mutable {
					on_started(i);
					asio::io_context::run();
				});
			}
		}

		void start_in_parallel(size_t thread_count) {
			start_in_parallel(thread_count, [](int){});
		}

		void join_all() {
			for (auto& it : pool_) {
				it.join();
			}
			pool_.clear();
		}

		void stop_and_join() {
			asio::io_context::stop();
			join_all();
		}
	};

}

