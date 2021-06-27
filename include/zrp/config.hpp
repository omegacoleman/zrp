// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

#include "zrp/json_misc.hpp"

namespace zrp {

namespace client {

struct config_t {

	string server_host;
	unsigned short server_port;

	struct tcp_share_t {
		string local_host;
		unsigned short local_port;
		unsigned short remote_port;
	};
	map<string, tcp_share_t> tcp_shares;

	int forwarder_threads;

	int worker_count_initial;
	int worker_count_low;
	int worker_count_more;

	bool access_log;

	int rlimit_nofile;
};

void tag_invoke(json::value_from_tag, json::value& jv, const config_t::tcp_share_t& c)
{
	jv = {
		{"local_host", c.local_host},
		{"local_port", c.local_port},
		{"remote_port", c.remote_port},
	};
}

config_t::tcp_share_t tag_invoke(json::value_to_tag<config_t::tcp_share_t>, const json::value& jv)
{
	config_t::tcp_share_t ret;
	json::object const& obj = jv.as_object();
	extract_with_default(obj, ret.local_host, "local_host", "127.0.0.1");
	extract(obj, ret.local_port, "local_port");
	extract(obj, ret.remote_port, "remote_port");
	return ret;
}

void tag_invoke(json::value_from_tag, json::value& jv, const config_t& c)
{
	jv = {
		{"server_host", c.server_host},
		{"server_port", c.server_port},
		{"tcp_shares", c.tcp_shares},
		{"forwarder_threads", c.forwarder_threads},
		{"worker_count_initial", c.worker_count_initial},
		{"worker_count_low", c.worker_count_low},
		{"worker_count_more", c.worker_count_more},
		{"access_log", c.access_log},
		{"rlimit_nofile", c.rlimit_nofile},
	};
}

config_t tag_invoke(json::value_to_tag<config_t>, const json::value& jv)
{
	config_t ret;
	json::object const& obj = jv.as_object();
	extract(obj, ret.server_host, "server_host");
	extract_with_default(obj, ret.server_port, "server_port", 11433);
	extract_with_default(obj, ret.tcp_shares, "tcp_shares", map<string, config_t::tcp_share_t>{});
	extract_with_default(obj, ret.forwarder_threads, "forwarder_threads", -1);
	extract_with_default(obj, ret.worker_count_initial, "worker_count_initial", 16);
	extract_with_default(obj, ret.worker_count_low, "worker_count_low", 8);
	extract_with_default(obj, ret.worker_count_more, "worker_count_more", 16);
	extract_with_default(obj, ret.access_log, "access_log", true);
	extract_with_default(obj, ret.rlimit_nofile, "rlimit_nofile", 65533);
	return ret;
}

json::value example_config() {
	json::object obj;
	obj["server_host"] = "192.168.0.33";

	json::object share_ssh;
	share_ssh["local_port"] = 22;
	share_ssh["remote_port"] = 9022;

	json::object share_http;
	share_http["local_port"] = 8080;
	share_http["remote_port"] = 8080;

	json::object shares;
	shares["ssh"] = share_ssh;
	shares["http"] = share_http;
	obj["tcp_shares"] = shares;

	return move(obj);
}

}

namespace server {

struct config_t {
	string server_host;
	unsigned short server_port;
	string sharing_host;
	string welcome;

	int forwarder_threads;

	bool access_log;

	int rlimit_nofile;
};

void tag_invoke(json::value_from_tag, json::value& jv, const config_t& c)
{
	jv = {
		{"server_host", c.server_host},
		{"server_port", c.server_port},
		{"sharing_host", c.sharing_host},
		{"welcome", c.welcome},
		{"forwarder_threads", c.forwarder_threads},
		{"access_log", c.access_log},
		{"rlimit_nofile", c.rlimit_nofile},
	};
}

config_t tag_invoke(json::value_to_tag<config_t>, const json::value& jv)
{
	config_t ret;
	json::object const& obj = jv.as_object();
	extract(obj, ret.server_host, "server_host");
	extract_with_default(obj, ret.server_port, "server_port", 11433);
	extract_with_default(obj, ret.sharing_host, "sharing_host", "0.0.0.0");
	extract_with_default(obj, ret.welcome, "welcome", "welcome to zrp server");
	extract_with_default(obj, ret.forwarder_threads, "forwarder_threads", -1);
	extract_with_default(obj, ret.access_log, "access_log", true);
	extract_with_default(obj, ret.rlimit_nofile, "rlimit_nofile", 65533);
	return ret;
}

json::value example_config() {
	json::object obj;
	obj["server_host"] = "0.0.0.0";
	obj["server_port"] = 11433;

	return move(obj);
}

}

}


