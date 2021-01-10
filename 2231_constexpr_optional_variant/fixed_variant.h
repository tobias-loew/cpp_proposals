#ifndef _GLIBCXX_VARIANT
#define _GLIBCXX_VARIANT 1

#if __cplusplus >= 201703L

#include <type_traits>
#include <utility>
#include <bits/enable_special_members.h>
#include <bits/functexcept.h>
#include <bits/move.h>
#include <bits/functional_hash.h>
#include <bits/invoke.h>
#include <ext/aligned_buffer.h>
#include <bits/parse_numbers.h>
#include <bits/stl_iterator_base_types.h>
#include <bits/stl_iterator_base_funcs.h>
#include <bits/stl_construct.h>
#if __cplusplus > 201703L
# include <compare>
#endif

namespace std _GLIBCXX_VISIBILITY(default)
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

namespace __detail
{
namespace __variant
{
template<size_t _Np, typename... _Types>
struct _Nth_type;

template<size_t _Np, typename _First, typename... _Rest>
struct _Nth_type<_Np, _First, _Rest...>
    : _Nth_type<_Np-1, _Rest...> { };

template<typename _First, typename... _Rest>
struct _Nth_type<0, _First, _Rest...> {
    using type = _First;
};

} // namespace __variant
} // namespace __detail

#define __cpp_lib_variant 201606L

template<typename... _Types> class tuple;
template<typename... _Types> class variant;
template <typename> struct hash;

template<typename _Variant>
struct variant_size;

template<typename _Variant>
struct variant_size<const _Variant> : variant_size<_Variant> {};

template<typename _Variant>
struct variant_size<volatile _Variant> : variant_size<_Variant> {};

template<typename _Variant>
struct variant_size<const volatile _Variant> : variant_size<_Variant> {};

template<typename... _Types>
struct variant_size<variant<_Types...>>
                                     : std::integral_constant<size_t, sizeof...(_Types)> {};

template<typename _Variant>
inline constexpr size_t variant_size_v = variant_size<_Variant>::value;

template<size_t _Np, typename _Variant>
struct variant_alternative;

template<size_t _Np, typename _First, typename... _Rest>
struct variant_alternative<_Np, variant<_First, _Rest...>>
            : variant_alternative<_Np-1, variant<_Rest...>> {};

template<typename _First, typename... _Rest>
struct variant_alternative<0, variant<_First, _Rest...>> {
    using type = _First;
};

template<size_t _Np, typename _Variant>
using variant_alternative_t =
    typename variant_alternative<_Np, _Variant>::type;

template<size_t _Np, typename _Variant>
struct variant_alternative<_Np, const _Variant> {
    using type = add_const_t<variant_alternative_t<_Np, _Variant>>;
};

template<size_t _Np, typename _Variant>
struct variant_alternative<_Np, volatile _Variant> {
    using type = add_volatile_t<variant_alternative_t<_Np, _Variant>>;
};

template<size_t _Np, typename _Variant>
struct variant_alternative<_Np, const volatile _Variant> {
    using type = add_cv_t<variant_alternative_t<_Np, _Variant>>;
};

inline constexpr size_t variant_npos = -1;

template<size_t _Np, typename... _Types>
constexpr variant_alternative_t<_Np, variant<_Types...>>&
        get(variant<_Types...>&);

template<size_t _Np, typename... _Types>
constexpr variant_alternative_t<_Np, variant<_Types...>>&&
        get(variant<_Types...>&&);

template<size_t _Np, typename... _Types>
constexpr variant_alternative_t<_Np, variant<_Types...>> const&
        get(const variant<_Types...>&);

template<size_t _Np, typename... _Types>
constexpr variant_alternative_t<_Np, variant<_Types...>> const&&
        get(const variant<_Types...>&&);

template<typename _Result_type, typename _Visitor, typename... _Variants>
constexpr decltype(auto)
__do_visit(_Visitor&& __visitor, _Variants&&... __variants);

template <typename... _Types, typename _Tp>
constexpr decltype(auto)
__variant_cast(_Tp&& __rhs) {
    if constexpr (is_lvalue_reference_v<_Tp>) {
        if constexpr (is_const_v<remove_reference_t<_Tp>>) {
            return static_cast<const variant<_Types...>&>(__rhs);
        } else {
            return static_cast<variant<_Types...>&>(__rhs);
        }
    } else {
        return static_cast<variant<_Types...>&&>(__rhs);
    }
}

namespace __detail
{
namespace __variant
{
// Returns the first appearence of _Tp in _Types.
// Returns sizeof...(_Types) if _Tp is not in _Types.
template<typename _Tp, typename... _Types>
struct __index_of : std::integral_constant<size_t, 0> {};

template<typename _Tp, typename... _Types>
inline constexpr size_t __index_of_v = __index_of<_Tp, _Types...>::value;

template<typename _Tp, typename _First, typename... _Rest>
struct __index_of<_Tp, _First, _Rest...> :
    std::integral_constant<size_t, is_same_v<_Tp, _First>
    ? 0 : __index_of_v<_Tp, _Rest...> + 1> {};

// used for raw visitation
struct __variant_cookie {};
// used for raw visitation with indices passed in
struct __variant_idx_cookie {
    using type = __variant_idx_cookie;
};
// Used to enable deduction (and same-type checking) for std::visit:
template<typename> struct __deduce_visit_result { };

// Visit variants that might be valueless.
template<typename _Visitor, typename... _Variants>
constexpr void
__raw_visit(_Visitor&& __visitor, _Variants&&... __variants)
{
    std::__do_visit<__variant_cookie>(std::forward<_Visitor>(__visitor),
                                      std::forward<_Variants>(__variants)...);
}

// Visit variants that might be valueless, passing indices to the visitor.
template<typename _Visitor, typename... _Variants>
constexpr void
__raw_idx_visit(_Visitor&& __visitor, _Variants&&... __variants)
{
    std::__do_visit<__variant_idx_cookie>(std::forward<_Visitor>(__visitor),
                                          std::forward<_Variants>(__variants)...);
}

// _Uninitialized<T> is guaranteed to be a literal type, even if T is not.
// We have to do this, because [basic.types]p10.5.3 (n4606) is not implemented
// yet. When it's implemented, _Uninitialized<T> can be changed to the alias
// to T, therefore equivalent to being removed entirely.
//
// Another reason we may not want to remove _Uninitialzied<T> may be that, we
// want _Uninitialized<T> to be trivially destructible, no matter whether T
// is; but we will see.
template<typename _Type, bool = std::is_literal_type_v<_Type>>
struct _Uninitialized;

template<typename _Type>
struct _Uninitialized<_Type, true> {
    template<typename... _Args>
    constexpr
    _Uninitialized(in_place_index_t<0>, _Args&&... __args)
        : _M_storage(std::forward<_Args>(__args)...)
    { }

    constexpr const _Type& _M_get() const & noexcept
    {
        return _M_storage;
    }

    constexpr _Type& _M_get() & noexcept
    { return _M_storage; }

    constexpr const _Type&& _M_get() const && noexcept
    {
        return std::move(_M_storage);
    }

    constexpr _Type&& _M_get() && noexcept
    { return std::move(_M_storage); }

    _Type _M_storage;
};

template<typename _Type>
struct _Uninitialized<_Type, false> {
    template<typename... _Args>
    constexpr
    _Uninitialized(in_place_index_t<0>, _Args&&... __args)
    {
        ::new ((void*)std::addressof(_M_storage))
        _Type(std::forward<_Args>(__args)...);
    }

    const _Type& _M_get() const & noexcept
    {
        return *_M_storage._M_ptr();
    }

    _Type& _M_get() & noexcept
    { return *_M_storage._M_ptr(); }

    const _Type&& _M_get() const && noexcept
    {
        return std::move(*_M_storage._M_ptr());
    }

    _Type&& _M_get() && noexcept
    { return std::move(*_M_storage._M_ptr()); }

