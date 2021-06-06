// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

#include "zrp/config.hpp"
#include "zrp/json_misc.hpp"
#include "zrp/forwarder.hpp"
#include "zrp/waitqueue.hpp"
#include "zrp/msg.hpp"
#include "zrp/log.hpp"
#include "zrp/rlimit.hpp"

namespace zrp {

namespace client {

static inline config_t cfg;

inline void load_config(const string_view filename) {
	static json::value jv = parse_file(filename);
	cfg = json::value_to<config_t>(jv);

	log::as(log::tag_client{}).info("Loaded config : ");
	pretty_print(json::value_from(cfg));

	try_set_rlimit_nofile(cfg.rlimit_nofile);
}


extern int exit_code;

struct tcp_share;
struct tcp_share_worker;
struct controller;

using ctrl_t = controller;
using ctrl_ptr_t = shared_ptr<ctrl_t>;
using tcp_share_worker_ptr_t = shared_ptr<tcp_share_worker>;
using tcp_share_worker_weak_ptr_t = weak_ptr<tcp_share_worker>;
using tcp_share_ptr_t = shared_ptr<tcp_share>;
using tcp_share_weak_ptr_t = weak_ptr<tcp_share>;

struct tcp_share : enable_shared_from_this<tcp_share> {
	asio::io_context &ioc_;
	const string share_id_;
	const tcp::endpoint ep_;
	waitqueue<tcp::socket> wq_;
	unsigned short port_;
	ctrl_ptr_t ctrl_;
	map<int, tcp_share_worker_weak_ptr_t> workers_{};
	int next_worker_id_ = 0;
	atomic<int> nr_workers_ = 0;
	bool closing_ = false;
	log::logger logger_;

	struct upstream {
		shared_ptr<tcp_share> sh_;

		upstream(shared_ptr<tcp_share> sh) noexcept;
		awaitable<tcp::socket> get_socket();
	};

	struct downstream {
		shared_ptr<tcp_share> sh_;

		downstream(shared_ptr<tcp_share> sh) noexcept;
		awaitable<tcp::socket> get_socket();
	};

	using forwarder_t = forwarder<upstream, downstream>;
	using forwarder_ptr_t = shared_ptr<forwarder_t>;
	using forwarder_weak_ptr_t = weak_ptr<forwarder_t>;
	forwarder_weak_ptr_t fwd_;

	tcp_share(asio::io_context &ioc, ctrl_ptr_t ctrl, string share_id, tcp::endpoint ep, unsigned short port);
	static shared_ptr<tcp_share> create(asio::io_context &ioc, ctrl_ptr_t ctrl, string share_id, tcp::endpoint ep, unsigned short port);

	void try_stop() noexcept;
	void handle_error(const exception& e) noexcept;

	int next_worker_id();

	upstream make_upstream() noexcept;
	downstream make_downstream() noexcept;

	void run();
	awaitable<void> run_forwarder();

	awaitable<void> add_worker();
	void cleanup_workers();
	void chk_need_workers();
	awaitable<void> chk_need_workers_coro();
	awaitable<void> add_workers(size_t count);
};

struct controller : enable_shared_from_this<controller> {
	asio::io_context &ioc_;
	tcp::endpoint ep_;
	tcp::socket s_;
	string client_uuid_;
	map<string, tcp_share_weak_ptr_t> tcp_shares_;
	msg::client_hello hello_;
	bool stopping_ = false;
	log::logger logger_;
	steady_timer ping_timer_;

	controller(asio::io_context &ioc);
	static shared_ptr<controller> create(asio::io_context &ioc);
	void init();

	void add_tcp_share(string share_id, tcp::endpoint ep, unsigned short port);

	void try_stop() noexcept;
	void handle_error(const exception& e) noexcept;

	void run();
	awaitable<void> controller_socket_send_recv_msgs();
	void set_ping_timer(chrono::seconds after);
	awaitable<void> ping_actor();

	awaitable<tcp::socket> get_socket();

	awaitable<void> handle_msg(msg::server_hello m);
	awaitable<void> handle_msg(msg::pong);
};

struct tcp_share_worker : enable_shared_from_this<tcp_share_worker> {
	asio::io_context &ioc_;
	tcp::socket s_;
	tcp_share_ptr_t share_;
	string share_id_;
	int worker_id_;
	bool visited_ = false;
	bool stopping_ = false;
	log::logger logger_;
	steady_timer ping_timer_;

