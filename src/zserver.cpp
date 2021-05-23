// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <vector>

#include "zrp/args.hpp"
#include "zrp/server.hpp"
#include "zrp/config.hpp"
#include "zrp/dump_config.hpp"
#include "zrp/log.hpp"

namespace zrp {
namespace server {

asio::io_context ioc;
asio::system_executor sysex;
int exit_code = 0;

weak_ptr<server> serv_weak;

void try_grace_exit() {
	auto logger = log::as(log::tag_main{});
	static bool in_grace_exit = false;
	if (in_grace_exit) {
		logger.warning("got SIGINT again, aborting ..");
		std::abort();
	}
	if (auto ptr = serv_weak.lock()) {
		logger.info("got SIGINT, trying to grace exit ..");
		ptr->try_stop();
	}
	in_grace_exit = true;
}

awaitable<void> handle_sigint(){
	auto logger = log::as(log::tag_main{});
	auto exec = co_await this_coro::executor;
	asio::signal_set signals(sysex, SIGINT);
	try {
		for (;;) {
			co_await signals.async_wait(asio::use_awaitable);
			asio::post(ioc, try_grace_exit);
		}
	} catch(const exception& e) {
		logger.error("sigint handler got error :").with_exception(e);
	}
}

void run() {
	auto logger = log::as(log::tag_main{});
	try {
		{
			auto serv = server::create(ioc);
			serv->run();
			serv_weak = serv;
		}
		co_spawn(sysex, handle_sigint, asio::detached);
		ioc.run();
		logger.info("gracefully exited");
	} catch (const exception &e) {
		logger.error("server::run() got error, exiting :").with_exception(e);
	}
	std::exit(exit_code);
}

}
}

int main(int argc, char** argv) {
	zrp::parse_env();
	auto logger = zrp::log::as(zrp::log::tag_main{});
	try {
		std::vector<zrp::string> args{argv, argv + argc};
		try {
			zrp::parse_args(args);
		} catch(const zrp::exceptions::bad_args&) {
			zrp::print_usage();
			return 1;
		}
		switch (zrp::program_action) {
			case zrp::program_action_t::run:
				zrp::server::load_config(zrp::config_file_path);
				zrp::server::run();
				break;
			case zrp::program_action_t::dump_config:
				zrp::server::dump_example_config(zrp::dump_full);
				break;
			case zrp::program_action_t::help:
				zrp::print_usage();
				break;
		}
	} catch(const zrp::exception& e) {
		logger.error("main() got error, exiting : ").with_exception(e);
		return 1;
	}
	return 0;
}

