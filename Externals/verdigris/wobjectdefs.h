/****************************************************************************
 *  Copyright (C) 2013-2015 Woboq GmbH
 *  Olivier Goffart <contact at woboq.com>
 *  https://woboq.com/
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program.
 *  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#ifndef Q_MOC_RUN // don't define anything when moc is run

#include <QtCore/qobjectdefs.h>
#include <QtCore/qmetatype.h>
#include <utility>

#if !defined(__cpp_constexpr) || __cpp_constexpr < 201304
#error Verdigris requires C++14 relaxed constexpr
#endif

namespace w_internal {
using std::index_sequence;  // From C++14, make sure to enable the C++14 option in your compiler

/* The default std::make_index_sequence from libstdc++ is recursing O(N) times which is reaching
    recursion limits level for big strings. This implementation has only O(log N) recursion */
template<size_t... I1, size_t... I2>
index_sequence<I1... , (I2 +(sizeof...(I1)))...>
make_index_sequence_helper_merge(index_sequence<I1...>, index_sequence<I2...>);

template<int N> struct make_index_sequence_helper {
    using part1 = typename make_index_sequence_helper<(N+1)/2>::result;
    using part2 = typename make_index_sequence_helper<N/2>::result;
    using result = decltype(make_index_sequence_helper_merge(part1(), part2()));
};
template<> struct make_index_sequence_helper<1> { using result = index_sequence<0>; };
template<> struct make_index_sequence_helper<0> { using result = index_sequence<>; };
template<int N> using make_index_sequence = typename make_index_sequence_helper<N>::result;


/**
 * In this namespace we find the implementation of a template binary tree container
 * It has a similar API than std::tuple, but is stored in a binary way.
 * the libstdc++ std::tuple recurse something like 16*N for a tuple of N elements. (Because the copy
 * constructor uses traits for stuff like noexcept.) Which means we are quickly reaching the
 * implementation limit of recursion. (Cannot have more than  ~16 items in a tuple)
 * Also, a linear tuple happens to lead to very slow compilation times.
 *
 * So a std::tuple<T1, T2, T3, T4> is represented by a
 * binary::tree<Node<Node<Leaf<T1>, Leaf<T2>>, Node<Leaf<T3>, Leaf<T4>>>
 */
namespace binary {

    template <typename T> struct Leaf {
        T data;
        static constexpr int Depth = 0;
        static constexpr int Count = 1;
        static constexpr bool Balanced = true;
    };

    template <class A, class B> struct Node {
        A a;
        B b;
        static constexpr int Count = A::Count + B::Count;
        static constexpr int Depth = A::Depth + 1;
        static constexpr bool Balanced = A::Depth == B::Depth && B::Balanced;
    };

    /** Add the node 'N' to the tree 'T'  (helper for tree_append) */
    template <class T, typename N, bool Balanced = T::Balanced > struct Add {
        typedef Node<T, Leaf<N> > Result;
        static constexpr Result add(T t, N n) { return {t, {n} }; }
    };
    template <class A, class B, typename N> struct Add<Node<A, B>, N, false> {
        typedef Node<A, typename Add<B, N>::Result > Result;
        static constexpr Result add(Node<A, B> t, N n) { return {t.a , Add<B, N>::add(t.b, n) }; }
    };

    /** Add the node 'N' to the tree 'T', on the left  (helper for tree_prepend) */
    template <class T, typename N, bool Balanced = T::Balanced > struct AddPre {
        typedef Node<Leaf<N> , T > Result;
        static constexpr Result add(T t, N n) { return {{n}, t}; }
    };
    template <class A, class B, typename N> struct AddPre<Node<A, B>, N, false> {
        typedef Node<typename AddPre<A, N>::Result, B > Result;
        static constexpr Result add(Node<A, B> t, N n) { return {AddPre<A, N>::add(t.a, n) , t.b }; }
    };

    /** helper for binary::get<> */
    template <class T, int I, typename = void> struct Get;
    template <class N> struct Get<Leaf<N>, 0>
    { static constexpr N get(Leaf<N> t) { return t.data; } };
    template <class A, class B, int I> struct Get<Node<A,B>, I, std::enable_if_t<(A::Count <= I)>>
    { static constexpr auto get(Node<A,B> t) { return Get<B,I - A::Count>::get(t.b); } };
    template <class A, class B, int I> struct Get<Node<A,B>, I, std::enable_if_t<(A::Count > I)>>
    { static constexpr auto get(Node<A,B> t) { return Get<A,I>::get(t.a); } };

    /** helper for tree_tail */
    template<typename A, typename B> struct Tail;
    template<typename A, typename B> struct Tail<Leaf<A>, B> {
        using Result = B;
        static constexpr B tail(Node<Leaf<A>,B> t) { return t.b; }
    };
    template<typename A, typename B, typename C> struct Tail<Node<A,B>, C> {
        using Result = Node<typename Tail<A,B>::Result, C>;
        static constexpr Result tail(Node<Node<A,B>, C> t) { return { Tail<A,B>::tail(t.a) , t.b }; }
    };

    /** An equivalent of std::tuple hold in a binary tree for faster compile time */
    template<typename T = void> struct tree {
        static constexpr int size = T::Count;
        T root;
    };
    template<> struct tree<> { static constexpr int size = 0; };