    __gnu_cxx::__aligned_membuf<_Type> _M_storage;
};

template<typename _Union>
constexpr decltype(auto)
__get(in_place_index_t<0>, _Union&& __u) noexcept
{
    return std::forward<_Union>(__u)._M_first._M_get();
}

template<size_t _Np, typename _Union>
constexpr decltype(auto)
__get(in_place_index_t<_Np>, _Union&& __u) noexcept
{
    return __variant::__get(in_place_index<_Np-1>,
                            std::forward<_Union>(__u)._M_rest);
}

// Returns the typed storage for __v.
template<size_t _Np, typename _Variant>
constexpr decltype(auto)
__get(_Variant&& __v) noexcept
{
    return __variant::__get(std::in_place_index<_Np>,
                            std::forward<_Variant>(__v)._M_u);
}

template<typename... _Types>
struct _Traits {
    static constexpr bool _S_default_ctor =
        is_default_constructible_v<typename _Nth_type<0, _Types...>::type>;
    static constexpr bool _S_copy_ctor =
        (is_copy_constructible_v<_Types> && ...);
    static constexpr bool _S_move_ctor =
        (is_move_constructible_v<_Types> && ...);
    static constexpr bool _S_copy_assign =
        _S_copy_ctor
        && (is_copy_assignable_v<_Types> && ...);
    static constexpr bool _S_move_assign =
        _S_move_ctor
        && (is_move_assignable_v<_Types> && ...);

    static constexpr bool _S_trivial_dtor =
        (is_trivially_destructible_v<_Types> && ...);
    static constexpr bool _S_trivial_copy_ctor =
        (is_trivially_copy_constructible_v<_Types> && ...);
    static constexpr bool _S_trivial_move_ctor =
        (is_trivially_move_constructible_v<_Types> && ...);
    static constexpr bool _S_trivial_copy_assign =
        _S_trivial_dtor && _S_trivial_copy_ctor
        && (is_trivially_copy_assignable_v<_Types> && ...);
    static constexpr bool _S_trivial_move_assign =
        _S_trivial_dtor && _S_trivial_move_ctor
        && (is_trivially_move_assignable_v<_Types> && ...);

    // The following nothrow traits are for non-trivial SMFs. Trivial SMFs
    // are always nothrow.
    static constexpr bool _S_nothrow_default_ctor =
        is_nothrow_default_constructible_v<
        typename _Nth_type<0, _Types...>::type>;
    static constexpr bool _S_nothrow_copy_ctor = false;
    static constexpr bool _S_nothrow_move_ctor =
        (is_nothrow_move_constructible_v<_Types> && ...);
    static constexpr bool _S_nothrow_copy_assign = false;
    static constexpr bool _S_nothrow_move_assign =
        _S_nothrow_move_ctor
        && (is_nothrow_move_assignable_v<_Types> && ...);
};

// Defines members and ctors.
template<typename... _Types>
union _Variadic_union { };

template<typename _First, typename... _Rest>
union _Variadic_union<_First, _Rest...> {
    constexpr _Variadic_union() : _M_rest() { }

    template<typename... _Args>
    constexpr _Variadic_union(in_place_index_t<0>, _Args&&... __args)
        : _M_first(in_place_index<0>, std::forward<_Args>(__args)...)
    { }

    template<size_t _Np, typename... _Args>
    constexpr _Variadic_union(in_place_index_t<_Np>, _Args&&... __args)
        : _M_rest(in_place_index<_Np-1>, std::forward<_Args>(__args)...)
    { }

    constexpr ~_Variadic_union() {}
    constexpr ~_Variadic_union() 
        requires (std::is_trivially_destructible_v<_First> && ... &&
                  std::is_trivially_destructible_v<_Rest>)  = default;

    _Uninitialized<_First> _M_first;
    _Variadic_union<_Rest...> _M_rest;
};

// _Never_valueless_alt is true for variant alternatives that can
// always be placed in a variant without it becoming valueless.

// For suitably-small, trivially copyable types we can create temporaries
// on the stack and then memcpy them into place.
template<typename _Tp>
struct _Never_valueless_alt
: __and_<bool_constant<sizeof(_Tp) <= 256>, is_trivially_copyable<_Tp>> {
  };

// Specialize _Never_valueless_alt for other types which have a
// non-throwing and cheap move construction and move assignment operator,
// so that emplacing the type will provide the strong exception-safety
// guarantee, by creating and moving a temporary.
// Whether _Never_valueless_alt<T> is true or not affects the ABI of a
// variant using that alternative, so we can't change the value later!

// True if every alternative in _Types... can be emplaced in a variant
// without it becoming valueless. If this is true, variant<_Types...>
// can never be valueless, which enables some minor optimizations.
template <typename... _Types>
constexpr bool __never_valueless()
{
    return _Traits<_Types...>::_S_move_assign
           && (_Never_valueless_alt<_Types>::value && ...);
}

// Defines index and the dtor, possibly trivial.
template<bool __trivially_destructible, typename... _Types>
struct _Variant_storage;

template <typename... _Types>
using __select_index =
    typename __select_int::_Select_int_base<sizeof...(_Types),
    unsigned char,
    unsigned short>::type::value_type;

template<typename... _Types>
struct _Variant_storage<false, _Types...> {
    constexpr _Variant_storage() : _M_index(variant_npos) { }

    template<size_t _Np, typename... _Args>
    constexpr _Variant_storage(in_place_index_t<_Np>, _Args&&... __args)
        : _M_u(in_place_index<_Np>, std::forward<_Args>(__args)...),
          _M_index(_Np)
    { }

    constexpr void _M_reset()
    {
        if (!_M_valid()) [[unlikely]]
            return;

        std::__do_visit<void>([](auto&& __this_mem) mutable {
            std::_Destroy(std::__addressof(__this_mem));
        }, __variant_cast<_Types...>(*this));

        _M_index = variant_npos;
    }

    constexpr ~_Variant_storage()
    {
        _M_reset();
    }

    void*
    _M_storage() const noexcept
    {
        return const_cast<void*>(static_cast<const void*>(
                                     std::addressof(_M_u)));
    }

    constexpr bool
    _M_valid() const noexcept
    {
        if constexpr (__variant::__never_valueless<_Types...>()) {
            return true;
        }
        return this->_M_index != __index_type(variant_npos);
    }

    _Variadic_union<_Types...> _M_u;
    using __index_type = __select_index<_Types...>;
    __index_type _M_index;
};

template<typename... _Types>
struct _Variant_storage<true, _Types...> {
    template<typename _Tp>
    static constexpr size_t __index_of =
        __detail::__variant::__index_of_v<_Tp, _Types...>;

    constexpr _Variant_storage() : _M_index(variant_npos) { }

    template<size_t _Np, typename... _Args>
    constexpr _Variant_storage(in_place_index_t<_Np>, _Args&&... __args)
        : _M_u(in_place_index<_Np>, std::forward<_Args>(__args)...),
          _M_index(_Np)
    { }

    constexpr void _M_reset() noexcept
    {
        _M_index = variant_npos;
    }

    void*
    _M_storage() const noexcept
    {
        return const_cast<void*>(static_cast<const void*>(
                                     std::addressof(_M_u)));
    }

    constexpr bool
    _M_valid() const noexcept
    {
        if constexpr (__variant::__never_valueless<_Types...>()) {
            return true;
        }
        return this->_M_index != __index_type(variant_npos);
    }

    _Variadic_union<_Types...> _M_u;
    using __index_type = __select_index<_Types...>;
    __index_type _M_index;
};

template<typename... _Types>
using _Variant_storage_alias =
    _Variant_storage<_Traits<_Types...>::_S_trivial_dtor, _Types...>;

template<typename _Tp, typename _Up>
constexpr void __variant_construct_single(_Tp&& __lhs, _Up&& __rhs_mem)
{
    using _Type = remove_reference_t<decltype(__rhs_mem)>;
    constexpr auto index = remove_reference_t<_Tp>::template __index_of<_Type>;

    if constexpr (!is_same_v<_Type, __variant_cookie>) {
        std::construct_at(std::addressof(__lhs._M_u),
            in_place_index<index>,
            _Type(std::forward<decltype(__rhs_mem)>(__rhs_mem)));
    }
}

template<typename... _Types, typename _Tp, typename _Up>
void __variant_construct(_Tp&& __lhs, _Up&& __rhs)
{
    __lhs._M_index = __rhs._M_index;
    __variant::__raw_visit([&__lhs](auto&& __rhs_mem) mutable {
        __variant_construct_single(std::forward<_Tp>(__lhs),
                                   std::forward<decltype(__rhs_mem)>(__rhs_mem));
    }, __variant_cast<_Types...>(std::forward<_Up>(__rhs)));
}

// The following are (Copy|Move) (ctor|assign) layers for forwarding
// triviality and handling non-trivial SMF behaviors.

template<bool, typename... _Types>
struct _Copy_ctor_base : _Variant_storage_alias<_Types...> {
    using _Base = _Variant_storage_alias<_Types...>;
    using _Base::_Base;