	tcp_share_worker(asio::io_context &ioc, tcp_share_ptr_t share, tcp::socket s, string share_id, int worker_id);
	~tcp_share_worker();
	static shared_ptr<tcp_share_worker> create(asio::io_context &ioc, tcp_share_ptr_t share, tcp::socket s, string share_id, int worker_id);

	void try_stop();
	void handle_error(const exception& e);

	void run();
	awaitable<void> send_and_recv_msgs();
	void set_ping_timer(chrono::seconds after);
	awaitable<void> ping_actor();

	awaitable<void> handle_msg(msg::visit_tcp_share);
	awaitable<void> handle_msg(msg::pong);
};

inline tcp_share::upstream::upstream(shared_ptr<tcp_share> sh) noexcept : sh_(sh) {}

inline awaitable<tcp::socket> tcp_share::upstream::get_socket() {
	tcp::socket ret{sh_->ioc_};
	co_await ret.async_connect(sh_->ep_, asio::use_awaitable);
	co_return move(ret);
}

inline tcp_share::downstream::downstream(shared_ptr<tcp_share> sh) noexcept : sh_(sh) {}

inline awaitable<tcp::socket> tcp_share::downstream::get_socket() {
    auto ret = co_await sh_->wq_.wait();
    sh_->chk_need_workers();
    co_return move(ret);
}

inline tcp_share::tcp_share(asio::io_context &ioc, ctrl_ptr_t ctrl, string share_id, tcp::endpoint ep, unsigned short port)
	: ioc_(ioc), ctrl_(ctrl), share_id_(share_id), ep_(move(ep)), port_(port), wq_(ioc.get_executor()), logger_(log::tag_tcp_share{share_id})
{}

inline shared_ptr<tcp_share> tcp_share::create(asio::io_context &ioc, ctrl_ptr_t ctrl, string share_id, tcp::endpoint ep, unsigned short port) {
	return make_shared<tcp_share>(ioc, ctrl, move(share_id), move(ep), port);
}

inline void tcp_share::try_stop() noexcept {
	closing_ = true;
	if (forwarder_ptr_t ptr = fwd_.lock()) {
		ptr->try_stop();
	}
	wq_.close();
	for (auto& it : workers_) {
		if (tcp_share_worker_ptr_t w = it.second.lock()) { // if not already stopped
			w->try_stop();
		}
	}
}

inline void tcp_share::handle_error(const exception& e) noexcept {
	if (!closing_) {
		exit_code = 1;
		logger_.error("got an exception, stopping whole client : ").with_exception(e);
		ctrl_->try_stop(); // stop the whole ctrl when tcp share got an error
	} else {
		logger_.trace("exited by exception : ").with_exception(e);
	}
}

inline int tcp_share::next_worker_id() {
	return next_worker_id_++;
}

inline tcp_share::upstream tcp_share::make_upstream() noexcept {
	return {this->shared_from_this()};
}

inline tcp_share::downstream tcp_share::make_downstream() noexcept {
	return {this->shared_from_this()};
}

inline void tcp_share::run() {
	auto sg = this->shared_from_this();
	co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
		co_await run_forwarder();
	}, asio::detached);
}

inline awaitable<void> tcp_share::run_forwarder() {
	try {
		forwarder_ptr_t fwd = forwarder_t::create(ioc_, share_id_, make_upstream(), make_downstream());
		fwd_ = fwd;
		co_await fwd->forward();
	} catch (const exception& e) {
		handle_error(e);
	}
}

inline awaitable<void> tcp_share::add_worker() {
	tcp::socket s = co_await ctrl_->get_socket(); // establish a new connection to server ctrl port
	int worker_id = next_worker_id();
	tcp_share_worker_ptr_t w = tcp_share_worker::create(ioc_, this->shared_from_this(), move(s), share_id_, worker_id);
	w->run();
	workers_.emplace(worker_id, w);
}

inline void tcp_share::cleanup_workers() {
	for(auto it = workers_.begin(); it != workers_.end();) {
		if (it->second.expired()) {
			it = workers_.erase(it);
		} else {
			++it;
		}
	}
}

inline void tcp_share::chk_need_workers() {
	if (nr_workers_ < cfg.worker_count_low) {
        auto sg = this->shared_from_this();
		co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
			co_await chk_need_workers_coro();
		}, asio::detached);
	}
}