    /**
     * tree_append(tree, T):  append an element at the end of the tree.
     */
    template<typename T>
    constexpr tree<Leaf<T>> tree_append(tree<>, T n)
    { return {{n}}; }
    template<typename Root, typename T>
    constexpr tree<typename Add<Root,T>::Result> tree_append(tree<Root> t, T n)
    { return {Add<Root,T>::add(t.root,n)}; }

    /**
     * tree_append(tree, T):  prepend an element at the beginning of the tree.
     */
    template<typename T>
    constexpr tree<Leaf<T>> tree_prepend(tree<>, T n)
    { return {{n}}; }
    template<typename Root, typename T>
    constexpr tree<typename AddPre<Root,T>::Result> tree_prepend(tree<Root> t, T n)
    { return {AddPre<Root,T>::add(t.root,n)}; }

    /**
     * get<N>(tree): Returns the element from the tree at index N.
     */
    template<int N, typename Root> constexpr auto get(tree<Root> t)
    { return Get<Root, N>::get(t.root); }

    /**
     * tree_tail(tree):  Returns a tree with the first element removed.
     */
    template<typename A, typename B>
    constexpr tree<typename Tail<A,B>::Result> tree_tail(tree<Node<A, B>> t)
    { return { Tail<A,B>::tail(t.root) }; }
    template<typename N>
    constexpr tree<> tree_tail(tree<Leaf<N>>) { return {}; }
    constexpr tree<> tree_tail(tree<>) { return {}; }

    /**
     * tree_head(tree): same as get<O> but return something invalid in case the tuple is too small.
     */
    template<typename T> constexpr auto tree_head(tree<T> t) { return get<0>(t); }
    constexpr auto tree_head(tree<void>) { struct _{}; return _{}; }
    template<typename T> constexpr auto tree_head(T) { struct _{}; return _{}; }

    template<int I, typename T> using tree_element_t = decltype(get<I>(std::declval<T>()));

    /**
     * tree_cat(tree1, tree2, ....): concatenate trees (like tuple_cat)
     */
    // FIXME: Should we balance?
    template<class A, class B>
    constexpr tree<Node<A,B>> tree_cat(tree<A> a, tree<B> b) { return { { a.root, b.root } }; }
    template<class A>
    constexpr tree<A> tree_cat(tree<>, tree<A> a) { return a; }
    template<class A>
    constexpr tree<A> tree_cat(tree<A> a, tree<>) { return a; }
    constexpr tree<> tree_cat(tree<>, tree<>) { return {}; }
    template<class A, class B, class C, class...D>
    constexpr auto tree_cat(A a, B b, C c, D... d) { return tree_cat(a, tree_cat(b, c, d...)); }
} // namespace binary


/** Compute the sum of many integers */
constexpr int sums() { return 0; }
template<typename... Args>
constexpr int sums(int i, Args... args) { return i + sums(args...);  }
template <int ...Args>
constexpr int summed = sums(Args...);

/*
 * Helpers to play with static strings
 */

/** A compile time character array of size N  */
template<int N> using StaticStringArray = const char [N];

/** Represents a string of size N  (N includes the '\0' at the end) */
template<int N> struct StaticString  {
    StaticStringArray<N> data;
    template <std::size_t... I>
    constexpr StaticString(StaticStringArray<N> &d, std::index_sequence<I...>) : data{ (d[I])... } { }
    constexpr StaticString(StaticStringArray<N> &d) : StaticString(d, w_internal::make_index_sequence<N>()) {}
    static constexpr int size = N;
    constexpr char operator[](int p) const { return data[p]; }
};
template <int N> constexpr StaticString<N> makeStaticString(StaticStringArray<N> &d) { return {d}; }

/** A list containing many StaticString with possibly different sizes */
template<typename T = void> using StaticStringList = binary::tree<T>;

/** Make a StaticStringList out of many char array  */
constexpr StaticStringList<> makeStaticStringList() { return {}; }
template<typename... T>
constexpr StaticStringList<> makeStaticStringList(StaticStringArray<1> &, T...)
{ return {}; }
template<int N, typename... T>
constexpr auto makeStaticStringList(StaticStringArray<N> &h, T&...t)
{ return binary::tree_prepend(makeStaticStringList(t...), StaticString<N>(h)); }

/** Add a string in a StaticStringList */
template<int L, typename T>
constexpr auto addString(const StaticStringList<T> &l, const StaticString<L> & s) {
    return binary::tree_append(l, s);
}


/*-----------*/

    // From qmetaobject_p.h
enum class PropertyFlags  {
    Invalid = 0x00000000,
    Readable = 0x00000001,
    Writable = 0x00000002,
    Resettable = 0x00000004,
    EnumOrFlag = 0x00000008,
    StdCppSet = 0x00000100,
    //     Override = 0x00000200,
    Constant = 0x00000400,
    Final = 0x00000800,
    Designable = 0x00001000,
    ResolveDesignable = 0x00002000,
    Scriptable = 0x00004000,
    ResolveScriptable = 0x00008000,
    Stored = 0x00010000,
    ResolveStored = 0x00020000,
    Editable = 0x00040000,
    ResolveEditable = 0x00080000,
    User = 0x00100000,
    ResolveUser = 0x00200000,
    Notify = 0x00400000,
    Revisioned = 0x00800000
};
constexpr uint operator|(uint a, PropertyFlags b) { return a | uint(b); }