    _Copy_ctor_base(const _Copy_ctor_base& __rhs)
    noexcept(_Traits<_Types...>::_S_nothrow_copy_ctor)
    {
        __variant_construct<_Types...>(*this, __rhs);
    }

    _Copy_ctor_base(_Copy_ctor_base&&) = default;
    _Copy_ctor_base& operator=(const _Copy_ctor_base&) = default;
    _Copy_ctor_base& operator=(_Copy_ctor_base&&) = default;
};

template<typename... _Types>
struct _Copy_ctor_base<true, _Types...> : _Variant_storage_alias<_Types...> {
    using _Base = _Variant_storage_alias<_Types...>;
    using _Base::_Base;
};

template<typename... _Types>
using _Copy_ctor_alias =
    _Copy_ctor_base<_Traits<_Types...>::_S_trivial_copy_ctor, _Types...>;

template<bool, typename... _Types>
struct _Move_ctor_base : _Copy_ctor_alias<_Types...> {
    using _Base = _Copy_ctor_alias<_Types...>;
    using _Base::_Base;

    _Move_ctor_base(_Move_ctor_base&& __rhs)
    noexcept(_Traits<_Types...>::_S_nothrow_move_ctor)
    {
        __variant_construct<_Types...>(*this, std::move(__rhs));
    }

    template<typename _Up>
    constexpr void _M_destructive_move(unsigned short __rhs_index, _Up&& __rhs)
    {
        this->_M_reset();
        __variant_construct_single(*this, std::forward<_Up>(__rhs));
        this->_M_index = __rhs_index;
    }

    template<typename _Up>
    constexpr void _M_destructive_copy(unsigned short __rhs_index, const _Up& __rhs)
    {
        this->_M_reset();
        __variant_construct_single(*this, __rhs);
        this->_M_index = __rhs_index;
    }

    _Move_ctor_base(const _Move_ctor_base&) = default;
    _Move_ctor_base& operator=(const _Move_ctor_base&) = default;
    _Move_ctor_base& operator=(_Move_ctor_base&&) = default;
};

template<typename... _Types>
struct _Move_ctor_base<true, _Types...> : _Copy_ctor_alias<_Types...> {
    using _Base = _Copy_ctor_alias<_Types...>;
    using _Base::_Base;

    template<typename _Up>
    constexpr void _M_destructive_move(unsigned short __rhs_index, _Up&& __rhs)
    {
        this->_M_reset();
        __variant_construct_single(*this, std::forward<_Up>(__rhs));
        this->_M_index = __rhs_index;
    }

    template<typename _Up>
    constexpr void _M_destructive_copy(unsigned short __rhs_index, const _Up& __rhs)
    {
        this->_M_reset();
        __variant_construct_single(*this, __rhs);
        this->_M_index = __rhs_index;
    }
};

template<typename... _Types>
using _Move_ctor_alias =
    _Move_ctor_base<_Traits<_Types...>::_S_trivial_move_ctor, _Types...>;

template<bool, typename... _Types>
struct _Copy_assign_base : _Move_ctor_alias<_Types...> {
    using _Base = _Move_ctor_alias<_Types...>;
    using _Base::_Base;

    constexpr _Copy_assign_base&
    operator=(const _Copy_assign_base& __rhs)
    noexcept(_Traits<_Types...>::_S_nothrow_copy_assign)
    {
        __variant::__raw_idx_visit(
        [this](auto&& __rhs_mem, auto __rhs_index) mutable {
            if constexpr (__rhs_index != variant_npos)
            {
                if (this->_M_index == __rhs_index) {
                    __variant::__get<__rhs_index>(*this) = __rhs_mem;
                } else {
                    using __rhs_type = __remove_cvref_t<decltype(__rhs_mem)>;
                    if constexpr (is_nothrow_copy_constructible_v<__rhs_type>
                                  || !is_nothrow_move_constructible_v<__rhs_type>)
                        // The standard says this->emplace<__rhs_type>(__rhs_mem)
                        // should be used here, but _M_destructive_copy is
                        // equivalent in this case. Either copy construction
                        // doesn't throw, so _M_destructive_copy gives strong
                        // exception safety guarantee, or both copy construction
                        // and move construction can throw, so emplace only gives
                        // basic exception safety anyway.
                    {
                        this->_M_destructive_copy(__rhs_index, __rhs_mem);
                    } else
                        __variant_cast<_Types...>(*this)
                            = variant<_Types...>(__rhs_mem);
                }
            } else
            {
                this->_M_reset();
            }
        }, __variant_cast<_Types...>(__rhs));
        return *this;
    }

    _Copy_assign_base(const _Copy_assign_base&) = default;
    _Copy_assign_base(_Copy_assign_base&&) = default;
    _Copy_assign_base& operator=(_Copy_assign_base&&) = default;
};

template<typename... _Types>
struct _Copy_assign_base<true, _Types...> : _Move_ctor_alias<_Types...> {
    using _Base = _Move_ctor_alias<_Types...>;
    using _Base::_Base;
};

template<typename... _Types>
using _Copy_assign_alias =
    _Copy_assign_base<_Traits<_Types...>::_S_trivial_copy_assign, _Types...>;

template<bool, typename... _Types>
struct _Move_assign_base : _Copy_assign_alias<_Types...> {
    using _Base = _Copy_assign_alias<_Types...>;
    using _Base::_Base;

    constexpr _Move_assign_base&
    operator=(_Move_assign_base&& __rhs)
    noexcept(_Traits<_Types...>::_S_nothrow_move_assign)
    {
        __variant::__raw_idx_visit(
        [this](auto&& __rhs_mem, auto __rhs_index) mutable {
            if constexpr (__rhs_index != variant_npos)
            {
                if (this->_M_index == __rhs_index) {
                    __variant::__get<__rhs_index>(*this) = std::move(__rhs_mem);
                } else
                    __variant_cast<_Types...>(*this)
                    .template emplace<__rhs_index>(std::move(__rhs_mem));
            } else
            {
                this->_M_reset();
            }
        }, __variant_cast<_Types...>(__rhs));
        return *this;
    }

    _Move_assign_base(const _Move_assign_base&) = default;
    _Move_assign_base(_Move_assign_base&&) = default;
    _Move_assign_base& operator=(const _Move_assign_base&) = default;
};

template<typename... _Types>
struct _Move_assign_base<true, _Types...> : _Copy_assign_alias<_Types...> {
    using _Base = _Copy_assign_alias<_Types...>;
    using _Base::_Base;
};

template<typename... _Types>
using _Move_assign_alias =
    _Move_assign_base<_Traits<_Types...>::_S_trivial_move_assign, _Types...>;

template<typename... _Types>
struct _Variant_base : _Move_assign_alias<_Types...> {
    using _Base = _Move_assign_alias<_Types...>;

    constexpr
    _Variant_base()
    noexcept(_Traits<_Types...>::_S_nothrow_default_ctor)
        : _Variant_base(in_place_index<0>) { }

    template<size_t _Np, typename... _Args>
    constexpr explicit
    _Variant_base(in_place_index_t<_Np> __i, _Args&&... __args)
        : _Base(__i, std::forward<_Args>(__args)...)
    { }

