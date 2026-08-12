#pragma once
// utils/Function.hpp - type-erased callable replacing std::function<R(Args...)>
// Uses small-buffer optimization (SBO): callables ≤ 32 bytes are stored inline
#include <stdexcept>
#include <utility>
#include <type_traits>
#include <cstring>
#include <functional> // std::bad_function_call
	
namespace utils {

template <typename Sig>
class Function;	   // primary template - intentionally undefined
		
template <typename R, typename... Args>
class Function<R(Args...)> {
	static constexpr std::size_t SBO = 32;

	// Vtable for type-erased operations
	struct VTable {
		R	(*call)(void*, Args&&...);
		void    (*copy)(void* dst, const void* src);
		void	(*move)(void* dst, void* src) noexcept;
		void	(*destroy)(void*) noexcept;
		bool	heap;	// true ⇒ pointer in buf, false ⇒ value in buf
	};

	template <typename F>
	static VTable* vtable_for() {
		static VTable vt {
			// call
			[](void* p, Args&&... a) -> R {
				auto& f = *reinterpret_cast<F*>(p);
				return f(std::forward<Args>(a)...);
			},
			// copy
			[](void* dst, const void* src) {
				new (dst) F(*reinterpret_cast<const F*>(src));
			},
			// move
			[](void* dst, void* src) {
				new (dst) F(std::move(*reinterpret_cast<F*>(src)));
				reinterpret_cast<F*>(src)->~F();
			},
			// destroy
			[](void* p) noexcept { reinterpret_cast<F*>(p)->~F(); },
			// heap
			sizeof(F) > SBO
		};
		return &vt;
	}

public:
	Function() noexcept : vt_(nullptr) {}

	Function(std::nullptr_t) noexcept : vt_(nullptr) {}		// NOLINT

	template <typename F,
		  typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Function>>>
	Function(F&& f) : vt_(vtable_for<std::decay_t<F>>()) {		// NOLINT
		using FD = std::decay_t<F>;
		if constexpr (sizeof(FD) <= SBO) {
			new (buf_) FD(std::forward<F>(f));
		} else {
			*reinterpret_cast<void**>(buf_) = new FD(std::forward<F>(f));
		}
	}

	Function(const Function& o) : vt_(o.vt_) {
		if (!vt_) return;
		if (vt_->heap) {
			// The src stores a heap pointer; copy-construct a new heap object
			// and store its address.
			// We don't have access to the concrete type here, so we delegate
			// to the vtable's copy handler on the actual object pointer.
			void* src_obj = *reinterpret_cast<void* const*>(o.buf_);
			void* new_obj = ::operator new(SBO * 4); // conservative upper bound
			vt_->copy(new_obj, src_obj);
			*reinterpret_cast<void**>(buf_) = new_obj;
		} else {
			vt_->copy(buf_, o.buf_);
		}
	}

	Function(Function&& o) noexcept : vt_(o.vt_) {
		if (!vt_) return;
		if (vt_->heap) {
			std::memcpy(buf_, o.buf_, sizeof(void*));
		} else {
			vt_->move(buf_, o.buf_);
		}
		o.vt_ = nullptr;
	}

	Function& operator=(const Function& o) { Function tmp(o); swap(tmp); return *this; }
	Function& operator=(Function&& o) noexcept { Function tmp(std::move(o)); swap(tmp); return *this; }
	Function& operator=(std::nullptr_t) noexcept { destroy(); vt_ = nullptr; return *this; }

	~Function() { destroy(); }

	explicit operator bool() const noexcept { return vt_ != nullptr; }

	R operator()(Args... args) const {
		if (!vt_) throw std::bad_function_call{};
		void* obj = vt_->heap
			? *reinterpret_cast<void* const*>(buf_)
			: const_cast<void*>(static_cast<const void*>(buf_));
		return vt_->call(obj, std::forward<Args>(args)...);
	}

	void swap(Function& o) noexcept {
		// Byte-swap buffers; valid because both are either inline or heap-ptr
		alignas(alignof(void*)) char tmp[sizeof(buf_)];
		std::memcpy(tmp,    buf_,    sizeof(buf_));
		std::memcpy(buf_,   o.buf_,  sizeof(buf_));
		std::memcpy(o.buf_, tmp,     sizeof(buf_));
		std::swap(vt_, o.vt_);
	}

private:
	alignas(alignof(void*)) unsigned char buf_[SBO < sizeof(void*) ? sizeof(void*) : SBO];
	mutable VTable* vt_;

	void destroy() noexcept {
		if (!vt_) return;
		if (vt_->heap) {
			void* p = *reinterpret_cast<void**>(buf_);
			vt_->destroy(p);
			::operator delete(p);
		} else {
			vt_->destroy(buf_);
		}
	}
};

// Common aliases used throughout MathEngine
using RealFn  = Function<double(double)>;
using RealFn2 = Function<double(double, double)>;
using Func1   = Function<double(double)>;
using Func2   = Function<double(double, double)>;

} // namespace utils

