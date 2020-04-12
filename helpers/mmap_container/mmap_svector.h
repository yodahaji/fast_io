#pragma once
#include<cstddef>
#include<cstdlib>
#include<concepts>
#include<iterator>
#include<memory>
#include"mmap_allocate.h"

namespace fast_io
{

struct default_init_t
{
	explicit constexpr default_init_t() = default;
};

inline constexpr default_init_t default_init{};

template<std::movable T,std::size_t N,std::size_t page_bytes_exp=12>
class mmap_svector
{
public:
	using value_type = T;
	using difference_type = std::ptrdiff_t;
	using reference = T&;
	using const_reference = T const&;
	using pointer = T*;
	using const_pointer = T const*;
	using iterator = pointer;
	using const_iterator = const_pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	pointer begin_ptr{},end_ptr{},capacity_ptr{};
	typename std::aligned_storage<sizeof(value_type),alignof(value_type)>::type static_storage[N];
private:
	constexpr void resetimpl() noexcept
	{
		if(std::is_constant_evaluated())
		{
			std::allocator<value_type> alloc;
			capacity_ptr=(end_ptr=begin_ptr=std::allocator_traits<std::allocator<value_type>>::allocate(alloc,N))+N;
		}
		else
		{
			capacity_ptr=(end_ptr=begin_ptr=reinterpret_cast<value_type*>(static_storage))+N;
		}
	}
public:
	constexpr mmap_svector() noexcept
	{
		resetimpl();
	}
	constexpr bool is_allocated_by_mmap() const noexcept
	{
		if(std::is_constant_evaluated())
			return false;
		else
			return static_cast<void const*>(begin_ptr)!=static_cast<void const*>(static_storage);
	}
	constexpr bool is_allocated_on_stack() const noexcept
	{
		if(std::is_constant_evaluated())
			return false;
		else
			return static_cast<void const*>(begin_ptr)==static_cast<void const*>(static_storage);
	}
	constexpr reference emplace_back_unchecked_default_init()
	{
		return *(new (end_ptr++) value_type);
	}
	template<typename... Args>
	requires std::constructible_from<T,Args...>
	constexpr reference emplace_back_unchecked(Args&& ...args)
	{
		return *(new (end_ptr++) value_type(std::forward<Args>(args)...));
	}
	constexpr void clear() noexcept
	{
		std::destroy(begin_ptr,end_ptr);
		end_ptr=begin_ptr;
	}
	inline constexpr std::size_t capacity() const noexcept
	{
		return capacity_ptr-begin_ptr;
	}
	inline constexpr std::size_t size() const noexcept
	{
		return end_ptr-begin_ptr;
	}
	inline static constexpr std::size_t max_size() noexcept
	{
		constexpr std::size_t bts{std::numeric_limits<std::size_t>::max()/sizeof(value_type)};
		return bts;
	}
	inline static constexpr std::size_t page_unit() noexcept
	{
		constexpr std::size_t page_bytes{1<<page_bytes_exp};
		constexpr std::size_t sz{sizeof(static_storage)<=page_bytes?1:
			sizeof(static_storage)%page_bytes?
			(sizeof(static_storage)/page_bytes+1):
			sizeof(static_storage)/page_bytes};
		return sz;
	}
private:
	constexpr void destroy_container() noexcept
	{
		clear();
		if(is_allocated_by_mmap())[[unlikely]]
			mmap_deallocate<page_bytes_exp>(begin_ptr,capacity()>>page_bytes_exp);
	}