    _Variant_base(const _Variant_base&) = default;
    _Variant_base(_Variant_base&&) = default;
    _Variant_base& operator=(const _Variant_base&) = default;
    _Variant_base& operator=(_Variant_base&&) = default;
};

// For how many times does _Tp appear in _Tuple?
template<typename _Tp, typename _Tuple>
struct __tuple_count;

template<typename _Tp, typename _Tuple>
inline constexpr size_t __tuple_count_v =
    __tuple_count<_Tp, _Tuple>::value;

template<typename _Tp, typename... _Types>
struct __tuple_count<_Tp, tuple<_Types...>>
            : integral_constant<size_t, 0> { };

template<typename _Tp, typename _First, typename... _Rest>
struct __tuple_count<_Tp, tuple<_First, _Rest...>>
            : integral_constant<
              size_t,
              __tuple_count_v<_Tp, tuple<_Rest...>> + is_same_v<_Tp, _First>> { };

// TODO: Reuse this in <tuple> ?
template<typename _Tp, typename... _Types>
inline constexpr bool __exactly_once =
    __tuple_count_v<_Tp, tuple<_Types...>> == 1;

// Helper used to check for valid conversions that don't involve narrowing.
template<typename _Ti> struct _Arr {
    _Ti _M_x[1];
};

// Build an imaginary function FUN(Ti) for each alternative type Ti
template<size_t _Ind, typename _Tp, typename _Ti,
         bool _Ti_is_cv_bool = is_same_v<remove_cv_t<_Ti>, bool>,
         typename = void>
struct _Build_FUN {
    // This function means 'using _Build_FUN<I, T, Ti>::_S_fun;' is valid,
    // but only static functions will be considered in the call below.
    void _S_fun();
};

// ... for which Ti x[] = {std::forward<T>(t)}; is well-formed,
template<size_t _Ind, typename _Tp, typename _Ti>
struct _Build_FUN<_Ind, _Tp, _Ti, false,
       void_t<decltype(_Arr<_Ti> {
                           {
                               std::declval<_Tp>()
                           }
})>> {
    // This is the FUN function for type _Ti, with index _Ind
    static integral_constant<size_t, _Ind> _S_fun(_Ti);
};

// ... and if Ti is cv bool, remove_cvref_t<T> is bool.
template<size_t _Ind, typename _Tp, typename _Ti>
struct _Build_FUN<_Ind, _Tp, _Ti, true,
           enable_if_t<is_same_v<__remove_cvref_t<_Tp>, bool>>> {
    // This is the FUN function for when _Ti is cv bool, with index _Ind
    static integral_constant<size_t, _Ind> _S_fun(_Ti);
};

template<typename _Tp, typename _Variant,
         typename = make_index_sequence<variant_size_v<_Variant>>>
                                        struct _Build_FUNs;

         template<typename _Tp, typename... _Ti, size_t... _Ind>
         struct _Build_FUNs<_Tp, variant<_Ti...>, index_sequence<_Ind...>>
: _Build_FUN<_Ind, _Tp, _Ti>... {
    using _Build_FUN<_Ind, _Tp, _Ti>::_S_fun...;
};

// The index j of the overload FUN(Tj) selected by overload resolution
// for FUN(std::forward<_Tp>(t))
template<typename _Tp, typename _Variant>
using _FUN_type
= decltype(_Build_FUNs<_Tp, _Variant>::_S_fun(std::declval<_Tp>()));

// The index selected for FUN(std::forward<T>(t)), or variant_npos if none.
template<typename _Tp, typename _Variant, typename = void>
struct __accepted_index
: integral_constant<size_t, variant_npos> {
};

template<typename _Tp, typename _Variant>
struct __accepted_index<_Tp, _Variant, void_t<_FUN_type<_Tp, _Variant>>>
: _FUN_type<_Tp, _Variant> {
};

// Returns the raw storage for __v.
template<typename _Variant>
void* __get_storage(_Variant&& __v) noexcept
{
    return __v._M_storage();
}

template <typename _Maybe_variant_cookie, typename _Variant>
struct _Extra_visit_slot_needed {
    template <typename> struct _Variant_never_valueless;

    template <typename... _Types>
    struct _Variant_never_valueless<variant<_Types...>>
: bool_constant<__variant::__never_valueless<_Types...>()> {};

    static constexpr bool value =
        (is_same_v<_Maybe_variant_cookie, __variant_cookie>
         || is_same_v<_Maybe_variant_cookie, __variant_idx_cookie>)
        && !_Variant_never_valueless<__remove_cvref_t<_Variant>>::value;
};

// Used for storing a multi-dimensional vtable.
template<typename _Tp, size_t... _Dimensions>
struct _Multi_array;

// Partial specialization with rank zero, stores a single _Tp element.
template<typename _Tp>
struct _Multi_array<_Tp> {
    template<typename>
    struct __untag_result
: false_type {
        using element_type = _Tp;
    };

    template <typename... _Args>
    struct __untag_result<const void(*)(_Args...)>
: false_type {
        using element_type = void(*)(_Args...);
    };

    template <typename... _Args>
    struct __untag_result<__variant_cookie(*)(_Args...)>
: false_type {
        using element_type = void(*)(_Args...);
    };

    template <typename... _Args>
    struct __untag_result<__variant_idx_cookie(*)(_Args...)>
: false_type {
        using element_type = void(*)(_Args...);
    };

    template <typename _Res, typename... _Args>
    struct __untag_result<__deduce_visit_result<_Res>(*)(_Args...)>
: true_type {
        using element_type = _Res(*)(_Args...);
    };

    using __result_is_deduced = __untag_result<_Tp>;

    constexpr const typename __untag_result<_Tp>::element_type&
    _M_access() const
    {
        return _M_data;
    }

    typename __untag_result<_Tp>::element_type _M_data;
};

// Partial specialization with rank >= 1.
template<typename _Ret,
         typename _Visitor,
         typename... _Variants,
         size_t __first, size_t... __rest>
struct _Multi_array<_Ret(*)(_Visitor, _Variants...), __first, __rest...> {
    static constexpr size_t __index =
    sizeof...(_Variants) - sizeof...(__rest) - 1;

    using _Variant = typename _Nth_type<__index, _Variants...>::type;

    static constexpr int __do_cookie =
        _Extra_visit_slot_needed<_Ret, _Variant>::value ? 1 : 0;

    using _Tp = _Ret(*)(_Visitor, _Variants...);

    template<typename... _Args>
    constexpr decltype(auto)
    _M_access(size_t __first_index, _Args... __rest_indices) const
    {
        return _M_arr[__first_index + __do_cookie]
        ._M_access(__rest_indices...);
    }

