#pragma once

#include <array>
#include <string_view>
#include <type_traits>
#include <tuple>


namespace mini_reflection
{

template<class T>
struct is_tuple
{
    constexpr static bool value = false;
};

template<class... T>
struct is_tuple<std::tuple<T...>>
{
    constexpr static bool value = true;
};

template<class T>
inline constexpr bool is_tuple_v = is_tuple<T>::value;

template<bool, size_t, class T, class Default>
struct tuple_element_or_default_impl
{
    using type = Default;
};

template<size_t Index, class T, class Default>
struct tuple_element_or_default_impl<true, Index, T, Default>
{
    using type = std::tuple_element_t<Index, T>;
};

template<size_t Index, class T, class Default = void>
struct tuple_element_or_default
{
    using type = typename tuple_element_or_default_impl<
        (std::tuple_size_v<T> > Index),
        Index, T, Default
    >::type;
};

template<size_t Index, class T, class Default = void>
using tuple_element_or_default_t = typename tuple_element_or_default<Index, T, Default>::type;

// From C++20
template<class T>
struct remove_cvref {
    typedef std::remove_cv_t<std::remove_reference_t<T>> type;
};

template<class T>
using remove_cvref_t = typename remove_cvref<T>::type;

template<class T>
struct self
{
};

template<class T>
struct reflect
{
    template<class T, std::enable_if_t<std::is_same_v<T, remove_cvref_t<T>>>* = nullptr>
    static constexpr auto get_members()
    {
        return std::tuple<self<remove_cvref_t<T>>>{};
    };

    template<class T, std::enable_if_t<!std::is_same_v<T, remove_cvref_t<T>>>* = nullptr>
    static constexpr auto get_members()
    {
        return reflect<remove_cvref_t<T>>::members;
    };

    constexpr static auto members = get_members<T>();
};

template<class T>
struct reflect<self<T>>
{
    // invalid
};

template<class T, size_t N>
struct array{};

template<class T, size_t N>
struct reflect<T[N]>
{
    constexpr static auto members = std::tuple<array<remove_cvref_t<T>, N>>{};
};

template<class T, size_t N>
struct reflect<std::array<T, N>>
{
    constexpr static auto members = std::tuple<array<remove_cvref_t<T>, N>>{};
};

template<class T, size_t N>
struct reflect<array<T, N>>
{
    // invalid
};

template<class T, class MemberT>
struct member_descr
{
    MemberT T::*member;
    std::string_view name;

    using owner_t = T;
    using type = MemberT;

    constexpr member_descr(
        MemberT T::*member_, const char* name_ = "")
        : member(member_), name(name_) {}
};

constexpr auto member_tuple()
{
    return std::make_tuple();
}

template<class T, class MemberT, class... Args>
constexpr auto member_tuple(MemberT T::*member, const char* name, Args... args)
{
    return std::tuple_cat(
        std::make_tuple(member_descr(member, name)),
        member_tuple(args...)
    );
}

template<class T, class... Args, std::enable_if_t<is_tuple_v<T>>* = nullptr>
constexpr auto member_tuple(const T& tuple, Args... args)
{
    return std::tuple_cat(
        tuple,
        member_tuple(args...)
    );
}

template<class T, class MemberT, class... Args,
         std::enable_if_t<
            !std::is_same_v<char*, std::decay_t<tuple_element_or_default_t<0, std::tuple<Args...>>>>
         >* = nullptr
        >
constexpr auto member_tuple(MemberT T::*member, Args... args)
{
    return std::tuple_cat(
        std::make_tuple(member_descr(member)),
        member_tuple(args...)
    );
}

template<class T, class FuncT, class... Args>
constexpr void for_each_member(T&& o, FuncT f, Args&&... args)
{
    std::apply(
        [&](const auto&... m)
        {
            (f(std::forward<T>(o), m, std::forward<Args>(args)...), ...);
        },
        reflect<T>::members
    );
}

template<class T>
struct sizeof_noalign
{
    constexpr static size_t value = std::apply(
        [](const auto&... m)
        {
            return (sizeof_noalign<remove_cvref_t<decltype(m)>>::value + ... + 0);
        },
        reflect<T>::members
    );
};

template<class T, class MemberT>
struct sizeof_noalign<member_descr<T, MemberT>>
{
    constexpr static size_t value = sizeof_noalign<remove_cvref_t<MemberT>>::value;
};

template<class T>
struct sizeof_noalign<self<T>>
{
    constexpr static size_t value = sizeof(T);
};

template<class T, size_t N>
struct sizeof_noalign<array<T, N>>
{
    constexpr static size_t value = sizeof_noalign<remove_cvref_t<T>>::value * N;
};

}