	constexpr void reallocate_page(std::size_t pages)
	{

		if constexpr(
#if defined(__WINNT__) || defined(_MSC_VER)
			false
#else
			std::is_scalar_v<value_type>
#endif
)
		{
			if(is_allocated_by_mmap())[[unlikely]]
			{
				mmap_allocation_unique_ptr<value_type,page_bytes_exp> uptr(pages,begin_ptr);
				end_ptr=uptr.get()+(end_ptr-begin_ptr);
				capacity_ptr=(begin_ptr=uptr.release())+(pages<<page_bytes_exp)/sizeof(value_type);
			}
			else
			{
				mmap_allocation_unique_ptr<value_type,page_bytes_exp> uptr(pages);
				end_ptr=std::uninitialized_move(begin_ptr,end_ptr,uptr.get());
				capacity_ptr=(begin_ptr=uptr.release())+(pages<<page_bytes_exp)/sizeof(value_type);
			}
		}
		else
		{
			mmap_allocation_unique_ptr<value_type,page_bytes_exp> uptr(pages);
			end_ptr=std::uninitialized_move(begin_ptr,end_ptr,uptr.get());
			if(is_allocated_by_mmap())[[unlikely]]
			{
				auto v{begin_ptr};
				std::size_t nb{capacity()>>(page_bytes_exp)};
				mmap_deallocate<page_bytes_exp>(v,nb);
			}
			capacity_ptr=(begin_ptr=uptr.release())+(pages<<page_bytes_exp)/sizeof(value_type);
		}
	}
	constexpr std::size_t next_page_capacity() const noexcept
	{
		if(is_allocated_by_mmap())
			return (capacity()*sizeof(value_type))>>(page_bytes_exp-1);
		return (page_unit()<<1);
	}
	static constexpr std::size_t allocate_pages_by_n(std::size_t n) noexcept
	{
		n*=sizeof(value_type);
		constexpr std::size_t pus{(page_unit()<<page_bytes_exp)/sizeof(T)};
		return n%pus?n/pus+1:n/pus;
	}
public:
/*
	constexpr void shrink_to_fit()
	{
		if(capacity()<=size())
			return;
		if(capacity()<=)
	}
	constexpr void reserve(std::size_t n)
	{
		if(capacity()<=n)
			return;
		if(n<=N)
		{

		}
		else
		{
			n%page_size()?
		}
	}
*/
	constexpr reference emplace_back_default_init()
	{
		if(end_ptr==capacity_ptr)[[unlikely]]
			reallocate_page(next_page_capacity());
		return *(new (end_ptr++) value_type);
	}
	template<typename... Args>
	constexpr reference emplace_back(Args&& ...args)
	{
		if(end_ptr==capacity_ptr)[[unlikely]]
			reallocate_page(next_page_capacity());
		return *(new (end_ptr++) value_type(std::forward<Args>(args)...));
	}
	constexpr reference pop_back() noexcept
	{
		(--end_ptr)->~value_type();
	}
	constexpr mmap_svector(std::size_t n,T const& init_val) requires(std::default_initializable<T>)
	{
		if(n<=N)[[likely]]
		{
			capacity_ptr=(end_ptr=begin_ptr=reinterpret_cast<value_type*>(static_storage))+N;
			std::uninitialized_fill_n(begin_ptr,n,init_val);
			end_ptr=begin_ptr+n;
		}
		else
		{
			std::size_t const cap_s{allocate_pages_by_n(n)*sizeof(T)};
			mmap_allocation_unique_ptr<T,page_bytes_exp> uptr(cap_s);
			std::uninitialized_fill(uptr.get(),uptr.get()+n,init_val);
			begin_ptr=uptr.release();
			end_ptr=begin_ptr+n;
			capacity_ptr=begin_ptr+(cap_s<<page_bytes_exp);
	
		}
	}
	constexpr mmap_svector(std::size_t n,default_init_t) requires(std::default_initializable<T>)
	{
		if(n<=N)[[likely]]
		{
			capacity_ptr=(end_ptr=(begin_ptr=reinterpret_cast<value_type*>(static_storage)+n))+N;
			std::uninitialized_default_construct(begin_ptr,end_ptr);
		}
		else
		{
			std::size_t const cap_s{allocate_pages_by_n(n)*sizeof(T)};
			mmap_allocation_unique_ptr<T,page_bytes_exp> uptr(cap_s);
			std::uninitialized_default_construct(uptr.get(),uptr.get()+n);
			begin_ptr=uptr.release();
			end_ptr=begin_ptr+n;
			capacity_ptr=begin_ptr+(cap_s<<page_bytes_exp);
	
		}
	}
	constexpr mmap_svector(std::size_t n) requires(std::default_initializable<T>)
	{
		if(n<=N)[[likely]]
		{
			capacity_ptr=(end_ptr=(begin_ptr=reinterpret_cast<value_type*>(static_storage)+n))+N;
			std::uninitialized_value_construct(begin_ptr,end_ptr);
		}
		else
		{
			std::size_t const cap_s{allocate_pages_by_n(n)*sizeof(T)};
			mmap_allocation_unique_ptr<T,page_bytes_exp> uptr(cap_s);
			if constexpr(std::is_scalar_v<value_type>)
				std::uninitialized_default_construct(uptr.get(),uptr.get()+n);
			else
				std::uninitialized_value_construct(uptr.get(),uptr.get()+n);
			begin_ptr=uptr.release();
			end_ptr=begin_ptr+n;
			capacity_ptr=begin_ptr+(cap_s<<page_bytes_exp);
		}
	}
	mmap_svector(mmap_svector const&) = delete;
	mmap_svector& operator=(mmap_svector const&) = delete;
	constexpr mmap_svector(mmap_svector&& bmv) noexcept
	{
		if(is_allocated_by_mmap())[[unlikely]]
		{
			begin_ptr=bmv.begin_ptr;
			end_ptr=bmv.end_ptr;
			capacity_ptr=bmv.capacity_ptr;
			bmv.resetimpl();
		}
		else
		{
			end_ptr=std::uninitialized_move(bmv.begin_ptr,bmv.end_ptr,begin_ptr);
			bmv.end_ptr=bmv.begin_ptr;
		}
	}
	constexpr mmap_svector& operator=(mmap_svector&& bmv) noexcept
	{
		if(this!=std::addressof(bmv))[[likely]]
		{
			destroy_container();
			if(is_allocated_by_mmap())[[unlikely]]
			{
				begin_ptr=bmv.begin_ptr;
				end_ptr=bmv.end_ptr;
				capacity_ptr=bmv.capacity_ptr;
				bmv.resetimpl();
			}
			else
			{
				end_ptr=std::uninitialized_move(bmv.begin_ptr,bmv.end_ptr,begin_ptr);
				bmv.end_ptr=bmv.begin_ptr;
			}
		}
		return *this;
	}
	constexpr pointer data() noexcept
	{
		return begin_ptr;
	}
	constexpr const_pointer data() const noexcept
	{
		return begin_ptr;
	}

