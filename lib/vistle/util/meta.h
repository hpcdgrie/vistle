#ifndef VISTLE_UTIL_META_H
#define VISTLE_UTIL_META_H

#include <type_traits>
#include <cstdlib>
#include <utility>

namespace vistle {
namespace meta {

namespace detail {
// returns a function that shifts the integral constant argument by N
template<std::size_t N, class F>
auto _shift(F func)
{
    return [&, func](auto i) {
        return func(std::integral_constant<std::size_t, i() + N>{});
    };
}
} // namespace detail

// invoke func for every index in supplied integer_sequence
template<class F, std::size_t... Is>
void _for(F func, std::integer_sequence<std::size_t, Is...>)
{
    (func(std::integral_constant<std::size_t, Is>{}), ...);
}

// invoke func for indices 0..N-1
template<std::size_t N, typename F>
void _for(F func)
{
    _for(func, std::make_integer_sequence<std::size_t, N>());
}

// invoke func for indices B..E-1
template<std::size_t B, std::size_t E, typename F>
void _for(F func)
{
    _for<E - B>(detail::_shift<B>(func));
}

// Perform switch on a runtime Predicate to perform compile-time dispatch
// if Predicate matches any value in Array, Function<Array[i]> is invoked,
// if the Predicate does not match any value in Array nothing happens.
// Function is called up to once
// usage: wrap your function template with WRAP_FUNC_TEMPLATE, e.g.
// WRAP_FUNC_TEMPLATE(MyFunc)
// then call switch_ like this:
// constexpr std::array MyTypes = {Type1, Type2, Type3};
// auto result = switch_<MyFunc, MyTypes>(runtimeVariable, other, args,
// note: member functions must be public and wrapped with WRAP_MEMBER_FUNC_TEMPLATE
// e.g. WRAP_MEMBER_FUNC_TEMPLATE(MyClass, MyMemberFunc)

#define WRAP_FUNC_TEMPLATE(name) \
    template<auto Specialization> \
    struct name##_Wrapper { \
        template<typename... Args> \
        static constexpr auto call(Args &&...args) \
        { \
            return name<Specialization>(std::forward<Args>(args)...); \
        } \
    };

#define WRAP_MEMBER_FUNC_TEMPLATE(Class, name) \
    template<auto Specialization> \
    struct Class##_##name##_Wrapper { \
        template<typename... Args> \
        static constexpr auto call(Class &obj, Args &&...args) \
        { \
            return obj.template name<Specialization>(std::forward<Args>(args)...); \
        } \
    };

template<template<auto> class Function, auto Array, typename Predicate, typename... Args>
auto _switch(Predicate pred, Args &&...args)
{
    using ReturnType = decltype(Function<Array[0]>::call(std::forward<Args>(args)...));
    if constexpr (std::is_void_v<ReturnType>) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>)
        {
            ((Array[Is] == pred ? (Function<Array[Is]>::call(std::forward<Args>(args)...), true) : false) || ...);
        }
        (std::make_index_sequence<Array.size()>{});
    } else {
        ReturnType retval{};
        [&]<std::size_t... Is>(std::index_sequence<Is...>)
        {
            ((Array[Is] == pred ? (retval = Function<Array[Is]>::call(std::forward<Args>(args)...), true) : false) ||
             ...);
        }
        (std::make_index_sequence<Array.size()>{});
        return retval;
    }
}


} // namespace meta
} // namespace vistle
#endif