/** w_number<I> is a helper to implement state */
template<int N = 255> struct w_number : public w_number<N - 1> {
    static constexpr int value = N;
    static constexpr w_number<N-1> prev() { return {}; }
};
template<> struct w_number<0> { static constexpr int value = 0; };

template <int N> struct W_MethodFlags { static constexpr int value = N; };
} // w_internal

/** Objects that can be used as flags in the W_SLOT macro */

// Mirror of QMetaMethod::Access
namespace W_Access {
/* // From qmetaobject_p.h MethodFlags
    AccessPrivate = 0x00,
    AccessProtected = 0x01,
    AccessPublic = 0x02,
    AccessMask = 0x03, //mask
 */
    constexpr w_internal::W_MethodFlags<0x00> Public{};
    constexpr w_internal::W_MethodFlags<0x01> Protected{};
    constexpr w_internal::W_MethodFlags<0x02> Private{}; // Note: Public and Private are reversed so Public can be the default
}

// Mirror of QMetaMethod::MethodType
namespace W_MethodType {
    /*  // From qmetaobject_p.h MethodFlags
        MethodMethod = 0x00,
        MethodSignal = 0x04,
        MethodSlot = 0x08,
        MethodConstructor = 0x0c,
        MethodTypeMask = 0x0c,
    */
    constexpr w_internal::W_MethodFlags<0x00> Method{};
    constexpr w_internal::W_MethodFlags<0x04> Signal{};
    constexpr w_internal::W_MethodFlags<0x08> Slot{};
    constexpr w_internal::W_MethodFlags<0x0c> Constructor{};
}

/*
MethodCompatibility = 0x10,
MethodCloned = 0x20,
MethodScriptable = 0x40,
MethodRevisioned = 0x80
*/
constexpr w_internal::W_MethodFlags<0x10> W_Compat{};
constexpr w_internal::W_MethodFlags<0x40> W_Scriptable{};
constexpr struct {} W_Notify{};
constexpr struct {} W_Reset{};
constexpr std::integral_constant<int, int(w_internal::PropertyFlags::Constant)> W_Constant{};
constexpr std::integral_constant<int, int(w_internal::PropertyFlags::Final)> W_Final{};

namespace w_internal {

// workaround to avoid leading comma in macro that can optionaly take a flag
struct W_RemoveLeadingComma { constexpr W_MethodFlags<0> operator+() const { return {}; } };
template <typename T> constexpr T operator+(T &&t, W_RemoveLeadingComma) { return t; }
constexpr W_RemoveLeadingComma W_removeLeadingComma{};

/** Holds information about a method */
template<typename F, int NameLength, int Flags, typename ParamTypes, typename ParamNames = StaticStringList<>>
struct MetaMethodInfo {
    F func;
    StaticString<NameLength> name;
    ParamTypes paramTypes;
    ParamNames paramNames;
    static constexpr int argCount = QtPrivate::FunctionPointer<F>::ArgumentCount;
    static constexpr int flags = Flags;
};

// Called from the W_SLOT macro
template<typename F, int N, typename ParamTypes, int... Flags>
constexpr MetaMethodInfo<F, N, summed<Flags...> | W_MethodType::Slot.value, ParamTypes>
makeMetaSlotInfo(F f, StaticStringArray<N> &name, const ParamTypes &paramTypes, W_MethodFlags<Flags>...)
{ return { f, {name}, paramTypes, {} }; }

// Called from the W_METHOD macro
template<typename F, int N, typename ParamTypes, int... Flags>
constexpr MetaMethodInfo<F, N, summed<Flags...> | W_MethodType::Method.value, ParamTypes>
makeMetaMethodInfo(F f, StaticStringArray<N> &name, const ParamTypes &paramTypes, W_MethodFlags<Flags>...)
{ return { f, {name}, paramTypes, {} }; }

// Called from the W_SIGNAL macro
template<typename F, int N, typename ParamTypes, typename ParamNames, int... Flags>
constexpr MetaMethodInfo<F, N, summed<Flags...> | W_MethodType::Signal.value,
                            ParamTypes, ParamNames>
makeMetaSignalInfo(F f, StaticStringArray<N> &name, const ParamTypes &paramTypes,
                    const ParamNames &paramNames, W_MethodFlags<Flags>...)
{ return { f, {name}, paramTypes, paramNames }; }

/** Holds information about a constructor */
template<int NameLength, typename... Args> struct MetaConstructorInfo {
    static constexpr int argCount = sizeof...(Args);
    static constexpr int flags = W_MethodType::Constructor.value | W_Access::Public.value;
    StaticString<NameLength> name;
    template<int N>
    constexpr MetaConstructorInfo<N, Args...> setName(StaticStringArray<N> &name)
    { return { { name } }; }
    template<typename T, std::size_t... I>
    void createInstance(void **_a, std::index_sequence<I...>) const {
        *reinterpret_cast<T**>(_a[0]) =
            new T(*reinterpret_cast<std::remove_reference_t<Args> *>(_a[I+1])...);
    }
};
// called from the W_CONSTRUCTOR macro
template<typename...  Args> constexpr MetaConstructorInfo<1,Args...> makeMetaConstructorInfo()
{ return { {""} }; }

/** Holds information about a property */
template<typename Type, int NameLength, int TypeLength, typename Getter = std::nullptr_t,
            typename Setter = std::nullptr_t, typename Member = std::nullptr_t,
            typename Notify = std::nullptr_t, typename Reset = std::nullptr_t, int Flags = 0>
struct MetaPropertyInfo {
    using PropertyType = Type;
    StaticString<NameLength> name;
    StaticString<TypeLength> typeStr;
    Getter getter;
    Setter setter;
    Member member;
    Notify notify;
    Reset reset;
    static constexpr uint flags = Flags;