	constexpr iterator begin() noexcept
	{
		return begin_ptr;
	}
	constexpr iterator end() noexcept
	{
		return end_ptr;
	}
	constexpr const_iterator begin() const noexcept
	{
		return begin_ptr;
	}
	constexpr const_iterator end() const noexcept
	{
		return end_ptr;
	}
	constexpr const_iterator cbegin() const noexcept
	{
		return begin_ptr;
	}
	constexpr const_iterator cend() const noexcept
	{
		return end_ptr;
	}

	constexpr reverse_iterator rbegin() noexcept
	{
		return {end_ptr};
	}
	constexpr reverse_iterator rend() noexcept
	{
		return {begin_ptr};
	}
	constexpr const_reverse_iterator rbegin() const noexcept
	{
		return {end_ptr};
	}
	constexpr const_reverse_iterator rend() const noexcept
	{
		return {begin_ptr};
	}
	constexpr const_reverse_iterator crbegin() const noexcept
	{
		return {end_ptr};
	}
	constexpr const_reverse_iterator crend() const noexcept
	{
		return {begin_ptr};
	}
	constexpr const_reference front() const noexcept
	{
		return *begin_ptr;
	}
	constexpr reference front() noexcept
	{
		return *begin_ptr;
	}
	constexpr const_reference back() const noexcept
	{
		return end_ptr[-1];
	}
	constexpr reference back() noexcept
	{
		return end_ptr[-1];
	}
	template<std::integral inttype>
	constexpr const_reference operator[](inttype i) const noexcept
	{
		return begin_ptr[i];
	}
	template<std::integral inttype>
	constexpr reference operator[](inttype i) noexcept
	{
		return begin_ptr[i];
	}
	constexpr ~mmap_svector()
	{
		destroy_container();
	}
};

}