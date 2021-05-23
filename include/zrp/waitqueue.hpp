// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

#include "zrp/completion_handler.hpp"

namespace zrp {

template <class R>
struct waitqueue {
	using waiter_completion_handler_t = completion_handler<void(error_code, optional<R>)>;
	using provider_completion_handler_t = completion_handler<void(error_code)>;

	deque<waiter_completion_handler_t> q_;
	deque<tuple<provider_completion_handler_t, R>> vq_;

	asio::executor exec_;
	bool closed_{false};

	waitqueue(asio::executor exec)
	: exec_(exec)
	{}

	awaitable<R> wait() {
		auto initiation = [this](auto&& handler) mutable
		{
			waiter_completion_handler_t curr_handler = forward<decltype(handler)>(handler);
			asio::post(exec_, [this, curr_handler = move(curr_handler)]() mutable {
				if (closed_) {
					curr_handler(asio::error::operation_aborted, {});
					return;
				}
				if (!vq_.empty()) {
					auto tup = move(vq_.front());
					provider_completion_handler_t h{move(std::get<0>(tup))};
					R r{move(std::get<1>(tup))};
					vq_.pop_front();
					curr_handler({}, move(r));
					h({});
				} else {
					q_.emplace_back(move(curr_handler));
				}
			});
		};
		auto opt = co_await asio::async_initiate<decltype(asio::use_awaitable), void(error_code, optional<R>)>(initiation, asio::use_awaitable);
		co_return move(opt).value();
	}

	awaitable<void> provide(R r) {
		shared_ptr<R> p_r = make_shared<R>(move(r));
		auto initiation = [this, p_r](auto&& handler) mutable
		{
			provider_completion_handler_t curr_handler = forward<decltype(handler)>(handler);
			asio::post(exec_, [this, curr_handler = move(curr_handler), p_r]() mutable {
				if (closed_) {
					curr_handler(asio::error::operation_aborted);
					return;
				}
				if (!q_.empty()) {
					auto h = move(q_.front());
					q_.pop_front();
					h({}, move(*p_r));
					curr_handler({});
				} else {
					vq_.emplace_back(make_tuple<provider_completion_handler_t, R>(move(curr_handler), move(*p_r)));
				}
			});
		};
		return asio::async_initiate<decltype(asio::use_awaitable), void(error_code)>(initiation, asio::use_awaitable);
	}

	void close() {
		closed_ = true;
		asio::post(exec_, [this]() mutable {
			for (auto& it : q_) {
				it(asio::error::operation_aborted, {});
			}
			q_.clear();
			for (auto& it : vq_) {
				std::get<0>(it)(asio::error::operation_aborted);
			}
			vq_.clear();
		});
	}
};

};