inline awaitable<void> tcp_share::chk_need_workers_coro() {
	if (closing_)
		co_return;
	if (nr_workers_ < cfg.worker_count_low) {
		logger_.trace(fmt::format(FMT_COMPILE("got {} workers, getting more .."), nr_workers_));
		co_await add_workers(cfg.worker_count_more);
	}
}

inline awaitable<void> tcp_share::add_workers(size_t count) {
	cleanup_workers();
	for (int i = 0; i < count; ++i) {
		co_await add_worker();
	}
	logger_.trace(fmt::format(FMT_COMPILE("got {} more workers"), count));
}

inline controller::controller(asio::io_context &ioc)
	: ioc_(ioc), s_(ioc), ep_(asio::ip::address::from_string(cfg.server_host), cfg.server_port), client_uuid_(uuids::to_string(uuids::random_generator()())), logger_(log::tag_controller{client_uuid_}), ping_timer_(ioc)
{
	hello_.version = 0; // TODO
	hello_.client_uuid = client_uuid_;
	logger_.info(fmt::format(FMT_COMPILE("client uuid : {}"), client_uuid_));
}

inline void controller::init() {
	for (auto& it : cfg.tcp_shares) {
		add_tcp_share(it.first, {asio::ip::address::from_string(it.second.local_host), it.second.local_port}, it.second.remote_port);
	}
}

inline shared_ptr<controller> controller::create(asio::io_context &ioc) {
	return make_shared<controller>(ioc);
}

inline void controller::add_tcp_share(string share_id, tcp::endpoint ep, unsigned short port) {
	tcp_share_ptr_t sh = tcp_share::create(ioc_, this->shared_from_this(), share_id, move(ep), port);
	sh->run();
	tcp_shares_.emplace(share_id, sh);

	msg::tcp_share sh_msg;
	sh_msg.id = sh->share_id_;
	sh_msg.port = port;
	hello_.tcp_shares.emplace_back(move(sh_msg));

	logger_.info(fmt::format(FMT_COMPILE("add tcp share : {}"), share_id));
}

inline void controller::try_stop() noexcept {
	stopping_ = true;
	try {
		s_.close();
	} catch(...) {}
	for (auto& it : tcp_shares_) {
		if (tcp_share_ptr_t sh = it.second.lock()) {
			sh->try_stop();
		}
	}
	try {
		ping_timer_.cancel();
	} catch (...) {}
}

inline void controller::handle_error(const exception& e) noexcept {
	if (!stopping_) {
		exit_code = 1;
		logger_.error("got an exception, stopping : ").with_exception(e);
		try_stop();
	} else {
		logger_.trace("exited by exception : ").with_exception(e);
	}
}

inline void controller::run() {
	ping_timer_.expires_at(steady_timer::time_point::max());
	auto sg = this->shared_from_this();
	co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
		co_await controller_socket_send_recv_msgs();
	}, asio::detached);
	co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
		co_await ping_actor();
	}, asio::detached);
}

inline awaitable<tcp::socket> controller::get_socket() {
	tcp::socket ret{ioc_};
	co_await ret.async_connect(ep_, asio::use_awaitable);
	co_return move(ret);
}

inline awaitable<void> controller::controller_socket_send_recv_msgs() {
	try {
		co_await s_.async_connect(ep_, asio::use_awaitable);

		co_await send_msg(s_, marshal_msg(hello_));

		auto f_in = co_await recv_msg(s_);
		co_await visit([this](auto&& m) mutable -> auto {
			return handle_msg(forward<decltype(m)>(m));
		}, unmarshal_msg<msg::server_hello>(f_in));

		for (auto& it : tcp_shares_) {
			if (tcp_share_ptr_t ptr = it.second.lock()) {
				co_await ptr->add_workers(cfg.worker_count_initial);
			}
		}

		for(;;) {
			set_ping_timer(chrono::seconds{20});
			auto in = co_await recv_msg(s_);
			co_await visit([this](auto&& m) mutable -> auto {
				return handle_msg(forward<decltype(m)>(m));
			}, unmarshal_msg<msg::pong>(in));
		}
	} catch (const exception& e) {
		handle_error(e);
	}
}

inline void controller::set_ping_timer(chrono::seconds after) {
	ping_timer_.expires_after(after);
}

