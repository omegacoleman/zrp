// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

#include "zrp/fmt_misc.hpp"
#include "zrp/args.hpp"

namespace zrp {

namespace log {

struct tag_forwarder {
	string name;

	tag_forwarder(std::string name)
		:name(name) {}
};

inline string to_string(const tag_forwarder& t) {
	return fmt::format("forwarder:{}", t.name);
}

struct tag_pipe {
	string forwarder_name;
	int id;

	tag_pipe(string forwarder_name, int id)
		:forwarder_name(forwarder_name), id(id) {}
};

inline string to_string(const tag_pipe& t) {
	return fmt::format("pipe:{}#{}", t.forwarder_name, t.id);
}

struct tag_tcp_share_worker {
	string share_id;
	int id;

	tag_tcp_share_worker(string share_id, int id)
		: share_id(share_id), id(id) {}
};

inline string to_string(const tag_tcp_share_worker& t) {
	return fmt::format("worker:{}#{}", t.share_id, t.id);
}

struct tag_tcp_share {
	string share_id;

	tag_tcp_share(string share_id)
		:share_id(share_id) {}
};

inline string to_string(const tag_tcp_share& t) {
	return fmt::format("tcp_share:{}", t.share_id);
}

struct tag_controller {
	string client_uuid;
};

inline string to_string(const tag_controller& t) {
	return fmt::format("controller:{}", t.client_uuid);
}

struct tag_server {};

inline string to_string(const tag_server& t) {
	return fmt::format("server");
}

struct tag_client {};

inline string to_string(const tag_client& t) {
	return fmt::format("client");
}

struct tag_main {};

inline string to_string(const tag_main& t) {
	return fmt::format("main");
}

struct tag_msg {};

inline string to_string(const tag_msg& t) {
	return fmt::format("msg");
}

struct tag_timeout {};

inline string to_string(const tag_timeout& t) {
	return fmt::format("timeout");
}

using tag_t = variant<tag_forwarder, tag_pipe, tag_tcp_share_worker, tag_tcp_share, tag_controller, tag_server, tag_client, tag_main, tag_msg, tag_timeout>;

inline string to_string(const tag_t &t) {
	return std::visit([](auto && t) -> string {
		return to_string(t);
	}, t);
}
enum class severity_t {
	debug,
	trace,
	info,
	warning,
	error,
};

inline string to_string(const severity_t &s) {
	switch (s) {
		case severity_t::debug:
			return "DEBG";
		case severity_t::trace:
			return "TRAC";
		case severity_t::info:
			return "INFO";
		case severity_t::warning:
			return "WARN";
		case severity_t::error:
			return "ERRO";
	}
	return "INVL";
}

struct message {
	chrono::system_clock::time_point time_;
	severity_t severity_;
	tag_t tag_{tag_main{}};
	string msg_;
	bool fired_ = false;

	message() = default;
	message(const message&) = delete;
	message(message&& o)
		: time_(move(o.time_)), severity_(move(o.severity_)), tag_(move(o.tag_)), msg_(move(o.msg_)), fired_(o.fired_)
	{
		o.fired_ = true;
	}
	message& operator=(const message&) = delete;
	message& operator=(message&& o) {
		time_ = move(o.time_);
		severity_ = move(o.severity_);
		tag_ = move(o.tag_);
		msg_ = move(o.msg_);
		fired_ = o.fired_;
		o.fired_ = true;
		return *this;
	}

	~message() {
		if (!fired_) {
			fire();
		}
	}

	message &time(chrono::system_clock::time_point t) {
		time_ = move(t);
		return *this;
	}

	message &severity(severity_t s) {
		severity_ = move(s);
		return *this;
	}

	message &tag(tag_t t) {
		tag_ = move(t);
		return *this;
	}

	message &msg(string s) {
		msg_ = move(s);
		return *this;
	}

	message &msg_append(string s) {
		msg_ += s;
		return *this;
	}

	message & operator()(string s) {
		return msg(s);
	}

	message &with_exception(const exception& e) {
		msg_append(e.what());
		return *this;
	}

	void fire() {
		if ((!show_trace) && severity_ == severity_t::trace)
			return;
		if ((!show_debug) && severity_ == severity_t::debug)
			return;
		fmt::text_style time_style = fmt::fg(fmt::terminal_color::bright_blue),
			tag_style = fmt::fg(fmt::terminal_color::blue),
			severity_style = {},
			msg_style = {};
		if (severity_ == severity_t::error) {
			msg_style = fmt::fg(fmt::terminal_color::red);
			severity_style = fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red);
		} else if (severity_ == severity_t::warning) {
			severity_style = fg(fmt::terminal_color::yellow);
		} else if (severity_ == severity_t::info) {
			severity_style = fg(fmt::terminal_color::cyan);
		}
		fmt::print("[{}][{}]<{}>: {}\n",
			fmt::format(may(time_style), "{:%a, %d %b %Y %T %z}", time_),
			fmt::format(may(severity_style), "{}", to_string(severity_)),
			fmt::format(may(tag_style), "{}", to_string(tag_)),
			fmt::format(may(msg_style), "{}", msg_));
		fired_ = true;
	}
};

struct logger {
	tag_t tag_;

	logger(tag_t t)
		:tag_(t) {}

	template<severity_t Severity>
	message gen(string s) {
		message m;
		m.time(chrono::system_clock::now()).tag(tag_).severity(Severity).msg(move(s));
		return move(m);
	}

	message debug(string s) {
		return gen<severity_t::debug>(s);
	}

	message trace(string s) {
		return gen<severity_t::trace>(s);
	}

	message info(string s) {
		return gen<severity_t::info>(s);
	}

	message warning(string s) {
		return gen<severity_t::warning>(s);
	}

	message error(string s) {
		return gen<severity_t::error>(s);
	}
};

logger as(tag_t t) {
	return {t};
}

}

}

