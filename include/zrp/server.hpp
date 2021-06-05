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

namespace server {

static inline config_t cfg;

static inline asio::ip::address tcp_share_host = asio::ip::address::from_string("0.0.0.0");
static inline string welcome_msg = "welcome to zrp server";

inline void load_config(const string_view filename) {
	static json::value jv = parse_file(filename);
	cfg = json::value_to<config_t>(jv);

	log::as(log::tag_server{}).info("Loaded config : ");
	pretty_print(json::value_from(cfg));

	try_set_rlimit_nofile(cfg.rlimit_nofile);
	tcp_share_host = asio::ip::address::from_string(cfg.sharing_host);
	welcome_msg = cfg.welcome;
}

extern int exit_code;

struct tcp_share;
struct tcp_share_worker;
struct controller_socket;
struct server;

using tcp_share_ptr_t = shared_ptr<tcp_share>;
using tcp_share_weak_ptr_t = weak_ptr<tcp_share>;
using tcp_share_worker_ptr_t = shared_ptr<tcp_share_worker>;
using tcp_share_worker_weak_ptr_t = weak_ptr<tcp_share_worker>;
using ctrl_t = controller_socket;
using ctrl_ptr_t = shared_ptr<ctrl_t>;
using ctrl_weak_ptr_t = weak_ptr<ctrl_t>;

struct tcp_share : enable_shared_from_this<tcp_share> {
	asio::io_context &ioc_;
	string share_id_;
	unsigned short listen_port_;
	tcp::endpoint listen_;
	atomic<int> nr_workers_ = 0;
	ctrl_ptr_t ctrl_;

	waitqueue<tcp_share_worker_weak_ptr_t> wq_;
	map<int, tcp_share_worker_weak_ptr_t> workers_{};

	bool closing_ = false;
	log::logger logger_;

	tcp_share(asio::io_context &ioc, ctrl_ptr_t ctrl, string share_id, unsigned short listen_port);
	static shared_ptr<tcp_share> create(asio::io_context &ioc, ctrl_ptr_t ctrl, string share_id, unsigned short port);

	struct upstream {
		using tcp_share_ptr_t = shared_ptr<tcp_share>;

		tcp_share_ptr_t sh_;

		upstream(tcp_share_ptr_t sh);
		awaitable<tcp::socket> get_socket();
	};

	struct downstream {
		using tcp_share_ptr_t = shared_ptr<tcp_share>;

		tcp_share_ptr_t sh_;
		tcp::acceptor ac_;

		downstream(tcp_share_ptr_t sh);
		void try_stop() noexcept;
		awaitable<tcp::socket> get_socket();
	};

	using forwarder_t = forwarder<upstream, downstream>;
	using forwarder_ptr_t = shared_ptr<forwarder_t>;
	using forwarder_weak_ptr_t = weak_ptr<forwarder_t>;
	forwarder_weak_ptr_t fwd_;

	upstream make_upstream();
	downstream make_downstream();

	void try_stop() noexcept;
	void handle_error(const exception& e) noexcept;

	void run();
	awaitable<void> run_forwarder();

	void cleanup_workers();
	awaitable<void> got_worker(tcp_share_worker_weak_ptr_t w);
};

struct tcp_share_worker : enable_shared_from_this<tcp_share_worker> {
	asio::io_context &ioc_;
	tcp::socket s_;
	bool visited_ = false;
	int id_;
	waitqueue<msg_t> to_send_;

	tcp_share_ptr_t share_;

	bool stopping_ = false;
	log::logger logger_;

	steady_timer ddl_;
	string_view ddl_action_;

	tcp_share_worker(asio::io_context &ioc, tcp_share_ptr_t share, int id, tcp::socket s);
	~tcp_share_worker();
	static shared_ptr<tcp_share_worker> create(asio::io_context &ioc, tcp_share_ptr_t share, int id, tcp::socket s);

	void try_stop() noexcept;
	void handle_error(const exception& e) noexcept;

	void run();
	awaitable<void> recv_msgs();
	awaitable<void> send_msgs();