    template <typename S> constexpr auto setGetter(const S&s) const {
        return MetaPropertyInfo<Type, NameLength, TypeLength, S, Setter, Member, Notify, Reset,
                                Flags | PropertyFlags::Readable>
        {name, typeStr, s, setter, member, notify, reset};
    }
    template <typename S> constexpr auto setSetter(const S&s) const {
        return MetaPropertyInfo<Type, NameLength, TypeLength, Getter, S, Member, Notify, Reset,
                                Flags | PropertyFlags::Writable>
        {name, typeStr, getter, s, member, notify, reset};
    }
    template <typename S> constexpr auto setMember(const S&s) const {
        return MetaPropertyInfo<Type, NameLength, TypeLength, Getter, Setter, S, Notify, Reset,
                                Flags | PropertyFlags::Writable | PropertyFlags::Readable>
        {name, typeStr, getter, setter, s, notify, reset};
    }
    template <typename S> constexpr auto setNotify(const S&s) const {
        return MetaPropertyInfo<Type, NameLength, TypeLength, Getter, Setter, Member, S, Reset,
                                Flags | PropertyFlags::Notify>
        { name, typeStr, getter, setter, member, s, reset};
    }
    template <typename S> constexpr auto setReset(const S&s) const {
        return MetaPropertyInfo<Type, NameLength, TypeLength, Getter, Setter, Member, Notify, S,
                                Flags | PropertyFlags::Resettable>
        { name, typeStr, getter, setter, member, notify, s};
    }
    template <int Flag> constexpr auto addFlag() const {
        return MetaPropertyInfo<Type, NameLength, TypeLength, Getter, Setter, Member, Notify, Reset,
                                Flags | Flag>
        { name, typeStr, getter, setter, member, notify, reset};
    }
};

/** Parse a property and fill a MetaPropertyInfo (called from W_PRPERTY macro) */
// base case
template <typename PropInfo> constexpr auto parseProperty(const PropInfo &p) { return p; }
// setter
template <typename PropInfo, typename Obj, typename Arg, typename Ret, typename... Tail>
constexpr auto parseProperty(const PropInfo &p, Ret (Obj::*s)(Arg), Tail... t)
{ return parseProperty(p.setSetter(s) , t...); }
// getter
template <typename PropInfo, typename Obj, typename Ret, typename... Tail>
constexpr auto parseProperty(const PropInfo &p, Ret (Obj::*s)(), Tail... t)
{ return parseProperty(p.setGetter(s), t...); }
template <typename PropInfo, typename Obj, typename Ret, typename... Tail>
constexpr auto parseProperty(const PropInfo &p, Ret (Obj::*s)() const, Tail... t)
{ return parseProperty(p.setGetter(s), t...); }
// member
template <typename PropInfo, typename Obj, typename Ret, typename... Tail>
constexpr auto parseProperty(const PropInfo &p, Ret Obj::*s, Tail... t)
{ return parseProperty(p.setMember(s) ,t...); }
// notify
template <typename PropInfo, typename F, typename... Tail>
constexpr auto parseProperty(const PropInfo &p, decltype(W_Notify), F f, Tail... t)
{ return parseProperty(p.setNotify(f) ,t...); }
// reset
template <typename PropInfo, typename Obj, typename Ret, typename... Tail>
constexpr auto parseProperty(const PropInfo &p, decltype(W_Reset), Ret (Obj::*s)(), Tail... t)
{ return parseProperty(p.setReset(s) ,t...); }
// other flags flags
template <typename PropInfo, int Flag, typename... Tail>
constexpr auto parseProperty(const PropInfo &p, std::integral_constant<int, Flag>, Tail... t)
{ return parseProperty(p.template addFlag<Flag>() ,t...); }

template<typename T, int N1, int N2, typename ... Args>
constexpr auto makeMetaPropertyInfo(StaticStringArray<N1> &name, StaticStringArray<N2> &type, Args... args) {
    MetaPropertyInfo<T, N1, N2> meta
    { {name}, {type}, {}, {}, {}, {}, {} };
    return parseProperty(meta, args...);
}

/** Holds information about an enum */
template<int NameLength, typename Values_, typename Names, int Flags>
struct MetaEnumInfo {
    StaticString<NameLength> name;
    Names names;
    using Values = Values_;
    static constexpr uint flags = Flags;
    static constexpr uint count = Values::size();
};
template<typename Enum, Enum... Value> struct enum_sequence {};
// called from W_ENUM and W_FLAG
template<typename Enum, int Flag, int NameLength, Enum... Values, typename Names>
constexpr MetaEnumInfo<NameLength, std::index_sequence<size_t(Values)...> , Names, Flag> makeMetaEnumInfo(
                StaticStringArray<NameLength> &name, enum_sequence<Enum, Values...>, Names names) {
    return { {name}, names };
}

/**
 * Helper for the implementation of a signal.
 * Called from the signal implementation within the W_SIGNAL macro.
 *
 * 'Func' is the type of the signal. 'Idx' is the signal index (relative).
 * It is implemented as a functor so the operator() has exactly the same amount of argument of the
 * signal so the __VA_ARGS__ works also if there is no arguments (no leading commas)
 *
 * There is specialization for const and non-const,  and for void and non-void signals.
 */
template<typename Func, int Idx> struct SignalImplementation {};
template<typename Obj, typename Ret, typename... Args, int Idx>
struct SignalImplementation<Ret (Obj::*)(Args...), Idx>{
    Obj *this_;
    Ret operator()(Args... args) const {
        Ret r{};
        const void * a[]= { &r, (&args)... };
        QMetaObject::activate(this_, &Obj::staticMetaObject, Idx, const_cast<void **>(a));
        return r;
    }
};
template<typename Obj, typename... Args, int Idx>
struct SignalImplementation<void (Obj::*)(Args...), Idx>{
    Obj *this_;
    void operator()(Args... args) {
        const void *a[]= { nullptr, (&args)... };
        QMetaObject::activate(this_, &Obj::staticMetaObject, Idx, const_cast<void **>(a));
    }
};
template<typename Obj, typename Ret, typename... Args, int Idx>
struct SignalImplementation<Ret (Obj::*)(Args...) const, Idx>{
    const Obj *this_;
    Ret operator()(Args... args) const {
        Ret r{};
        const void * a[]= { &r, (&args)... };
        QMetaObject::activate(const_cast<Obj*>(this_), &Obj::staticMetaObject, Idx, const_cast<void **>(a));
        return r;
    }
};
template<typename Obj, typename... Args, int Idx>
struct SignalImplementation<void (Obj::*)(Args...) const, Idx>{
    const Obj *this_;
    void operator()(Args... args) {
        const void *a[]= { nullptr, (&args)... };
        QMetaObject::activate(const_cast<Obj*>(this_), &Obj::staticMetaObject, Idx, const_cast<void **>(a));
    }
};

/**
 * Used in the W_OBJECT macro to compute the base type.
 * Used like this:
 *  using W_BaseType = std::remove_reference_t<decltype(getParentObjectHelper(&W_ThisType::qt_metacast))>;
 * Since qt_metacast for W_ThisType will be declared later, the pointer to member function will be
 * pointing to the qt_metacast of the base class, so T will be deduced to the base class type.
 *
 * Returns a reference so this work if T is an abstract class.
 */
template<typename T> T &getParentObjectHelper(void* (T::*)(const char*));

// helper class that can access the private member of any class with W_OBJECT
struct FriendHelper;

} // w_internal

