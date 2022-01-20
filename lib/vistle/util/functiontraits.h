#ifndef VISTLE_FUNCTION_TRAITS_H
#define VISTLE_FUNCTION_TRAITS_H
namespace vistle {


template<typename>
struct member_function_traits;

template<typename Return, typename Object, typename... Args>
struct member_function_traits<Return (Object::*)(Args...)> {
    typedef Return return_type;
    typedef Object instance_type;
    typedef Object &instance_reference;

    // Can mess with Args... if you need to, for example:
    static constexpr size_t argument_count = sizeof...(Args);
};

// If you intend to support const member functions you need another specialization.
template<typename Return, typename Object, typename... Args>
struct member_function_traits<Return (Object::*)(Args...) const> {
    typedef Return return_type;
    typedef Object instance_type;
    typedef Object const &instance_reference;

    // Can mess with Args... if you need to, for example:
    static constexpr size_t argument_count = sizeof...(Args);
};

template<typename Func, typename... Args>
struct ReturnType {
    using value = decltype(std::declval<Func>()(std::declval<Args>()...));
};

} // namespace vistle
#endif // VISTLE_FUNCTION_TRAITS_H