	void set_ddl(const string_view action, chrono::seconds after);
	void cancel_ddl();
	awaitable<void> ddl_actor();

	awaitable<void> handle_msg(msg::ping);

	awaitable<tcp::socket> visit();

};

struct controller_socket : enable_shared_from_this<controller_socket> {
	asio::io_context &ioc_;
	tcp::socket s_;
	string client_uuid_;
	map<string, tcp_share_weak_ptr_t> shares_;
	waitqueue<msg_t> to_send_;
	bool stopping_ = false;
	log::logger logger_;

	steady_timer ddl_;
	string_view ddl_action_;

	controller_socket(asio::io_context &ioc, tcp::socket s, string client_uuid);
	static shared_ptr<controller_socket> create(asio::io_context &ioc, tcp::socket s, string client_uuid);

	tcp_share_ptr_t add_tcp_share(string share_id, unsigned short port);

	void try_stop() noexcept;
	void handle_error(const exception& e) noexcept;

	void run();
	awaitable<void> recv_msgs();
	awaitable<void> send_msgs();

	void set_ddl(const string_view action, chrono::seconds after);
	void cancel_ddl();
	awaitable<void> ddl_actor();

	awaitable<void> handle_msg(msg::ping);
};

struct server : enable_shared_from_this<server> {
	asio::io_context &ioc_;
	map<string, ctrl_weak_ptr_t> ctrls_;
	map<string, tcp_share_weak_ptr_t> tcp_shares_;
	tcp::acceptor ac_;
	bool stopping_ = false;
	log::logger logger_;

	server(asio::io_context &ioc);
	static shared_ptr<server> create(asio::io_context &ioc);

	void try_stop() noexcept;
	void handle_error(const exception &e) noexcept;

	void run();
	awaitable<void> serve();
	awaitable<void> handle_socket(tcp::socket s);

	awaitable<void> handle_hello_msg(msg::client_hello hello, tcp::socket s);
	awaitable<void> handle_hello_msg(msg::tcp_share_worker_hello hello, tcp::socket s);
};

inline tcp_share::tcp_share(asio::io_context &ioc, ctrl_ptr_t ctrl, string share_id, unsigned short listen_port)
	: ioc_(ioc), ctrl_(ctrl), share_id_(share_id), listen_port_(listen_port), listen_(tcp_share_host, listen_port), wq_(ioc.get_executor()), logger_(log::tag_tcp_share{share_id})
{}

inline shared_ptr<tcp_share> tcp_share::create(asio::io_context &ioc, ctrl_ptr_t ctrl, string share_id, unsigned short port) {
	return make_shared<tcp_share>(ioc, ctrl, move(share_id), port);
}

inline tcp_share::upstream::upstream(tcp_share_ptr_t sh)
	: sh_(sh)
{}

inline awaitable<tcp::socket> tcp_share::upstream::get_socket() {
	for (;;) {
		if(auto worker = (co_await sh_->wq_.wait()).lock()) { // skip if worker died before visit
			co_return co_await worker->visit();
		}
	}
}

inline tcp_share::downstream::downstream(tcp_share_ptr_t sh)
	: ac_(sh->ioc_, sh->listen_), sh_(sh)
{}

inline void tcp_share::downstream::try_stop() noexcept {
	try {
		ac_.close();
	} catch(...) {}
}

inline awaitable<tcp::socket> tcp_share::downstream::get_socket() {
	co_return co_await ac_.async_accept(asio::use_awaitable);
}

inline tcp_share::upstream tcp_share::make_upstream() {
	return {this->shared_from_this()};
}

inline tcp_share::downstream tcp_share::make_downstream() {
	return {this->shared_from_this()};
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
		logger_.error("got an exception, stopping whole client : ").with_exception(e);
		ctrl_->try_stop(); // stop the whole ctrl when tcp share got an error
	} else {
		logger_.trace("exited by exception : ").with_exception(e);
	}
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

inline void tcp_share::run() {
	auto sg = this->shared_from_this();
	co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
		co_await run_forwarder();
	}, asio::detached);
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

