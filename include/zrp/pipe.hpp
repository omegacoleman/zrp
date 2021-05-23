// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

#include "zrp/concepts.hpp"
#include "zrp/completion_handler.hpp"
#include "zrp/log.hpp"

namespace zrp {

	const size_t pipe_buffer_size = 8192;

	template <class Upstream, class Downstream>
		requires IsUpstream<Upstream> && IsDownstream<Downstream>
	struct forwarder;

	template <class Upstream, class Downstream>
		requires IsUpstream<Upstream> && IsDownstream<Downstream>
	struct pipe : enable_shared_from_this<pipe<Upstream, Downstream>> {
		using forwarder_ptr_t = shared_ptr<forwarder<Upstream, Downstream>>;

		asio::io_context &exec_;
		int id_;
		tcp::socket lhs_s_;
		tcp::socket rhs_s_;
		forwarder_ptr_t fwd_;
		bool stopping_ = false;
		log::logger logger_;

		pipe(asio::io_context &exec, forwarder_ptr_t fwd, int id, tcp::socket lhs_s, tcp::socket rhs_s)
			: exec_(exec), fwd_(fwd), id_(id), lhs_s_(move(lhs_s)), rhs_s_(move(rhs_s)), logger_(log::tag_pipe{fwd->name_, id}) {}

		static shared_ptr<pipe<Upstream, Downstream>> create(asio::io_context &exec, forwarder_ptr_t fwd, int id, tcp::socket lhs_s, tcp::socket rhs_s)
		{
			return make_shared<pipe<Upstream, Downstream>>(exec, fwd, id, move(lhs_s), move(rhs_s));
		}

		void try_stop() noexcept {
			stopping_ = true;
			try {
				if (lhs_s_.is_open())
					lhs_s_.close();
			} catch (...) {}
			try {
				if (rhs_s_.is_open())
					rhs_s_.close();
			} catch (...) {}
		}

		void handle_error(const exception& e) noexcept {
			if (!stopping_) {
				logger_.error("got an exception, stopping : ").with_exception(e);
				try_stop();
			} else {
				logger_.trace("exited by exception : ").with_exception(e);
			}
		}

		void run() {
			auto sg = this->shared_from_this();
			co_spawn(exec_, [this, sg]() mutable -> awaitable<void> {
				co_await half_pipe(lhs_s_, rhs_s_);
			}, asio::detached);
			co_spawn(exec_, [this, sg]() mutable -> awaitable<void> {
				co_await half_pipe(rhs_s_, lhs_s_);
			}, asio::detached);
		}

		awaitable<void> half_pipe(tcp::socket &read_s, tcp::socket &write_s) {
			try {
				try {
					char data[pipe_buffer_size];
					for (;;) {
						size_t n = co_await read_s.async_read_some(buffer(data, pipe_buffer_size), asio::use_awaitable);
						co_await async_write(write_s, buffer(data, n), asio::use_awaitable);
					}
				} catch (system_error & se) {
					if ((se.code() != asio::error::not_connected) &&
						(se.code() != asio::error::eof) &&
						(se.code() != asio::error::connection_reset)) {
						throw;
					}
					try {
						write_s.shutdown(tcp::socket::shutdown_send);
					} catch(...) {}
				}
			} catch (const exception& e) {
				handle_error(e);
			}
		}
	};
}