inline awaitable<void> controller::ping_actor() {
	try {
		for (;;) {
			try {
				co_await ping_timer_.async_wait(asio::use_awaitable);
			} catch (const system_error & se) {
				if (se.code() != asio::error::operation_aborted) {
					throw;
				}
			}
			if (stopping_) {
				co_return;
			}
			if (ping_timer_.expiry() <= steady_timer::clock_type::now()) {
				ping_timer_.expires_at(steady_timer::time_point::max());
				msg::ping ping;
				co_await send_msg(s_, marshal_msg(ping));
				logger_.trace("sent a ping");
			}
		}
	} catch (const exception& e) {
		handle_error(e);
	}
}

inline awaitable<void> controller::handle_msg(msg::server_hello m) {
	logger_.info(fmt::format(FMT_COMPILE("server version : {}"), m.version));
	logger_.info(fmt::format(FMT_COMPILE("server welcome message: {}"), m.welcome));
	co_return;
}

inline awaitable<void> controller::handle_msg(msg::pong) {
	logger_.trace("recv a pong");
	co_return;
}

inline tcp_share_worker::tcp_share_worker(asio::io_context &ioc, tcp_share_ptr_t share, tcp::socket s, string share_id, int worker_id)
	: ioc_(ioc), share_(share), s_(move(s)), share_id_(share_id), worker_id_(worker_id), logger_(log::tag_tcp_share_worker{share_id, worker_id}), ping_timer_(ioc)
{
	share_->nr_workers_++;
}

inline tcp_share_worker::~tcp_share_worker() {
    share_->nr_workers_--;
}

inline shared_ptr<tcp_share_worker> tcp_share_worker::create(asio::io_context &ioc, tcp_share_ptr_t share, tcp::socket s, string share_id, int worker_id) {
	return make_shared<tcp_share_worker>(ioc, share, move(s), move(share_id), worker_id);
}

inline void tcp_share_worker::try_stop() {
	stopping_ = true;
	try {
		s_.close();
	} catch (...) {}
	try {
		ping_timer_.cancel();
	} catch (...) {}
}

inline void tcp_share_worker::handle_error(const exception& e) {
	if (!stopping_) {
		logger_.error("got an exception, stopping : ").with_exception(e);
		try_stop();
	} else {
		logger_.trace("exited by exception : ").with_exception(e);
	}
}

inline void tcp_share_worker::run() {
	ping_timer_.expires_at(steady_timer::time_point::max());
	auto sg = this->shared_from_this();
	co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
		co_await send_and_recv_msgs();
	}, asio::detached);
	co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
		co_await ping_actor();
	}, asio::detached);
}

inline awaitable<void> tcp_share_worker::send_and_recv_msgs() {
	try {
		msg::tcp_share_worker_hello hello;
		hello.tcp_share_id = share_id_;
		hello.worker_id = worker_id_;
		co_await send_msg(s_, marshal_msg(hello));
		while (!visited_) {
			set_ping_timer(chrono::seconds{20});
			auto in = co_await recv_msg(s_);
			co_await visit([this](auto&& m) mutable -> auto {
				return this->handle_msg(forward<decltype(m)>(m));
			}, unmarshal_msg<msg::visit_tcp_share, msg::pong>(in));
		}
	} catch (const exception& e) {
		handle_error(e);
	}
}

inline void tcp_share_worker::set_ping_timer(chrono::seconds after) {
	ping_timer_.expires_after(after);
}

inline awaitable<void> tcp_share_worker::ping_actor() {
	try {
		for (;;) {
			try {
				co_await ping_timer_.async_wait(asio::use_awaitable);
			} catch (const system_error & se) {
				if (se.code() != asio::error::operation_aborted) {
					throw;
				}
			}
			if (stopping_ || visited_) {
				co_return;
			}
			if (ping_timer_.expiry() <= steady_timer::clock_type::now()) {
				ping_timer_.expires_at(steady_timer::time_point::max());
				msg::ping ping;
				co_await send_msg(s_, marshal_msg(ping));
				logger_.trace("sent a ping");
			}
		}
	} catch (const exception& e) {
		handle_error(e);
	}
}

inline awaitable<void> tcp_share_worker::handle_msg(msg::visit_tcp_share) {
	logger_.trace("was visited");
	visited_ = true;
	ping_timer_.expires_at(steady_timer::time_point::max());
	ping_timer_.cancel();
	s_.cancel();

	msg::visit_confirmed m;
	co_await send_msg(s_, marshal_msg(m));
	logger_.trace("sent confirm");

	co_await share_->wq_.provide(move(s_));
}

inline awaitable<void> tcp_share_worker::handle_msg(msg::pong) {
	logger_.trace("recv a pong");
	co_return;
}

}

}