inline awaitable<void> tcp_share::got_worker(tcp_share_worker_weak_ptr_t w) {
	int id;
	if (auto ptr = w.lock()) {
		id = ptr->id_;
	} else {
		co_return;
	}
	cleanup_workers();
	auto exec = co_await this_coro::executor;
	workers_.emplace(id, w);
	co_await wq_.provide(w);
}

inline tcp_share_worker::tcp_share_worker(asio::io_context &ioc, tcp_share_ptr_t share, int id, tcp::socket s)
		: ioc_(ioc), share_(share), id_(id), s_(move(s)), to_send_(ioc.get_executor()), logger_(log::tag_tcp_share_worker{share->share_id_, id}), ddl_(ioc)
{
	share_->nr_workers_ ++;
}

inline tcp_share_worker::~tcp_share_worker() {
	share_->nr_workers_ --;
}

inline shared_ptr<tcp_share_worker> tcp_share_worker::create(asio::io_context &ioc, tcp_share_ptr_t share, int id, tcp::socket s) {
	return make_shared<tcp_share_worker>(ioc, share, id, move(s));
}

inline void tcp_share_worker::try_stop() noexcept {
	stopping_ = true;
	if (visited_) {
		return;
	}
	to_send_.close();
	try {
		s_.close();
	} catch (...) {}
	try {
		ddl_.cancel();
	} catch (...) {}
}

inline void tcp_share_worker::handle_error(const exception& e) noexcept {
	if (visited_) {
		return;
	}
	if (!stopping_) {
		logger_.error("got an exception, stopping : ").with_exception(e);
		try_stop();
	} else {
		logger_.trace("exited by exception : ").with_exception(e);
	}
}

inline void tcp_share_worker::run() {
	ddl_.expires_at(steady_timer::time_point::max());
	auto sg = this->shared_from_this();
	co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
		co_await recv_msgs();
	}, asio::detached);
	co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
		co_await send_msgs();
	}, asio::detached);
	co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
		co_await ddl_actor();
	}, asio::detached);
}

inline awaitable<void> tcp_share_worker::recv_msgs() {
	try {
		for (;;) {
			set_ddl("recv_msgs()", std::chrono::seconds(60));
			auto in = co_await recv_msg(s_);
			co_await std::visit([this](auto&& m) mutable -> auto {
				return this->handle_msg(forward<decltype(m)>(m));
			}, unmarshal_msg<msg::ping>(in));
		}
	} catch (const exception& e) {
		handle_error(e);
	}
}

inline awaitable<void> tcp_share_worker::send_msgs() {
	try {
		for (;;) {
			msg_t m = co_await to_send_.wait();
			co_await send_msg(s_, m);
		}
	} catch (const exception& e) {
		handle_error(e);
	}
}

inline void tcp_share_worker::set_ddl(const string_view action, chrono::seconds after) {
	ddl_.expires_after(after);
	ddl_action_ = action;
}

inline void tcp_share_worker::cancel_ddl() {
	ddl_.expires_at(steady_timer::time_point::max());
}

inline awaitable<void> tcp_share_worker::ddl_actor() {
	try {
		for (;;) {
			try {
				co_await ddl_.async_wait(asio::use_awaitable);
			} catch (const system_error & se) {
				if (se.code() != asio::error::operation_aborted) {
					throw;
				}
			}
			if (stopping_ || visited_) {
				co_return;
			}
			if (ddl_.expiry() <= steady_timer::clock_type::now()) {
				logger_.warning(fmt::format(FMT_COMPILE("timeout exceeded : {}"), ddl_action_));
				try_stop();
				co_return;
			}
		}
	} catch (const exception& e) {
		handle_error(e);
	}
}

inline awaitable<void> tcp_share_worker::handle_msg(msg::ping) {
	logger_.trace("recv a ping");
	msg::pong pong;
	co_await to_send_.provide(marshal_msg(pong));
	logger_.trace("sent a pong");
}

