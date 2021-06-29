// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

#include "zrp/concepts.hpp"
#include "zrp/pipe.hpp"
#include "zrp/log.hpp"

namespace zrp
{

	inline tcp::socket rebind_ioc(asio::io_context& ioc, tcp::socket&& s) {
		tcp::socket ret{ioc};
		auto proto = s.local_endpoint().protocol();
		ret.assign(proto, s.release());
		return move(ret);
	}

	/**
	 * Pipes sockets between upstream & downstream, but sockets are
	 * initially created by downstream only.
	 */
	template <class Upstream, class Downstream>
		requires IsUpstream<Upstream> && IsDownstream<Downstream>
	struct forwarder : enable_shared_from_this<forwarder<Upstream, Downstream>> {
			asio::io_context & ioc_;

			string name_;
			Upstream ups_;
			Downstream dow_;

			using pipe_t = pipe<Upstream, Downstream>;
			using pipe_ptr_t = shared_ptr<pipe_t>;
			using pipe_weak_ptr_t = weak_ptr<pipe_t>;

			asio::strand<asio::io_context::executor_type> str_pipes_;
			map<int, pipe_weak_ptr_t> pipes_;
			int next_pipe_id_ = 0;

			bool stopping_ = false;
			log::logger logger_;

			int next_pipe_id() noexcept {
				return next_pipe_id_++;
			}

			forwarder(asio::io_context &ioc, string name, Upstream ups, Downstream dow)
				: ioc_(ioc), name_(name), ups_(move(ups)), dow_(move(dow)), logger_(log::tag_forwarder(name)), str_pipes_(asio::make_strand(ioc)) {}

			static shared_ptr<forwarder<Upstream, Downstream>> create(asio::io_context &ioc, string name, Upstream ups, Downstream dow) {
				return make_shared<forwarder<Upstream, Downstream>>(ioc, move(name), move(ups), move(dow));
			}

			void post_try_stop() noexcept {
				auto sg = this->shared_from_this();
				co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
					try {
						co_await try_stop();
					} catch(...) {};
				}, asio::detached);
			}

			awaitable<void> try_stop() noexcept {
				stopping_ = true;
				if constexpr (IsTryStoppable<Downstream>) {
					dow_.try_stop();
				}
				if constexpr (IsTryStoppable<Upstream>) {
					ups_.try_stop();
				}

				auto exec = co_await this_coro::executor;
				co_await asio::post(str_pipes_, asio::use_awaitable);
				for (auto& it : pipes_) {
					if (pipe_ptr_t p = it.second.lock()) {
						p->try_stop();
					}
				}
				co_await asio::post(exec, asio::use_awaitable);
			}

			awaitable<void> handle_error(const exception_ptr eptr) noexcept {
				if (!stopping_) {
					try {
						std::rethrow_exception(eptr);
					} catch(const exception& e) {
						logger_.error("got an exception, stopping : ").with_exception(e);
					}
					co_await try_stop();
				} else {
					try {
						std::rethrow_exception(eptr);
					} catch(const exception& e) {
						logger_.trace("exited by exception : ").with_exception(e);
					}
				}
			}

			void run() {
				auto sg = this->shared_from_this();
				co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
					try {
						co_await forward();
					} catch(...) {};
				}, asio::detached);
			}

			awaitable<void> forward() {
				auto sg = this->shared_from_this();
				exception_ptr eptr;
				try {
					for(;;) {
						tcp::endpoint ep;
						tcp::socket d_s = rebind_ioc(ioc_, co_await dow_.get_socket(ep));
						co_spawn(ioc_, [this, sg, d_s = move(d_s), ep]() mutable -> awaitable<void> {
							co_await handle_socket(move(d_s), ep);
						}, asio::detached);
					}
				} catch (const exception e) { // clang prohibits co_await inside catch block
					eptr = std::current_exception();
				}
				if (eptr) {
					co_await handle_error(eptr);
					std::rethrow_exception(eptr);
				}
			}

			awaitable<void> handle_socket(tcp::socket s, const tcp::endpoint ep) {
				try {
					auto u_s = rebind_ioc(ioc_, co_await ups_.get_socket(ep));

					auto exec = co_await this_coro::executor;
					co_await asio::post(str_pipes_, asio::use_awaitable);
					int id = next_pipe_id();
					pipe_ptr_t p = pipe_t::create(ioc_, this->shared_from_this(), id, move(s), move(u_s));
					pipes_.emplace(id, p);
					co_await asio::post(exec, asio::use_awaitable);

					p->run();
				} catch (const exception& e) {
					logger_.warning("got an exception getting upstream socket, closing downstream socket : ").with_exception(e);
				}
			}
		};

}

