// SPDX-License-Identifier: BSL-1.0
// copyleft 2021 youcai <omegacoleman@gmail.com>
// Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "zrp/bindings.hpp"

namespace zrp {

class file {
	FILE* f_ = nullptr;
	long size_ = 0;

	void fail(error_code& ec) {
		ec = make_error_code(static_cast<errc::errc_t>(errno));
	}

	public:

	~file() {
		if (f_) {
			std::fclose(f_);
		}
	}

	file() = default;

	file(file&& other) noexcept
		: f_(other.f_)
	{
		other.f_ = nullptr;
	}

	file(char const* path, char const* mode) {
		open(path, mode);
	}

	file& operator=(file&& other) noexcept {
		close();
		f_ = other.f_;
		other.f_ = nullptr;
		return *this;
	}

	void close() {
		if (f_)
		{
			std::fclose(f_);
			f_ = nullptr;
			size_ = 0;
		}
	}

	void open(char const* path, char const* mode, error_code& ec) {
		close();
		f_ = std::fopen(path, mode);
		if (!f_) {
			return fail(ec);
		}
		if (std::fseek(f_, 0, SEEK_END) != 0) {
			return fail(ec);
		}
		size_ = std::ftell(f_);
		if (size_ == -1) {
			size_ = 0;
			return fail(ec);
		}
		if (std::fseek(f_, 0, SEEK_SET) != 0) {
			return fail(ec);
		}
	}

	void open(char const* path, char const* mode) {
		error_code ec;
		open(path, mode, ec);
		if (ec) {
			throw system_error(ec);
		}
	}

	long size() const noexcept {
		return size_;
	}

	bool eof() const noexcept {
		return std::feof(f_) != 0;
	}

	size_t read(char* data, size_t size, error_code& ec) {
		auto const nread = std::fread(data, 1, size, f_);
		if (std::ferror(f_)) {
			fail(ec);
		}
		return nread;
	}

	size_t read(char* data, size_t size) {
		error_code ec;
		auto const nread = read(data, size, ec);
		if (ec) {
			throw system_error(ec);
		}
		return nread;
	}
};

}

