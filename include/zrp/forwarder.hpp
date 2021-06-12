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

	/**
	 * Pipes sockets between upstream & downstream, but sockets are
	 * initially created by downstream only.
	 */
	template <class Upstream, class Downstream>
		requires IsUpstream<Upstream> && IsDownstream<Downstream>
	struct forwarder : enable_shared_from_this<forwarder<Upstream, Downstream>> {
			asio::io_context & exec_;

			string name_;
			Upstream ups_;
			Downstream dow_;

			using pipe_t = pipe<Upstream, Downstream>;
			using pipe_ptr_t = shared_ptr<pipe_t>;
			using pipe_weak_ptr_t = weak_ptr<pipe_t>;

			map<int, pipe_weak_ptr_t> pipes_;
			int next_pipe_id_ = 0;

			bool stopping_ = false;
			log::logger logger_;

			int next_pipe_id() noexcept {
				return next_pipe_id_++;
			}

			forwarder(asio::io_context &exec, string name, Upstream ups, Downstream dow)
				: exec_(exec), name_(name), ups_(move(ups)), dow_(move(dow)), logger_(log::tag_forwarder(name)) {}

			static shared_ptr<forwarder<Upstream, Downstream>> create(asio::io_context &exec, string name, Upstream ups, Downstream dow) {
				return make_shared<forwarder<Upstream, Downstream>>(exec, move(name), move(ups), move(dow));
			}

			void try_stop() noexcept {
				stopping_ = true;
				if constexpr (IsTryStoppable<Downstream>) {
					dow_.try_stop();
				}
				if constexpr (IsTryStoppable<Upstream>) {
					ups_.try_stop();
				}
				for (auto& it : pipes_) {
					if (pipe_ptr_t p = it.second.lock()) {
						p->try_stop();
					}
				}
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
					try {
						co_await forward();
					} catch(...) {};
				}, asio::detached);
			}

			awaitable<void> forward() {
				auto sg = this->shared_from_this();
				try {
					for(;;) {
						tcp::endpoint ep;
						tcp::socket d_s = co_await dow_.get_socket(ep);
						co_spawn(exec_, [this, sg, d_s = move(d_s), ep]() mutable -> awaitable<void> {
							co_await handle_socket(move(d_s), ep);
						}, asio::detached);
					}
				} catch (const exception& e) {
					handle_error(e);
					throw;
				}
			}

			awaitable<void> handle_socket(tcp::socket s, const tcp::endpoint ep) {
				try {
					auto u_s = co_await ups_.get_socket(ep);
					int id = next_pipe_id();
					pipe_ptr_t p = pipe_t::create(exec_, this->shared_from_this(), id, move(s), move(u_s));
					p->run();
					pipes_.emplace(id, p);
				} catch (const exception& e) {
					logger_.warning("got an exception getting upstream socket, closing downstream socket : ").with_exception(e);
				}
			}
		};

}

