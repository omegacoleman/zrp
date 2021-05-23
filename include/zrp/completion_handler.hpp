// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

namespace zrp {

	template <class ...Args>
	struct completion_handler_erasure {
		virtual void operator()(Args... args)=0;
		virtual ~completion_handler_erasure(){}
	};

	template <class CompletionHandler, class ...Args>
		// requires CallableWithArgs(CompletionHandler, ...Args)
	struct completion_handler_t final : completion_handler_erasure<Args...> {
		CompletionHandler h_;

		completion_handler_t(CompletionHandler h) : h_(move(h)) {}

		void operator()(Args... args) {
			auto exec = asio::get_associated_executor(h_);
			asio::post(exec, [...args = forward<Args>(args), h_(move(h_))]() mutable
			{
				h_(forward<Args>(args)...);
			});
		}
	};

	template <class Signature> struct completion_handler;

	template <class ...Args>
	struct completion_handler<void(Args...)> {

		unique_ptr<completion_handler_erasure<Args...>> ptr_;

		void operator()(Args... args) {
			(*ptr_)(forward<Args>(args)...);
		}

		template <class CompletionHandler>
			// requires CallableWithArgs(CompletionHandler, ...Args)
		completion_handler(CompletionHandler h)
		: ptr_(make_unique<completion_handler_t<CompletionHandler, Args...>>(move(h))) {}

		completion_handler() {}
		completion_handler(completion_handler&&)=default;
		completion_handler& operator=(completion_handler&&)=default;

	};

}

