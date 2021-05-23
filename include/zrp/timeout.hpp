// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

#include "zrp/log.hpp"

namespace zrp {

template <class Cancellable>
struct with_timeout_t : enable_shared_from_this<with_timeout_t<Cancellable>> {
	asio::io_context& ioc_;
	asio::strand<asio::io_context::executor_type> str_;

	steady_timer ddl_;
	Cancellable &can_;

	with_timeout_t(asio::io_context& ioc, Cancellable &can)
		: ioc_(ioc), str_(ioc.get_executor()), ddl_(ioc), can_(can)
	{}

	static shared_ptr<with_timeout_t> create(asio::io_context& ioc, Cancellable &can) {
		return make_shared<with_timeout_t>(ioc, can);
	}

	template<class T>
	awaitable<T> run(awaitable<T>&& aw, chrono::seconds dur) {
		try {
			auto sg = this->shared_from_this();
			ddl_.expires_from_now(dur);
			co_spawn(str_, [this, sg] () mutable -> awaitable<void> {
					co_return co_await run_ddl();
					}, asio::detached);
			auto exec = co_await this_coro::executor;
			co_await asio::post(str_, asio::use_awaitable);
			if constexpr (is_void_v<T>) {
				co_await move(aw);
				ddl_.cancel();
				co_await asio::post(exec, asio::use_awaitable);
			} else {
				auto ret = co_await move(aw);
				ddl_.cancel();
				co_await asio::post(exec, asio::use_awaitable);
				co_return move(ret);
			}
		} catch (...) {
			try {
				ddl_.cancel();
			} catch (...) {}
			throw;
		}
	}

	awaitable<void> run_ddl() {
		try {
			try {
				co_await ddl_.async_wait(asio::use_awaitable);
				can_.cancel();
			} catch (system_error & se) {
				if (se.code() == asio::error::operation_aborted) {
					co_return;
				}
				throw;
			}
		} catch (const exception& e) {
			log::as(log::tag_timeout{}).warning("ddl timer got error : ").with_exception(e);
		}
	}
};

template <class T, class Cancellable>
awaitable<T> with_timeout(asio::io_context &ioc, awaitable<T>&& aw, Cancellable &can, chrono::seconds dur) {
	auto wt_ptr = with_timeout_t<Cancellable>::create(ioc, can);
	if constexpr (is_void_v<T>) {
		co_await wt_ptr->run(move(aw), dur);
	} else {
		co_return co_await wt_ptr->run(move(aw), dur);
	}
}

};

