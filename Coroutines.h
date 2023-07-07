#pragma once
#include <coroutine>

struct ReturnObject {
	struct promise_type {
		ReturnObject get_return_object() {
			return {
				// Uses C++20 designated initializer syntax
				.h_ = std::coroutine_handle<promise_type>::from_promise(*this)
			};
		}
		std::suspend_never initial_suspend() { return {}; }
		std::suspend_never final_suspend() noexcept { return {}; }
		void unhandled_exception() {}
		void return_void() {}
	};

	std::coroutine_handle<promise_type> h_;
	operator std::coroutine_handle<promise_type>() const { return h_; }
	// A coroutine_handle<promise_type> converts to coroutine_handle<>
	operator std::coroutine_handle<>() const { return h_; }
};