inline awaitable<tcp::socket> tcp_share_worker::visit() {
	visited_ = true;
	msg::visit_tcp_share v;
	co_await to_send_.provide(marshal_msg(v));
	ddl_.expires_at(steady_timer::time_point::max());
	ddl_.cancel();
	s_.cancel();
	to_send_.close();
	co_return move(s_);
}

inline controller_socket::controller_socket(asio::io_context &ioc, tcp::socket s, string client_uuid)
	: ioc_(ioc), s_(move(s)), client_uuid_(client_uuid), to_send_(ioc.get_executor()), logger_(log::tag_controller{client_uuid}), ddl_(ioc)
{
	logger_.info("connected");
}

inline shared_ptr<controller_socket> controller_socket::create(asio::io_context &ioc, tcp::socket s, string client_uuid) {
	return make_shared<controller_socket>(ioc, move(s), move(client_uuid));
}

inline tcp_share_ptr_t controller_socket::add_tcp_share(string share_id, unsigned short port) {
	logger_.info(fmt::format(FMT_COMPILE("add tcp share : {} at port {}"), share_id, port));
	tcp_share_ptr_t sh = tcp_share::create(ioc_, this->shared_from_this(), share_id, port);
	shares_.emplace(share_id, sh);
	sh->run();
	return sh;
}

inline void controller_socket::try_stop() noexcept {
	stopping_ = true;
	for (auto& it : shares_) {
		if (auto ptr = it.second.lock()) {
			ptr->try_stop();
		}
	}
	to_send_.close();
	try {
		s_.close();
	} catch (...) {}
	try {
		ddl_.cancel();
	} catch (...) {}
}

inline void controller_socket::handle_error(const exception& e) noexcept {
	if (!stopping_) {
		logger_.error("got an exception, stopping : ").with_exception(e);
		try_stop();
	} else {
		logger_.trace("exited by exception : ").with_exception(e);
	}
}

inline void controller_socket::run() {
	ddl_.expires_at(steady_timer::time_point::max());
	auto sg = this->shared_from_this();
	co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
		co_await recv_msgs();
	}, asio::detached);
	co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
		co_await send_msgs();
	}, asio::detached);
}

inline awaitable<void> controller_socket::recv_msgs() {
	try {
		for (;;) {
			set_ddl("recv_msgs()", std::chrono::seconds(60));
			auto in = co_await recv_msg(s_);
			co_await visit([this](auto&& m) mutable -> auto {
				return this->handle_msg(forward<decltype(m)>(m));
			}, unmarshal_msg<msg::ping>(in));
		}
	} catch (const exception& e) {
		handle_error(e);
	}
}

inline awaitable<void> controller_socket::handle_msg(msg::ping) {
	logger_.trace("recv a ping");
	msg::pong pong;
	co_await to_send_.provide(marshal_msg(pong));
	logger_.trace("sent a pong");
}

inline awaitable<void> controller_socket::send_msgs() {
	msg::server_hello hello;
	hello.version = 0; // TODO
	hello.welcome = welcome_msg;
	co_await send_msg(s_, marshal_msg(hello));
	try {
		for (;;) {
			msg_t m = co_await to_send_.wait();
			co_await send_msg(s_, m);
		}
	} catch (const exception& e) {
		handle_error(e);
	}
}

inline void controller_socket::set_ddl(const string_view action, chrono::seconds after) {
	ddl_.expires_after(after);
	ddl_action_ = action;
}

inline void controller_socket::cancel_ddl() {
	ddl_.expires_at(steady_timer::time_point::max());
}

inline awaitable<void> controller_socket::ddl_actor() {
	try {
		for (;;) {
			try {
				co_await ddl_.async_wait(asio::use_awaitable);
			} catch (const system_error & se) {
				if (se.code() != asio::error::operation_aborted) {
					throw;
				}
			}
			if (stopping_) {
				co_return;
			}
			if (ddl_.expiry() <= steady_timer::clock_type::now()) {
				logger_.warning(fmt::format(FMT_COMPILE("timeout exceeded : {}"), ddl_action_));
				try_stop();
				co_return;
			}
		}
	} catch (const exception& e) {
		handle_error(e);
	}
}