#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
inline namespace w_ShouldBeInQt {
// This is already in Qt 5.7, but added in an inline namespace so it works with previous version of Qt
template <typename... Args>
struct QNonConstOverload
{
    template <typename R, typename T>
    constexpr auto operator()(R (T::*ptr)(Args...)) const
    { return ptr; }
};
template <typename... Args>
struct QConstOverload
{
    template <typename R, typename T>
    constexpr auto operator()(R (T::*ptr)(Args...) const) const
    { return ptr; }
};
template <typename... Args>
struct QOverload : QConstOverload<Args...>, QNonConstOverload<Args...>
{
    using QConstOverload<Args...>::operator();
    using QNonConstOverload<Args...>::operator();

    template <typename R>
    constexpr auto operator()(R (*ptr)(Args...)) const
    { return ptr; }
};
template <typename... Args> constexpr QOverload<Args...> qOverload = {};
}

#ifndef QT_ANNOTATE_CLASS // Was added in Qt 5.6.1
#define QT_ANNOTATE_CLASS(...)
#endif

#endif // Qt < 5.7

#define EXPAND(x) x

// Private macro helpers for classical macro programming
#define W_MACRO_EMPTY
#define W_MACRO_EVAL(...) __VA_ARGS__
#define W_MACRO_DELAY(X,...) EXPAND(X(__VA_ARGS__))
#define W_MACRO_DELAY2(X,...) EXPAND(X(__VA_ARGS__))
#define W_MACRO_TAIL(A, ...) __VA_ARGS__
#define W_MACRO_STRIGNIFY(...) W_MACRO_STRIGNIFY2(__VA_ARGS__)
#define W_MACRO_STRIGNIFY2(...) #__VA_ARGS__
#define W_MACRO_CONCAT(A, B) W_MACRO_CONCAT2(A,B)
#define W_MACRO_CONCAT2(A, B) A##_##_##B