    _Multi_array<_Tp, __rest...> _M_arr[__first + __do_cookie];
};

// Creates a multi-dimensional vtable recursively.
//
// For example,
// visit([](auto, auto){},
//       variant<int, char>(),  // typedef'ed as V1
//       variant<float, double, long double>())  // typedef'ed as V2
// will trigger instantiations of:
// __gen_vtable_impl<_Multi_array<void(*)(V1&&, V2&&), 2, 3>,
//                   tuple<V1&&, V2&&>, std::index_sequence<>>
//   __gen_vtable_impl<_Multi_array<void(*)(V1&&, V2&&), 3>,
//                     tuple<V1&&, V2&&>, std::index_sequence<0>>
//     __gen_vtable_impl<_Multi_array<void(*)(V1&&, V2&&)>,
//                       tuple<V1&&, V2&&>, std::index_sequence<0, 0>>
//     __gen_vtable_impl<_Multi_array<void(*)(V1&&, V2&&)>,
//                       tuple<V1&&, V2&&>, std::index_sequence<0, 1>>
//     __gen_vtable_impl<_Multi_array<void(*)(V1&&, V2&&)>,
//                       tuple<V1&&, V2&&>, std::index_sequence<0, 2>>
//   __gen_vtable_impl<_Multi_array<void(*)(V1&&, V2&&), 3>,
//                     tuple<V1&&, V2&&>, std::index_sequence<1>>
//     __gen_vtable_impl<_Multi_array<void(*)(V1&&, V2&&)>,
//                       tuple<V1&&, V2&&>, std::index_sequence<1, 0>>
//     __gen_vtable_impl<_Multi_array<void(*)(V1&&, V2&&)>,
//                       tuple<V1&&, V2&&>, std::index_sequence<1, 1>>
//     __gen_vtable_impl<_Multi_array<void(*)(V1&&, V2&&)>,
//                       tuple<V1&&, V2&&>, std::index_sequence<1, 2>>
// The returned multi-dimensional vtable can be fast accessed by the visitor
// using index calculation.
template<typename _Array_type, typename _Index_seq>
struct __gen_vtable_impl;

// Defines the _S_apply() member that returns a _Multi_array populated
// with function pointers that perform the visitation expressions e(m)
// for each valid pack of indexes into the variant types _Variants.
//
// This partial specialization builds up the index sequences by recursively
// calling _S_apply() on the next specialization of __gen_vtable_impl.
// The base case of the recursion defines the actual function pointers.
template<typename _Result_type, typename _Visitor, size_t... __dimensions,
         typename... _Variants, size_t... __indices>
struct __gen_vtable_impl<
    _Multi_array<_Result_type (*)(_Visitor, _Variants...), __dimensions...>,
    std::index_sequence<__indices...>> {
    using _Next =
    remove_reference_t<typename _Nth_type<sizeof...(__indices),
                                          _Variants...>::type>;
    using _Array_type =
        _Multi_array<_Result_type (*)(_Visitor, _Variants...),
        __dimensions...>;

    static constexpr _Array_type
    _S_apply()
    {
        _Array_type __vtable{};
        _S_apply_all_alts(
            __vtable, make_index_sequence<variant_size_v<_Next>>());
        return __vtable;
    }

    template<size_t... __var_indices>
    static constexpr void
    _S_apply_all_alts(_Array_type& __vtable,
                      std::index_sequence<__var_indices...>)
    {
        if constexpr (_Extra_visit_slot_needed<_Result_type, _Next>::value)
            (_S_apply_single_alt<true, __var_indices>(
                 __vtable._M_arr[__var_indices + 1],
                 &(__vtable._M_arr[0])), ...);
        else
            (_S_apply_single_alt<false, __var_indices>(
                        __vtable._M_arr[__var_indices]), ...);
    }

    template<bool __do_cookie, size_t __index, typename _Tp>
    static constexpr void
    _S_apply_single_alt(_Tp& __element, _Tp* __cookie_element = nullptr)
    {
        if constexpr (__do_cookie) {
            __element = __gen_vtable_impl<
                _Tp,
                std::index_sequence<__indices..., __index>>::_S_apply();
            *__cookie_element = __gen_vtable_impl<
                                _Tp,
                                std::index_sequence<__indices..., variant_npos>>::_S_apply();
        } else {
            __element = __gen_vtable_impl<
                        remove_reference_t<decltype(__element)>,
                        std::index_sequence<__indices..., __index>>::_S_apply();
        }
    }
};

// This partial specialization is the base case for the recursion.
// It populates a _Multi_array element with the address of a function
// that invokes the visitor with the alternatives specified by __indices.
template<typename _Result_type, typename _Visitor, typename... _Variants,
         size_t... __indices>
struct __gen_vtable_impl<
    _Multi_array<_Result_type (*)(_Visitor, _Variants...)>,
    std::index_sequence<__indices...>> {
    using _Array_type =
    _Multi_array<_Result_type (*)(_Visitor, _Variants...)>;

    template<size_t __index, typename _Variant>
    static constexpr decltype(auto)
    __element_by_index_or_cookie(_Variant&& __var) noexcept
    {
        if constexpr (__index != variant_npos) {
            return __variant::__get<__index>(std::forward<_Variant>(__var));
        } else
            return __variant_cookie{};
    }

    static constexpr decltype(auto)
    __visit_invoke(_Visitor&& __visitor, _Variants... __vars)
    {
        if constexpr (is_same_v<_Result_type, __variant_idx_cookie>)
            // For raw visitation using indices, pass the indices to the visitor
            // and discard the return value:
            std::__invoke(std::forward<_Visitor>(__visitor),
                          __element_by_index_or_cookie<__indices>(
                              std::forward<_Variants>(__vars))...,
                          integral_constant<size_t, __indices>()...);
        else if constexpr (is_same_v<_Result_type, __variant_cookie>)
            // For raw visitation without indices, and discard the return value:
            std::__invoke(std::forward<_Visitor>(__visitor),
                          __element_by_index_or_cookie<__indices>(
                              std::forward<_Variants>(__vars))...);
        else if constexpr (_Array_type::__result_is_deduced::value)
            // For the usual std::visit case deduce the return value:
            return std::__invoke(std::forward<_Visitor>(__visitor),
                                 __element_by_index_or_cookie<__indices>(
                                     std::forward<_Variants>(__vars))...);
        else // for std::visit<R> use INVOKE<R>
            return std::__invoke_r<_Result_type>(
                       std::forward<_Visitor>(__visitor),
                       __variant::__get<__indices>(std::forward<_Variants>(__vars))...);
    }

    static constexpr auto
    _S_apply()
    {
        return _Array_type{&__visit_invoke};
    }
};

template<typename _Result_type, typename _Visitor, typename... _Variants>
struct __gen_vtable {
    using _Array_type =
    _Multi_array<_Result_type (*)(_Visitor, _Variants...),
                 variant_size_v<remove_reference_t<_Variants>>...>;

    static constexpr _Array_type _S_vtable
        = __gen_vtable_impl<_Array_type, std::index_sequence<>>::_S_apply();
};

template<size_t _Np, typename _Tp>
struct _Base_dedup : public _Tp { };

template<typename _Variant, typename __indices>
struct _Variant_hash_base;

template<typename... _Types, size_t... __indices>
struct _Variant_hash_base<variant<_Types...>,
                          std::index_sequence<__indices...>>
: _Base_dedup<__indices, __poison_hash<remove_const_t<_Types>>>... { };

} // namespace __variant
} // namespace __detail

template<size_t _Np, typename _Variant, typename... _Args>
constexpr void __variant_construct_by_index(_Variant& __v, _Args&&... __args) {
    std::construct_at(std::addressof(__v), 
        in_place_index<_Np>,
        std::forward<_Args>(__args)...);
}

template<typename _Tp, typename... _Types>
constexpr bool
holds_alternative(const variant<_Types...>& __v) noexcept {
    static_assert(__detail::__variant::__exactly_once<_Tp, _Types...>,
                  "T must occur exactly once in alternatives");
    return __v.index() == __detail::__variant::__index_of_v<_Tp, _Types...>;
}

template<typename _Tp, typename... _Types>
constexpr _Tp& get(variant<_Types...>& __v) {
    static_assert(__detail::__variant::__exactly_once<_Tp, _Types...>,
                  "T must occur exactly once in alternatives");
    static_assert(!is_void_v<_Tp>, "_Tp must not be void");
    return std::get<__detail::__variant::__index_of_v<_Tp, _Types...>>(__v);
}

template<typename _Tp, typename... _Types>
constexpr _Tp&& get(variant<_Types...>&& __v) {
    static_assert(__detail::__variant::__exactly_once<_Tp, _Types...>,
                  "T must occur exactly once in alternatives");
    static_assert(!is_void_v<_Tp>, "_Tp must not be void");
    return std::get<__detail::__variant::__index_of_v<_Tp, _Types...>>(
               std::move(__v));
}

template<typename _Tp, typename... _Types>
constexpr const _Tp& get(const variant<_Types...>& __v) {
    static_assert(__detail::__variant::__exactly_once<_Tp, _Types...>,
                  "T must occur exactly once in alternatives");
    static_assert(!is_void_v<_Tp>, "_Tp must not be void");
    return std::get<__detail::__variant::__index_of_v<_Tp, _Types...>>(__v);
}

template<typename _Tp, typename... _Types>
constexpr const _Tp&& get(const variant<_Types...>&& __v) {
    static_assert(__detail::__variant::__exactly_once<_Tp, _Types...>,
                  "T must occur exactly once in alternatives");
    static_assert(!is_void_v<_Tp>, "_Tp must not be void");
    return std::get<__detail::__variant::__index_of_v<_Tp, _Types...>>(
               std::move(__v));
}

template<size_t _Np, typename... _Types>
constexpr add_pointer_t<variant_alternative_t<_Np, variant<_Types...>>>
get_if(variant<_Types...>* __ptr) noexcept {
    using _Alternative_type = variant_alternative_t<_Np, variant<_Types...>>;
    static_assert(_Np < sizeof...(_Types),
                  "The index must be in [0, number of alternatives)");
    static_assert(!is_void_v<_Alternative_type>, "_Tp must not be void");
    if (__ptr && __ptr->index() == _Np) {
        return std::addressof(__detail::__variant::__get<_Np>(*__ptr));
    }
    return nullptr;
}

template<size_t _Np, typename... _Types>
constexpr
add_pointer_t<const variant_alternative_t<_Np, variant<_Types...>>>
get_if(const variant<_Types...>* __ptr) noexcept {
    using _Alternative_type = variant_alternative_t<_Np, variant<_Types...>>;
    static_assert(_Np < sizeof...(_Types),
                  "The index must be in [0, number of alternatives)");
    static_assert(!is_void_v<_Alternative_type>, "_Tp must not be void");
    if (__ptr && __ptr->index() == _Np) {
        return std::addressof(__detail::__variant::__get<_Np>(*__ptr));
    }
    return nullptr;
}

