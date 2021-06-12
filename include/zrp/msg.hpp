// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

#include "zrp/json_misc.hpp"
#include "zrp/exceptions.hpp"
#include "zrp/log.hpp"

namespace zrp {

	template <endian e>
	inline uint64_t extract_uint64(const span<const char, 8> data) noexcept {
		auto align = [](const char ch) -> uint64_t {
			return static_cast<uint64_t>(static_cast<uint8_t>(ch)); // mind the sign
		};
		// thanks rob pike
		if constexpr (e == endian::little) {
			return (align(data[0]) << 0)
				 | (align(data[1]) << 8)
				 | (align(data[2]) << 16)
				 | (align(data[3]) << 24)
				 | (align(data[4]) << 32)
				 | (align(data[5]) << 40)
				 | (align(data[6]) << 48)
				 | (align(data[7]) << 56);
		} else {
			return (align(data[7]) << 0)
				 | (align(data[6]) << 8)
				 | (align(data[5]) << 16)
				 | (align(data[4]) << 24)
				 | (align(data[3]) << 32)
				 | (align(data[2]) << 40)
				 | (align(data[1]) << 48)
				 | (align(data[0]) << 56);
		}
	}

	template <endian e>
	inline void put_uint64(const span<char, 8> data, const uint64_t in) noexcept {
		auto trunc = [](const uint64_t i) -> uint64_t {
			return static_cast<char>(static_cast<uint8_t>(i)); // mind the sign
		};
		if constexpr (e == endian::little) {
			data[0] = trunc(in >> 0);
			data[1] = trunc(in >> 8);
			data[2] = trunc(in >> 16);
			data[3] = trunc(in >> 24);
			data[4] = trunc(in >> 32);
			data[5] = trunc(in >> 40);
			data[6] = trunc(in >> 48);
			data[7] = trunc(in >> 56);
		} else {
			data[7] = trunc(in >> 0);
			data[6] = trunc(in >> 8);
			data[5] = trunc(in >> 16);
			data[4] = trunc(in >> 24);
			data[3] = trunc(in >> 32);
			data[2] = trunc(in >> 40);
			data[1] = trunc(in >> 48);
			data[0] = trunc(in >> 56);
		}
	}

	struct msg_t {
		json::value jv_;
	};