inline server::server(asio::io_context &ioc)
	: ioc_(ioc), ac_(ioc, {asio::ip::address::from_string(cfg.server_host), cfg.server_port}), logger_(log::tag_server{})
{}

inline shared_ptr<server> server::create(asio::io_context &ioc) {
	return make_shared<server>(ioc);
}

inline void server::try_stop() noexcept {
	stopping_ = true;
	for (auto& it : ctrls_) {
		if (auto ptr = it.second.lock()) {
			ptr->try_stop();
		}
	}
	ac_.close();
}

inline void server::handle_error(const exception &e) noexcept {
	if (!stopping_) {
		exit_code = 1;
		logger_.error("got an exception, stopping : ").with_exception(e);
		try_stop();
	} else {
		logger_.trace("exited by exception : ").with_exception(e);
	}
}

inline void server::run() {
	auto sg = this->shared_from_this();
	co_spawn(ioc_, [this, sg]() mutable -> awaitable<void> {
		try {
			co_await sg->serve();
		} catch(...) {};
	}, asio::detached);
}

inline awaitable<void> server::serve() {
	try {
		auto sg = this->shared_from_this();
		for (;;) {
			tcp::socket s = co_await ac_.async_accept(asio::use_awaitable);
			co_spawn(ioc_, [this, sg, s = move(s)]() mutable -> awaitable<void> {
				co_await handle_socket(move(s));
			}, asio::detached);
		}
	} catch(const exception &e) {
		handle_error(e);
		throw;
	}
}

inline awaitable<void> server::handle_socket(tcp::socket s) {
	try {
		auto m = co_await recv_msg(s);
		co_await visit([this](auto msg, tcp::socket s) mutable -> awaitable<void> {
			return handle_hello_msg(msg, move(s));
		}, unmarshal_msg<msg::client_hello, msg::tcp_share_worker_hello>(m), variant<tcp::socket>(move(s)));
	} catch(const exception &e) {
		if (!stopping_) {
			logger_.warning("got error handling hello msg : ").with_exception(e);
		}
	}
}

inline awaitable<void> server::handle_hello_msg(msg::client_hello hello, tcp::socket s) {
	string client_uuid{hello.client_uuid};

	ctrl_ptr_t ctrl = ctrl_t::create(ioc_, move(s), client_uuid);
	if (ctrls_.find(client_uuid) == ctrls_.end()) {
		ctrls_.emplace(client_uuid, ctrl);
	} else {
		if (ctrls_.at(client_uuid).expired()) {
			ctrls_.at(client_uuid) = ctrl;
		} else {
			throw std::logic_error{"same client already connected"};
		}
	}

	for (auto it : hello.tcp_shares) {
		string id{it.id};
		if (tcp_shares_.find(id) == tcp_shares_.end()) {
			tcp_shares_.emplace(it.id, ctrl->add_tcp_share(id, it.port));
		} else {
			if (tcp_shares_.at(id).expired()) {
				tcp_shares_.at(id) = ctrl->add_tcp_share(id, it.port);
			} else {
				throw std::logic_error{std::string("duplicate tcp share ") + id};
			}
		}
	}
	ctrl->run();
	co_return;
}

inline awaitable<void> server::handle_hello_msg(msg::tcp_share_worker_hello hello, tcp::socket s) {
	string tcp_share_id{hello.tcp_share_id};

	if (auto tcp_share = tcp_shares_.at(tcp_share_id).lock()) {

		tcp_share_worker_weak_ptr_t worker_weak_ptr;
		{
			auto worker_ptr = tcp_share_worker::create(ioc_, tcp_share, hello.worker_id, move(s));
			worker_ptr->run();
			worker_weak_ptr = worker_ptr;
		}
		co_spawn(ioc_, [tcp_share, worker_weak_ptr]() mutable -> awaitable<void> {
			co_await tcp_share->got_worker(worker_weak_ptr);
			co_return;
		}, asio::detached);
	} else {
		throw std::out_of_range("tcp share already closed");
	}
	co_return;
}

}

}