template<typename _Tp, typename... _Types>
constexpr add_pointer_t<_Tp>
get_if(variant<_Types...>* __ptr) noexcept {
    static_assert(__detail::__variant::__exactly_once<_Tp, _Types...>,
                  "T must occur exactly once in alternatives");
    static_assert(!is_void_v<_Tp>, "_Tp must not be void");
    return std::get_if<__detail::__variant::__index_of_v<_Tp, _Types...>>(
               __ptr);
}

template<typename _Tp, typename... _Types>
constexpr add_pointer_t<const _Tp>
get_if(const variant<_Types...>* __ptr) noexcept {
    static_assert(__detail::__variant::__exactly_once<_Tp, _Types...>,
                  "T must occur exactly once in alternatives");
    static_assert(!is_void_v<_Tp>, "_Tp must not be void");
    return std::get_if<__detail::__variant::__index_of_v<_Tp, _Types...>>(
               __ptr);
}

struct monostate { };

#define _VARIANT_RELATION_FUNCTION_TEMPLATE(__OP, __NAME) \
  template<typename... _Types> \
    constexpr bool operator __OP(const variant<_Types...>& __lhs, \
				 const variant<_Types...>& __rhs) \
    { \
      bool __ret = true; \
      __detail::__variant::__raw_idx_visit( \
        [&__ret, &__lhs] (auto&& __rhs_mem, auto __rhs_index) mutable \
        { \
	  if constexpr (__rhs_index != variant_npos) \
	    { \
	      if (__lhs.index() == __rhs_index) \
	        { \
		  auto& __this_mem = std::get<__rhs_index>(__lhs);	\
                  __ret = __this_mem __OP __rhs_mem; \
                } \
	      else \
		__ret = (__lhs.index() + 1) __OP (__rhs_index + 1); \
            } \
          else \
            __ret = (__lhs.index() + 1) __OP (__rhs_index + 1); \
	}, __rhs); \
      return __ret; \
    }

_VARIANT_RELATION_FUNCTION_TEMPLATE(<, less)
_VARIANT_RELATION_FUNCTION_TEMPLATE(<=, less_equal)
_VARIANT_RELATION_FUNCTION_TEMPLATE(==, equal)
_VARIANT_RELATION_FUNCTION_TEMPLATE(!=, not_equal)
_VARIANT_RELATION_FUNCTION_TEMPLATE(>=, greater_equal)
_VARIANT_RELATION_FUNCTION_TEMPLATE(>, greater)

#undef _VARIANT_RELATION_FUNCTION_TEMPLATE

constexpr bool operator==(monostate, monostate) noexcept {
    return true;
}

#ifdef __cpp_lib_three_way_comparison
template<typename... _Types>
requires (three_way_comparable<_Types> && ...)
constexpr
common_comparison_category_t<compare_three_way_result_t<_Types>...>
operator<=>(const variant<_Types...>& __v, const variant<_Types...>& __w) {
    common_comparison_category_t<compare_three_way_result_t<_Types>...> __ret
    = strong_ordering::equal;

    __detail::__variant::__raw_idx_visit(
    [&__ret, &__v] (auto&& __w_mem, auto __w_index) mutable {
        if constexpr (__w_index != variant_npos)
        {
            if (__v.index() == __w_index) {
                auto& __this_mem = std::get<__w_index>(__v);
                __ret = __this_mem <=> __w_mem;
                return;
            }
        }
        __ret = (__v.index() + 1) <=> (__w_index + 1);
    }, __w);
    return __ret;
}

constexpr strong_ordering
operator<=>(monostate, monostate) noexcept {
    return strong_ordering::equal;
}
#else
constexpr bool operator!=(monostate, monostate) noexcept {
    return false;
}
constexpr bool operator<(monostate, monostate) noexcept {
    return false;
}
constexpr bool operator>(monostate, monostate) noexcept {
    return false;
}
constexpr bool operator<=(monostate, monostate) noexcept {
    return true;
}
constexpr bool operator>=(monostate, monostate) noexcept {
    return true;
}
#endif

template<typename _Visitor, typename... _Variants>
constexpr decltype(auto) visit(_Visitor&&, _Variants&&...);

template<typename... _Types>
constexpr
inline enable_if_t<(is_move_constructible_v<_Types> && ...)
                   && (is_swappable_v<_Types> && ...)>
swap(variant<_Types...>& __lhs, variant<_Types...>& __rhs)
noexcept(noexcept(__lhs.swap(__rhs))) {
    __lhs.swap(__rhs);
}

template<typename... _Types>
enable_if_t<!((is_move_constructible_v<_Types> && ...)
              && (is_swappable_v<_Types> && ...))>
swap(variant<_Types...>&, variant<_Types...>&) = delete;

class bad_variant_access : public exception
{
public:
    bad_variant_access() noexcept { }

    const char* what() const noexcept override
    {
        return _M_reason;
    }

private:
    bad_variant_access(const char* __reason) noexcept : _M_reason(__reason) { }

    // Must point to a string with static storage duration:
    const char* _M_reason = "bad variant access";

    friend void __throw_bad_variant_access(const char* __what);
};

// Must only be called with a string literal
inline void
__throw_bad_variant_access(const char* __what) {
    _GLIBCXX_THROW_OR_ABORT(bad_variant_access(__what));
}

inline void
__throw_bad_variant_access(bool __valueless) {
    if (__valueless) [[__unlikely__]]
        __throw_bad_variant_access("std::get: variant is valueless");
    else {
        __throw_bad_variant_access("std::get: wrong index for variant");
    }
}

template<typename... _Types>
class variant
: private __detail::__variant::_Variant_base<_Types...>,
private _Enable_default_constructor<
    __detail::__variant::_Traits<_Types...>::_S_default_ctor,
    variant<_Types...>>,