// strignify and make a StaticStringList out of an array of arguments
#define W_PARAM_TOSTRING(...) W_PARAM_TOSTRING2(__VA_ARGS__ ,,,,,,,,,,,,,,,,)
#define W_PARAM_TOSTRING2(A1,A2,A3,A4,A5,A6,A7,A8,A9,A10,A11,A12,A13,A14,A15,A16,...) \
    w_internal::makeStaticStringList(#A1,#A2,#A3,#A4,#A5,#A6,#A7,#A8,#A9,#A10,#A11,#A12,#A13,#A14,#A15,#A16)

// remove the surrounding parentheses
#define W_MACRO_REMOVEPAREN(A) W_MACRO_DELAY(W_MACRO_REMOVEPAREN2, W_MACRO_REMOVEPAREN_HELPER A)
#define W_MACRO_REMOVEPAREN2(...) W_MACRO_DELAY2(W_MACRO_TAIL, W_MACRO_REMOVEPAREN_HELPER_##__VA_ARGS__)
#define W_MACRO_REMOVEPAREN_HELPER(...) _ , __VA_ARGS__
#define W_MACRO_REMOVEPAREN_HELPER_W_MACRO_REMOVEPAREN_HELPER ,

// if __VA_ARGS__ is "(types), foobar"   then return just the types, otherwise return nothing
#define W_OVERLOAD_TYPES(A, ...) W_MACRO_DELAY(W_MACRO_TAIL,W_OVERLOAD_TYPES_HELPER A)
#define W_OVERLOAD_TYPES_HELPER(...) , __VA_ARGS__

#define W_OVERLOAD_RESOLVE(A, ...) W_MACRO_DELAY(W_MACRO_TAIL,W_OVERLOAD_RESOLVE_HELPER A)
#define W_OVERLOAD_RESOLVE_HELPER(...) , qOverload<__VA_ARGS__>

// remove the first argument if it is in parentheses"
#define W_OVERLOAD_REMOVE(...) W_MACRO_DELAY(W_OVERLOAD_REMOVE2, W_OVERLOAD_REMOVE_HELPER __VA_ARGS__)
#define W_OVERLOAD_REMOVE2(...) W_MACRO_DELAY2(W_MACRO_TAIL, W_OVERLOAD_REMOVE_HELPER_##__VA_ARGS__)

#define W_OVERLOAD_REMOVE_HELPER(...) _
#define W_OVERLOAD_REMOVE_HELPER_W_OVERLOAD_REMOVE_HELPER ,

#define W_RETURN(R) -> decltype(R) { return R; }

#define W_OBJECT_COMMON(TYPE) \
        using W_ThisType = TYPE; \
        static constexpr auto &W_UnscopedName = #TYPE; /* so we don't repeat it in W_CONSTRUCTOR */ \
        friend struct w_internal::FriendHelper; \
        friend constexpr w_internal::binary::tree<> w_SlotState(w_internal::w_number<0>, W_ThisType**) { return {}; } \
        friend constexpr w_internal::binary::tree<> w_SignalState(w_internal::w_number<0>, W_ThisType**) { return {}; } \
        friend constexpr w_internal::binary::tree<> w_MethodState(w_internal::w_number<0>, W_ThisType**) { return {}; } \
        friend constexpr w_internal::binary::tree<> w_ConstructorState(w_internal::w_number<0>, W_ThisType**) { return {}; } \
        friend constexpr w_internal::binary::tree<> w_PropertyState(w_internal::w_number<0>, W_ThisType**) { return {}; } \
        friend constexpr w_internal::binary::tree<> w_EnumState(w_internal::w_number<0>, W_ThisType**) { return {}; } \
        friend constexpr w_internal::binary::tree<> w_ClassInfoState(w_internal::w_number<0>, W_ThisType**) { return {}; } \
        friend constexpr w_internal::binary::tree<> w_InterfaceState(w_internal::w_number<0>, W_ThisType**) { return {}; } \
    public: \
        struct W_MetaObjectCreatorHelper;

#if defined Q_CC_GNU && !defined Q_CC_CLANG
// workaround gcc bug  (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69836)
#define W_STATE_APPEND(STATE, ...) \
    static constexpr int W_MACRO_CONCAT(W_WORKAROUND_, __LINE__) = \
        decltype(STATE(w_internal::w_number<>{}, static_cast<W_ThisType**>(nullptr)))::size+1; \
    friend constexpr auto STATE(w_internal::w_number<W_MACRO_CONCAT(W_WORKAROUND_, __LINE__)> w_counter, W_ThisType **w_this) \
        W_RETURN(w_internal::binary::tree_append(STATE(w_counter.prev(), w_this), __VA_ARGS__))
#else
#define W_STATE_APPEND(STATE, ...) \
    friend constexpr auto STATE(w_internal::w_number<decltype(STATE( \
            w_internal::w_number<>{}, static_cast<W_ThisType**>(nullptr)))::size+1> w_counter, \
            W_ThisType **w_this) \
        W_RETURN(w_internal::binary::tree_append(STATE(w_counter.prev(), w_this), __VA_ARGS__))
#endif

//
// public macros

/** \macro W_OBJECT(TYPE)
 * Like the Q_OBJECT macro, this declare that the object might have signals, slots or properties.
 * Must contains the class name as a parameter and need to be put before any other W_ macro in the class.
 */
#define W_OBJECT(TYPE) \
    W_OBJECT_COMMON(TYPE) \
    public: \
        using W_BaseType = std::remove_reference_t<decltype(\
            w_internal::getParentObjectHelper(&W_ThisType::qt_metacast))>; \
    Q_OBJECT \
    QT_ANNOTATE_CLASS(qt_fake, "")

/** \macro W_GADGET(TYPE)
 * Like the Q_GADGET macro, this declare that the object might have properties.
 * Must contains the class name as a parameter and need to be put before any other W_ macro in the class.
 */
#define W_GADGET(TYPE) \
    W_OBJECT_COMMON(TYPE) \
    Q_GADGET \
    QT_ANNOTATE_CLASS(qt_fake, "")

/**
 * W_SLOT( <slot name> [, (<parameters types>) ]  [, <flags>]* )
 *
 * The W_SLOT macro needs to be put after the slot declaration.
 *
 * The W_SLOT macro can optionally have a list of parameter types as second argument to disambiguate
 * overloads or use types that are not registered with W_REGISTER_ARGTYPE. The list of parameter
 * need to be within parentheses (even if there is 0 or 1 argument).
 *
 * The W_SLOT macro can have flags:
 * - Specifying the the access:  W_Access::Protected, W_Access::Private
 *   or W_Access::Public (the default)
 * - W_Compat: for deprecated methods (equivalent of Q_MOC_COMPAT)
 */
#define W_SLOT(NAME, ...) \
    W_STATE_APPEND(w_SlotState, w_internal::makeMetaSlotInfo( \
            W_OVERLOAD_RESOLVE(__VA_ARGS__)(&W_ThisType::NAME), #NAME,  \
            W_PARAM_TOSTRING(W_OVERLOAD_TYPES(__VA_ARGS__)), \
            W_OVERLOAD_REMOVE(__VA_ARGS__) +w_internal::W_removeLeadingComma))

/**
 * W_INVOKABLE( <slot name> [, (<parameters types>) ]  [, <flags>]* )
 * Exactly like W_SLOT but for Q_INVOKABLE methods.
 */
#define W_INVOKABLE(NAME, ...) \
    W_STATE_APPEND(w_MethodState, w_internal::makeMetaMethodInfo( \
            W_OVERLOAD_RESOLVE(__VA_ARGS__)(&W_ThisType::NAME), #NAME,  \
            W_PARAM_TOSTRING(W_OVERLOAD_TYPES(__VA_ARGS__)), \
            W_OVERLOAD_REMOVE(__VA_ARGS__) +w_internal::W_removeLeadingComma))

/**
 * <signal signature>
 * W_SIGNAL(<signal name> [, (<parameter types>) ] , <parameter names> )
 *
 * Unlike W_SLOT, W_SIGNAL must be placed directly after the signal signature declaration.
 * There should not be a semi colon between the signal signature and the macro
 *
 * Like W_SLOT, there can be the types of the parametter as a second argument, within parentheses.
 * You must then follow with the parameter names
 */
#define W_SIGNAL(NAME, ...) \
    { /* W_SIGNAL need to be placed directly after the signal declaration, without semicolon. */\
        using w_SignalType = decltype(EXPAND(W_OVERLOAD_RESOLVE(__VA_ARGS__))(&W_ThisType::NAME)); \
        return w_internal::SignalImplementation<w_SignalType, W_MACRO_CONCAT(w_signalIndex_##NAME,__LINE__)>{this}(W_OVERLOAD_REMOVE(__VA_ARGS__)); \
    } \
    static constexpr int W_MACRO_CONCAT(w_signalIndex_##NAME,__LINE__) = \
        decltype(w_SignalState(w_internal::w_number<>{}, static_cast<W_ThisType**>(nullptr)))::size; \
    friend constexpr auto w_SignalState(w_internal::w_number<W_MACRO_CONCAT(w_signalIndex_##NAME,__LINE__) + 1> w_counter, W_ThisType **w_this) \
        W_RETURN(w_internal::binary::tree_append(w_SignalState(w_counter.prev(), w_this), \
            w_internal::makeMetaSignalInfo( \
                W_OVERLOAD_RESOLVE(__VA_ARGS__)(&W_ThisType::NAME), #NAME, \
                W_PARAM_TOSTRING(W_OVERLOAD_TYPES(__VA_ARGS__)), W_PARAM_TOSTRING(W_OVERLOAD_REMOVE(__VA_ARGS__)))))

/** \macro W_SIGNAL_COMPAT
 * Same as W_SIGNAL, but set the W_Compat flag
 */
#define W_SIGNAL_COMPAT(NAME, ...) \
    { \
        using w_SignalType = decltype(W_OVERLOAD_RESOLVE(__VA_ARGS__)(&W_ThisType::NAME)); \
        return w_internal::SignalImplementation<w_SignalType, W_MACRO_CONCAT(w_signalIndex_##NAME,__LINE__)>{this}(W_OVERLOAD_REMOVE(__VA_ARGS__)); \
    } \
    static constexpr int W_MACRO_CONCAT(w_signalIndex_##NAME,__LINE__) = \
        decltype(w_SignalState(w_internal::w_number<>{}, static_cast<W_ThisType**>(nullptr)))::size; \
    friend constexpr auto w_SignalState(w_internal::w_number<W_MACRO_CONCAT(w_signalIndex_##NAME,__LINE__) + 1> w_counter, W_ThisType **w_this) \
        W_RETURN(w_internal::binary::tree_append(w_SignalState(w_counter.prev(), w_this), \
            w_internal::makeMetaSignalInfo( \
                W_OVERLOAD_RESOLVE(__VA_ARGS__)(&W_ThisType::NAME), #NAME, \
                W_PARAM_TOSTRING(W_OVERLOAD_TYPES(__VA_ARGS__)), W_PARAM_TOSTRING(W_OVERLOAD_REMOVE(__VA_ARGS__)), W_Compat)))

/** W_CONSTRUCTOR(<parameter types>)
 * Declares that this class can be constructed with this list of argument.
 * Equivalent to Q_INVOKABLE constructor.
 * One can have W_CONSTRUCTOR() for the default constructor even if it is implicit.
 */
#define W_CONSTRUCTOR(...) \
    W_STATE_APPEND(w_ConstructorState, \
                   w_internal::makeMetaConstructorInfo<__VA_ARGS__>().setName(W_UnscopedName))


/** W_PROPERTY(<type>, <name> [, <flags>]*)
 *
 * Declare a property <name> with the type <type>.
 * The flags can be function pointers that are detected to be setter, getters, notify signal or
 * other flags. Use the macro READ, WRITE, MEMBER, ... for the flag so you can write W_PROPERTY
 * just like in a Q_PROPERTY. The only difference with Q_PROPERTY would be the semicolon before the
 * name.
 * W_PROPERTY need to be put after all the setters, getters, signals and members have been declared.
 *
 * <type> can optionally be put in parentheses, if you have a type containing a comma
 */
#define W_PROPERTY(...) W_PROPERTY2(__VA_ARGS__) // expands the READ, WRITE, and other sub marcos
#define W_PROPERTY2(TYPE, NAME, ...) \
    W_STATE_APPEND(w_PropertyState, \
        w_internal::makeMetaPropertyInfo<W_MACRO_REMOVEPAREN(TYPE)>(\
                            #NAME, W_MACRO_STRIGNIFY(W_MACRO_REMOVEPAREN(TYPE)), __VA_ARGS__))

#define WRITE , &W_ThisType::
#define READ , &W_ThisType::
#define NOTIFY , W_Notify, &W_ThisType::
#define RESET , W_Reset, &W_ThisType::
#define MEMBER , &W_ThisType::
#define CONSTANT , W_Constant
#define FINAL , W_Final

/** W_ENUM(<name>, <values>)
 * Similar to Q_ENUM, but one must also manually write all the values.
 */
#define W_ENUM(NAME, ...) \
    W_STATE_APPEND(w_EnumState, w_internal::makeMetaEnumInfo<NAME,false>( \
            #NAME, w_internal::enum_sequence<NAME,__VA_ARGS__>{}, W_PARAM_TOSTRING(__VA_ARGS__))) \
    Q_ENUM(NAME)

/** W_FLAG<name>, <values>)
 * Similar to Q_FLAG, but one must also manually write all the values.
 */
namespace w_internal {
template<typename T> struct QEnumOrQFlags { using Type = T; };
template<typename T> struct QEnumOrQFlags<QFlags<T>> { using Type = T; };
}
#define W_FLAG(NAME, ...) \
    W_STATE_APPEND(w_EnumState, w_internal::makeMetaEnumInfo<w_internal::QEnumOrQFlags<NAME>::Type,true>( \
            #NAME, w_internal::enum_sequence<w_internal::QEnumOrQFlags<NAME>::Type,__VA_ARGS__>{}, W_PARAM_TOSTRING(__VA_ARGS__))) \
    Q_FLAG(NAME)

/** Same as Q_CLASSINFO */
#define W_CLASSINFO(A, B) \
    W_STATE_APPEND(w_ClassInfoState, \
        std::pair<w_internal::StaticString<sizeof(A)>, w_internal::StaticString<sizeof(B)>>{ {A}, {B} })

/** Same as Q_INTERFACE */
#define W_INTERFACE(A) \
    W_STATE_APPEND(w_InterfaceState, static_cast<A*>(nullptr))


/** \macro W_REGISTER_ARGTYPE(TYPE)
 Registers TYPE so it can be used as a parameter of a signal/slot or return value.
 The normalized signature must be used.
 Note: This does not imply Q_DECLARE_METATYPE, and Q_DECLARE_METATYPE does not imply this
*/
namespace w_internal { template<typename T> struct W_TypeRegistery { enum { registered = false }; }; }
#define W_REGISTER_ARGTYPE(...) namespace w_internal { \
    template<> struct W_TypeRegistery<__VA_ARGS__> { \
    enum { registered = true }; \
    static constexpr auto name = makeStaticString(#__VA_ARGS__); \
  };}
W_REGISTER_ARGTYPE(char*)
W_REGISTER_ARGTYPE(const char*)

#else // Q_MOC_RUN
// just to avoid parse errors when moc is run over things that it should ignore
#define W_SIGNAL(...)        ;
#define W_SIGNAL_COMPAT(...) ;
#define W_PROPERTY(...)
#define W_SLOT(...)
#define W_CLASSINFO(...)
#define W_INTERFACE(...)
#define W_CONSTRUCTOR(...)
#define W_FLAG(...)
#define W_ENUM(...)
#endif