	string to_string(const msg_t& msg) {
		ostringstream oss;
		oss << "Data: " << msg.jv_ << "\n";
		return oss.str();
	}

#if 0
	awaitable<msg_t> recv_msg(tcp::socket& s) {
#else
	template <class AsyncReadable>
	awaitable<msg_t> recv_msg(AsyncReadable& s) {
#endif
		json::stream_parser parser_;
		msg_t msg;

		char buf_meta[8];
		size_t meta_read_sz = co_await async_read(s, buffer(buf_meta), asio::use_awaitable);
		size_t len = extract_uint64<endian::big>(span<char, 8>{buf_meta, 8});
		log::as(log::tag_msg{}).debug(fmt::format(FMT_COMPILE("Read len {}"), len));

		if (len > 8192) {
			throw exceptions::msg_too_big{len};
		}

		size_t remain = len;
		while (remain > 4096) {
			char buf_payload[4096];
			size_t payload_read_sz = co_await s.async_read_some(buffer(buf_payload), asio::use_awaitable);
			log::as(log::tag_msg{}).debug(fmt::format(FMT_COMPILE("Read payload {}"), std::string_view{buf_payload, payload_read_sz}));
			parser_.write({buf_payload, payload_read_sz});
			remain -= payload_read_sz;
		}
		if (remain > 0) {
			char buf_payload[4096];
			size_t payload_read_sz = co_await async_read(s, buffer(buf_payload, remain), asio::use_awaitable);
			log::as(log::tag_msg{}).debug(fmt::format(FMT_COMPILE("Read payload {}"), std::string_view{buf_payload, payload_read_sz}));
			parser_.write({buf_payload, payload_read_sz});
		}
		msg.jv_ = move(parser_.release());
		co_return move(msg);
	}

#if 0
	awaitable<void> send_msg(tcp::socket& s, const msg_t& msg) {
#else
	template <class AsyncReadable>
	awaitable<void> send_msg(AsyncReadable& s, const msg_t& msg) {
#endif
		string str = json::serialize(msg.jv_);
		uint64_t len = static_cast<uint64_t>(str.size());
		char buf_meta[8];
		put_uint64<endian::big>(span<char, 8>{buf_meta, 8}, len);
		co_await async_write(s, array<asio::const_buffer, 2>{buffer(buf_meta), buffer(str)}, asio::use_awaitable);
	}

	namespace msg
	{

	template <class msg>
	class msg_type_id;

	// facets
	
	struct tcp_share {
		string_view id;
		unsigned short port;
	};
	
	void tag_invoke(json::value_from_tag, json::value& jv, const tcp_share& c)
	{
		jv = {
			{"id", c.id},
			{"port", c.port},
		};
	}

	tcp_share tag_invoke(json::value_to_tag<tcp_share>, const json::value& jv)
	{
		tcp_share ts;
		json::object const& obj = jv.as_object();
		extract(obj, ts.id, "id");
		extract(obj, ts.port, "port");
		return ts;
	}

	struct tcp_endpoint {
		string_view ip;
		unsigned short port;
	};

	void tag_invoke(json::value_from_tag, json::value& jv, const tcp_endpoint& c)
	{
		jv = {
			{"ip", c.ip},
			{"port", c.port},
		};
	}

	tcp_endpoint tag_invoke(json::value_to_tag<tcp_endpoint>, const json::value& jv)
	{
		tcp_endpoint ep;
		json::object const& obj = jv.as_object();
		extract(obj, ep.ip, "ip");
		extract(obj, ep.port, "port");
		return ep;
	}

	// server -> client

	struct server_hello {
		int version;
		string_view welcome;
	};

	void tag_invoke(json::value_from_tag, json::value& jv, const server_hello& c)
	{
		jv = {
			{"version", c.version},
			{"welcome", c.welcome},
		};
	}

	server_hello tag_invoke(json::value_to_tag<server_hello>, const json::value& jv)
	{
		server_hello sh;
		json::object const& obj = jv.as_object();
		extract(obj, sh.version, "version");
		extract(obj, sh.welcome, "welcome");
		return sh;
	}

	struct pong {
	};

	void tag_invoke(json::value_from_tag, json::value& jv, const pong& c)
	{
		jv = {};
	}

	pong tag_invoke(json::value_to_tag<pong>, const json::value& jv)
	{
		return {};
	}

	struct visit_tcp_share {
		uint64_t epoch;
		tcp_endpoint peer;
	};

	void tag_invoke(json::value_from_tag, json::value& jv, const visit_tcp_share& c)
	{
		jv = {
			{"epoch", c.epoch},
			{"peer", c.peer},
		};
	}

	visit_tcp_share tag_invoke(json::value_to_tag<visit_tcp_share>, const json::value& jv)
	{
		visit_tcp_share v;
		json::object const& obj = jv.as_object();
		extract(obj, v.epoch, "epoch");
		extract(obj, v.peer, "peer");
		return v;
	}


	template <> struct msg_type_id<server_hello> { inline static const string s = "server_hello"; };
	template <> struct msg_type_id<pong> { inline static const string s = "pong"; };
	template <> struct msg_type_id<visit_tcp_share> { inline static const string s = "visit_tcp_share"; };

	// client -> server

	struct client_hello {
		int version;
		string_view client_uuid;
		vector<tcp_share> tcp_shares;
	};

	void tag_invoke(json::value_from_tag, json::value& jv, const client_hello& c)
	{
		jv = {
			{"version", c.version},
			{"client_uuid", c.client_uuid},
			{"tcp_shares", c.tcp_shares},
		};
	}

	client_hello tag_invoke(json::value_to_tag<client_hello>, const json::value& jv)
	{
		client_hello ch;
		json::object const& obj = jv.as_object();
		extract(obj, ch.version, "version");
		extract(obj, ch.client_uuid, "client_uuid");
		extract(obj, ch.tcp_shares, "tcp_shares");
		return ch;
	}

	struct tcp_share_worker_hello {
		string_view tcp_share_id;
		int worker_id;
	};

	void tag_invoke(json::value_from_tag, json::value& jv, const tcp_share_worker_hello& c)
	{
		jv = {
			{"tcp_share_id", c.tcp_share_id},
			{"worker_id", c.worker_id},
		};
	}

	tcp_share_worker_hello tag_invoke(json::value_to_tag<tcp_share_worker_hello>, const json::value& jv)
	{
		tcp_share_worker_hello wr;
		json::object const& obj = jv.as_object();
		extract(obj, wr.tcp_share_id, "tcp_share_id");
		extract(obj, wr.worker_id, "worker_id");
		return wr;
	}

	struct ping {
	};

	void tag_invoke(json::value_from_tag, json::value& jv, const ping& c)
	{
		jv = {};
	}

	ping tag_invoke(json::value_to_tag<ping>, const json::value& jv)
	{
		return {};
	}

	struct visit_confirmed {
	};

	void tag_invoke(json::value_from_tag, json::value& jv, const visit_confirmed& c)
	{
		jv = {};
	}

	visit_confirmed tag_invoke(json::value_to_tag<visit_confirmed>, const json::value& jv)
	{
		return {};
	}

	template <> struct msg_type_id<client_hello> { inline static const string s = "client_hello"; };
	template <> struct msg_type_id<ping> { inline static const string s = "ping"; };
	template <> struct msg_type_id<tcp_share_worker_hello> { inline static const string s = "tcp_share_worker_hello"; };
	template <> struct msg_type_id<visit_confirmed> { inline static const string s = "visit_confirmed"; };

	template <class ReturningVariant>
	ReturningVariant unmarshal_msg_impl(string type_id, const msg_t& msg) {
		// no expected msg type found
		throw exceptions::unexpected_msg_type(type_id);
	}

	template <class ReturningVariant, class CurrExpecting, class... OtherExpecting>
	ReturningVariant unmarshal_msg_impl(string type_id, const msg_t& msg) {
		if (type_id == msg_type_id<CurrExpecting>::s) {
			return json::value_to<CurrExpecting>(msg.jv_);
		}
		return unmarshal_msg_impl<ReturningVariant, OtherExpecting...>(type_id, msg);
	}

	template <class... Expecting>
	variant<Expecting...> unmarshal_msg(const msg_t& msg) {
		string type_id;
		extract(msg.jv_.as_object(), type_id, "msg_type");
		return unmarshal_msg_impl<variant<Expecting...>, Expecting...>(type_id, msg);
	}

	template <class Message>
	msg_t marshal_msg(Message msg) {
		msg_t ret;
		ret.jv_ = json::value_from<Message>(move(msg));
		ret.jv_.as_object()["msg_type"] = msg_type_id<Message>::s;
		return ret;
	}

	}

	using msg::unmarshal_msg;
	using msg::marshal_msg;
}