private _Enable_copy_move<
    __detail::__variant::_Traits<_Types...>::_S_copy_ctor,
    __detail::__variant::_Traits<_Types...>::_S_copy_assign,
    __detail::__variant::_Traits<_Types...>::_S_move_ctor,
    __detail::__variant::_Traits<_Types...>::_S_move_assign,
    variant<_Types...>>
{
private:
    template <typename... _UTypes, typename _Tp>
    friend constexpr decltype(auto) __variant_cast(_Tp&&);
    template<size_t _Np, typename _Variant, typename... _Args>
    friend constexpr void __variant_construct_by_index(_Variant& __v,
            _Args&&... __args);

    static_assert(sizeof...(_Types) > 0,
                  "variant must have at least one alternative");
    static_assert(!(std::is_reference_v<_Types> || ...),
                  "variant must have no reference alternative");
    static_assert(!(std::is_void_v<_Types> || ...),
                  "variant must have no void alternative");

    using _Base = __detail::__variant::_Variant_base<_Types...>;
    using _Default_ctor_enabler =
        _Enable_default_constructor<
        __detail::__variant::_Traits<_Types...>::_S_default_ctor,
        variant<_Types...>>;

    template<typename _Tp>
    static constexpr bool __not_self
    = !is_same_v<__remove_cvref_t<_Tp>, variant>;

    template<typename _Tp>
    static constexpr bool
    __exactly_once = __detail::__variant::__exactly_once<_Tp, _Types...>;

    template<typename _Tp>
    static constexpr size_t __accepted_index
    = __detail::__variant::__accepted_index<_Tp, variant>::value;

    template<size_t _Np, typename = enable_if_t<(_Np < sizeof...(_Types))>>
             using __to_type = variant_alternative_t<_Np, variant>;

    template<typename _Tp, typename = enable_if_t<__not_self<_Tp>>>
             using __accepted_type = __to_type<__accepted_index<_Tp>>;

             template<typename _Tp>
             static constexpr size_t __index_of =
             __detail::__variant::__index_of_v<_Tp, _Types...>;

             using _Traits = __detail::__variant::_Traits<_Types...>;

             template<typename _Tp>
         struct __is_in_place_tag : false_type { };
             template<typename _Tp>
         struct __is_in_place_tag<in_place_type_t<_Tp>> : true_type { };
             template<size_t _Np>
         struct __is_in_place_tag<in_place_index_t<_Np>> : true_type { };

             template<typename _Tp>
             static constexpr bool __not_in_place_tag
             = !__is_in_place_tag<__remove_cvref_t<_Tp>>::value;

         public:
             variant() = default;
             variant(const variant& __rhs) = default;
             variant(variant&&) = default;
             variant& operator=(const variant&) = default;
             variant& operator=(variant&&) = default;
             ~variant() = default;

             template<typename _Tp,
                      typename = enable_if_t<sizeof...(_Types) != 0>,
                      typename = enable_if_t<__not_in_place_tag<_Tp>>,
                      typename _Tj = __accepted_type<_Tp&&>,
                      typename = enable_if_t<__exactly_once<_Tj>
                              && is_constructible_v<_Tj, _Tp>>>
                              constexpr
                              variant(_Tp&& __t)
                              noexcept(is_nothrow_constructible_v<_Tj, _Tp>)
                                  : variant(in_place_index<__accepted_index<_Tp>>,
                                            std::forward<_Tp>(__t))
    { }

    template<typename _Tp, typename... _Args,
             typename = enable_if_t<__exactly_once<_Tp>
                                    && is_constructible_v<_Tp, _Args...>>>
                                    constexpr explicit
                                    variant(in_place_type_t<_Tp>, _Args&&... __args)
                                        : variant(in_place_index<__index_of<_Tp>>,
                                                std::forward<_Args>(__args)...)
    { }

    template<typename _Tp, typename _Up, typename... _Args,
             typename = enable_if_t<__exactly_once<_Tp>
                                    && is_constructible_v<_Tp,
                                            initializer_list<_Up>&, _Args...>>>
                                    constexpr explicit
                                    variant(in_place_type_t<_Tp>, initializer_list<_Up> __il,
                                            _Args&&... __args)
                                        : variant(in_place_index<__index_of<_Tp>>, __il,
                                                std::forward<_Args>(__args)...)
    { }

    template<size_t _Np, typename... _Args,
             typename _Tp = __to_type<_Np>,
             typename = enable_if_t<is_constructible_v<_Tp, _Args...>>>
                                    constexpr explicit
                                    variant(in_place_index_t<_Np>, _Args&&... __args)
                                        : _Base(in_place_index<_Np>, std::forward<_Args>(__args)...),
                                          _Default_ctor_enabler(_Enable_default_constructor_tag{})
    { }

    template<size_t _Np, typename _Up, typename... _Args,
             typename _Tp = __to_type<_Np>,
             typename = enable_if_t<is_constructible_v<_Tp,
                                    initializer_list<_Up>&,
                                    _Args...>>>
                                    constexpr explicit
                                    variant(in_place_index_t<_Np>, initializer_list<_Up> __il,
                                            _Args&&... __args)
                                        : _Base(in_place_index<_Np>, __il, std::forward<_Args>(__args)...),
                                          _Default_ctor_enabler(_Enable_default_constructor_tag{})
    { }

    template<typename _Tp>
    constexpr
    enable_if_t<__exactly_once<__accepted_type<_Tp&&>>
                && is_constructible_v<__accepted_type<_Tp&&>, _Tp>
                && is_assignable_v<__accepted_type<_Tp&&>&, _Tp>,
                variant&>
    operator=(_Tp&& __rhs)
    noexcept(is_nothrow_assignable_v<__accepted_type<_Tp&&>&, _Tp>
             && is_nothrow_constructible_v<__accepted_type<_Tp&&>, _Tp>)
    {
        constexpr auto __index = __accepted_index<_Tp>;
        if (index() == __index) {
            std::get<__index>(*this) = std::forward<_Tp>(__rhs);
        } else {
            using _Tj = __accepted_type<_Tp&&>;
            if constexpr (is_nothrow_constructible_v<_Tj, _Tp>
                          || !is_nothrow_move_constructible_v<_Tj>) {
                this->emplace<__index>(std::forward<_Tp>(__rhs));
            } else {
                operator=(variant(std::forward<_Tp>(__rhs)));
            }
        }
        return *this;
    }

    template<typename _Tp, typename... _Args>
    constexpr
    enable_if_t<is_constructible_v<_Tp, _Args...> && __exactly_once<_Tp>,
                _Tp&>
    emplace(_Args&&... __args)
    {
        constexpr size_t __index = __index_of<_Tp>;
        return this->emplace<__index>(std::forward<_Args>(__args)...);
    }

    template<typename _Tp, typename _Up, typename... _Args>
    constexpr
    enable_if_t<is_constructible_v<_Tp, initializer_list<_Up>&, _Args...>
                && __exactly_once<_Tp>,
                _Tp&>
    emplace(initializer_list<_Up> __il, _Args&&... __args)
    {
        constexpr size_t __index = __index_of<_Tp>;
        return this->emplace<__index>(__il, std::forward<_Args>(__args)...);
    }

    template<size_t _Np, typename... _Args>
    constexpr
    enable_if_t<is_constructible_v<variant_alternative_t<_Np, variant>,
                                   _Args...>,
                variant_alternative_t<_Np, variant>&>
    emplace(_Args&&... __args)
    {
        static_assert(_Np < sizeof...(_Types),
                            "The index must be in [0, number of alternatives)");
        using type = variant_alternative_t<_Np, variant>;
        // Provide the strong exception-safety guarantee when possible,
        // to avoid becoming valueless.
        if constexpr (is_nothrow_constructible_v<type, _Args...>) {
            this->_M_reset();
            __variant_construct_by_index<_Np>(*this,
                                              std::forward<_Args>(__args)...);
        } else if constexpr (is_scalar_v<type>) {
            // This might invoke a potentially-throwing conversion operator:
            const type __tmp(std::forward<_Args>(__args)...);
            // But these steps won't throw:
            this->_M_reset();
            __variant_construct_by_index<_Np>(*this, __tmp);
        } else if constexpr (__detail::__variant::_Never_valueless_alt<type>()
                             && _Traits::_S_move_assign) {
            // This construction might throw:
            variant __tmp(in_place_index<_Np>,
                          std::forward<_Args>(__args)...);
            // But _Never_valueless_alt<type> means this won't:
            *this = std::move(__tmp);
        } else {
            // This case only provides the basic exception-safety guarantee,
            // i.e. the variant can become valueless.
            this->_M_reset();
            __try {
                __variant_construct_by_index<_Np>(*this,
                                                  std::forward<_Args>(__args)...);
            }
            __catch (...) {
                this->_M_index = variant_npos;
                __throw_exception_again;
            }
        }
        return std::get<_Np>(*this);
    }

    template<size_t _Np, typename _Up, typename... _Args>
    constexpr
    enable_if_t<is_constructible_v<variant_alternative_t<_Np, variant>,
                                   initializer_list<_Up>&, _Args...>,
                variant_alternative_t<_Np, variant>&>
    emplace(initializer_list<_Up> __il, _Args&&... __args)
    {
        static_assert(_Np < sizeof...(_Types),
                            "The index must be in [0, number of alternatives)");
        using type = variant_alternative_t<_Np, variant>;
        // Provide the strong exception-safety guarantee when possible,
        // to avoid becoming valueless.
        if constexpr (is_nothrow_constructible_v<type,
                      initializer_list<_Up>&,
                      _Args...>) {
            this->_M_reset();
            __variant_construct_by_index<_Np>(*this, __il,
                                              std::forward<_Args>(__args)...);
        } else if constexpr (__detail::__variant::_Never_valueless_alt<type>()
                             && _Traits::_S_move_assign) {
            // This construction might throw:
            variant __tmp(in_place_index<_Np>, __il,
                          std::forward<_Args>(__args)...);
            // But _Never_valueless_alt<type> means this won't:
            *this = std::move(__tmp);
        } else {
            // This case only provides the basic exception-safety guarantee,
            // i.e. the variant can become valueless.
            this->_M_reset();
            __try {
                __variant_construct_by_index<_Np>(*this, __il,
                                                  std::forward<_Args>(__args)...);
            }
            __catch (...) {
                this->_M_index = variant_npos;
                __throw_exception_again;
            }
        }
        return std::get<_Np>(*this);
    }

    constexpr bool valueless_by_exception() const noexcept
    {
        return !this->_M_valid();
    }

    constexpr size_t index() const noexcept
    {
        using __index_type = typename _Base::__index_type;
        if constexpr (__detail::__variant::__never_valueless<_Types...>()) {
            return this->_M_index;
        } else if constexpr (sizeof...(_Types) <= __index_type(-1) / 2) {
            return make_signed_t<__index_type>(this->_M_index);
        } else {
            return size_t(__index_type(this->_M_index + 1)) - 1;
        }
    }

    constexpr void
    swap(variant& __rhs)
    noexcept((__is_nothrow_swappable<_Types>::value && ...)
             && is_nothrow_move_constructible_v<variant>)
    {
        __detail::__variant::__raw_idx_visit(
        [this, &__rhs](auto&& __rhs_mem, auto __rhs_index) mutable {
            if constexpr (__rhs_index != variant_npos)
            {
                if (this->index() == __rhs_index) {
                    auto& __this_mem =
                    std::get<__rhs_index>(*this);
                    using std::swap;
                    swap(__this_mem, __rhs_mem);
                } else {
                    if (!this->valueless_by_exception()) [[__likely__]] {
                        auto __tmp(std::move(__rhs_mem));
                        __rhs = std::move(*this);
                        this->_M_destructive_move(__rhs_index,
                                                  std::move(__tmp));
                    } else {
                        this->_M_destructive_move(__rhs_index,
                                                  std::move(__rhs_mem));
                        __rhs._M_reset();
                    }
                }
            } else
            {
                if (!this->valueless_by_exception()) [[__likely__]] {
                    __rhs = std::move(*this);
                    this->_M_reset();
                }
            }
        }, __rhs);
    }

private:

#if defined(__clang__) && __clang_major__ <= 7
public:
    using _Base::_M_u; // See https://bugs.llvm.org/show_bug.cgi?id=31852
private:
#endif

    template<size_t _Np, typename _Vp>
    friend constexpr decltype(auto)
    __detail::__variant::__get(_Vp&& __v) noexcept;

    template<typename _Vp>
    friend void*
    __detail::__variant::__get_storage(_Vp&& __v) noexcept;

#define _VARIANT_RELATION_FUNCTION_TEMPLATE(__OP) \
      template<typename... _Tp> \
	friend constexpr bool \
	operator __OP(const variant<_Tp...>& __lhs, \
		      const variant<_Tp...>& __rhs);

    _VARIANT_RELATION_FUNCTION_TEMPLATE(<)
    _VARIANT_RELATION_FUNCTION_TEMPLATE(<=)
    _VARIANT_RELATION_FUNCTION_TEMPLATE(==)
    _VARIANT_RELATION_FUNCTION_TEMPLATE(!=)
    _VARIANT_RELATION_FUNCTION_TEMPLATE(>=)
    _VARIANT_RELATION_FUNCTION_TEMPLATE(>)

#undef _VARIANT_RELATION_FUNCTION_TEMPLATE
};

template<size_t _Np, typename... _Types>
constexpr variant_alternative_t<_Np, variant<_Types...>>&
get(variant<_Types...>& __v) {
    static_assert(_Np < sizeof...(_Types),
                        "The index must be in [0, number of alternatives)");
    if (__v.index() != _Np) {
        __throw_bad_variant_access(__v.valueless_by_exception());
    }
    return __detail::__variant::__get<_Np>(__v);
}

template<size_t _Np, typename... _Types>
constexpr variant_alternative_t<_Np, variant<_Types...>>&&
get(variant<_Types...>&& __v) {
    static_assert(_Np < sizeof...(_Types),
                        "The index must be in [0, number of alternatives)");
    if (__v.index() != _Np) {
        __throw_bad_variant_access(__v.valueless_by_exception());
    }
    return __detail::__variant::__get<_Np>(std::move(__v));
}

template<size_t _Np, typename... _Types>
constexpr const variant_alternative_t<_Np, variant<_Types...>>&
get(const variant<_Types...>& __v) {
    static_assert(_Np < sizeof...(_Types),
                        "The index must be in [0, number of alternatives)");
    if (__v.index() != _Np) {
        __throw_bad_variant_access(__v.valueless_by_exception());
    }
    return __detail::__variant::__get<_Np>(__v);
}

template<size_t _Np, typename... _Types>
constexpr const variant_alternative_t<_Np, variant<_Types...>>&&
get(const variant<_Types...>&& __v) {
    static_assert(_Np < sizeof...(_Types),
                        "The index must be in [0, number of alternatives)");
    if (__v.index() != _Np) {
        __throw_bad_variant_access(__v.valueless_by_exception());
    }
    return __detail::__variant::__get<_Np>(std::move(__v));
}

template<typename _Result_type, typename _Visitor, typename... _Variants>
constexpr decltype(auto)
__do_visit(_Visitor&& __visitor, _Variants&&... __variants) {
    constexpr auto& __vtable = __detail::__variant::__gen_vtable<
        _Result_type, _Visitor&&, _Variants&&...>::_S_vtable;

    auto __func_ptr = __vtable._M_access(__variants.index()...);
    return (*__func_ptr)(std::forward<_Visitor>(__visitor),
                         std::forward<_Variants>(__variants)...);
}

template<typename _Visitor, typename... _Variants>
constexpr decltype(auto)
visit(_Visitor&& __visitor, _Variants&&... __variants) {
    if ((__variants.valueless_by_exception() || ...)) {
        __throw_bad_variant_access("std::visit: variant is valueless");
    }

    using _Result_type = std::invoke_result_t<_Visitor,
          decltype(std::get<0>(std::declval<_Variants>()))...>;

    using _Tag = __detail::__variant::__deduce_visit_result<_Result_type>;

    return std::__do_visit<_Tag>(std::forward<_Visitor>(__visitor),
                                 std::forward<_Variants>(__variants)...);
}

#if __cplusplus > 201703L
template<typename _Res, typename _Visitor, typename... _Variants>
constexpr _Res
visit(_Visitor&& __visitor, _Variants&&... __variants) {
    if ((__variants.valueless_by_exception() || ...)) {
        __throw_bad_variant_access("std::visit<R>: variant is valueless");
    }

    return std::__do_visit<_Res>(std::forward<_Visitor>(__visitor),
                                 std::forward<_Variants>(__variants)...);
}
#endif

template<bool, typename... _Types>
struct __variant_hash_call_base_impl {
    size_t
    operator()(const variant<_Types...>& __t) const
    noexcept((is_nothrow_invocable_v<hash<decay_t<_Types>>, _Types> && ...))
    {
        size_t __ret;
        __detail::__variant::__raw_visit(
        [&__t, &__ret](auto&& __t_mem) mutable {
            using _Type = __remove_cvref_t<decltype(__t_mem)>;
            if constexpr (!is_same_v<_Type,
                          __detail::__variant::__variant_cookie>)
                __ret = std::hash<size_t>{}(__t.index())
            + std::hash<_Type>{}(__t_mem);
            else
                __ret = std::hash<size_t>{}(__t.index());
        }, __t);
        return __ret;
    }
};

template<typename... _Types>
struct __variant_hash_call_base_impl<false, _Types...> {};

template<typename... _Types>
using __variant_hash_call_base =
__variant_hash_call_base_impl<(__poison_hash<remove_const_t<_Types>>::
                               __enable_hash_call &&...), _Types...>;

template<typename... _Types>
struct hash<variant<_Types...>>
: private __detail::__variant::_Variant_hash_base<
    variant<_Types...>, std::index_sequence_for<_Types...>>,
public __variant_hash_call_base<_Types...> {
    using result_type [[__deprecated__]] = size_t;
    using argument_type [[__deprecated__]] = variant<_Types...>;
};

template<>
struct hash<monostate> {
    using result_type [[__deprecated__]] = size_t;
    using argument_type [[__deprecated__]] = monostate;

    size_t
    operator()(const monostate&) const noexcept
    {
        constexpr size_t __magic_monostate_hash = -7777;
        return __magic_monostate_hash;
    }
};

template<typename... _Types>
struct __is_fast_hash<hash<variant<_Types...>>>
: bool_constant<(__is_fast_hash<_Types>::value && ...)> {
};

_GLIBCXX_END_NAMESPACE_VERSION
} // namespace std

#endif // C++17

#endif // _GLIBCXX_VARIANT
