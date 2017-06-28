/****************************************************************************
 *  Copyright (C) 2016 Woboq GmbH
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
 *  License along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include "wobjectdefs.h"
#include <QtCore/qobject.h>

namespace w_internal {

/** concatenate()
 * Returns a StaticString which is the concatenation of all the strings in a StaticStringList
 * Note:  keeps the \0 between the strings
 */
template<typename I1, typename I2> struct concatenate_helper;
template<std::size_t... I1, std::size_t... I2> struct concatenate_helper<std::index_sequence<I1...>, std::index_sequence<I2...>> {
    static constexpr int size = sizeof...(I1) + sizeof...(I2);
    static constexpr auto concatenate(const StaticString<sizeof...(I1)> &s1, const StaticString<sizeof...(I2)> &s2) {
        StaticStringArray<size> d = { s1[I1]... , s2[I2]... };
        return StaticString<size>( d );
    }
};
constexpr StaticString<1> concatenate(StaticStringList<>) { return {""}; }
template<typename T> constexpr auto concatenate(StaticStringList<binary::Leaf<T>> s) { return s.root.data; }
template<typename A, typename B> constexpr auto concatenate(StaticStringList<binary::Node<A,B>> s) {
    auto a = concatenate(binary::tree<A>{s.root.a});
    auto b = concatenate(binary::tree<B>{s.root.b});
    return concatenate_helper<make_index_sequence<a.size>, make_index_sequence<b.size>>::concatenate(a, b);
}

enum { IsUnresolvedType = 0x80000000 };

/*
 * The QMetaObject is basically an array of int and an array of string.
 * Some of the int in the array are index in the string array.
 *
 * The IntermediateState class helps building the QMetaObject.
 * It contains the StaticStringList of strings within this meta object, and the array of int as
 * template parameter.
 *
 * It has helper method that helps extending the state
 */
template<typename Strings, uint... Ints>
struct IntermediateState {
    Strings strings;
    /// add a string to the strings state and add its index to the end of the int array
    template<int L>
    constexpr auto addString(const StaticString<L> & s) const {
        auto s2 = binary::tree_append(strings, s);
        return IntermediateState<decltype(s2), Ints..., Strings::size>{s2};
    }

    /// same as before but ass the IsUnresolvedType flag
    template<int L>
    constexpr auto addTypeString(const StaticString<L> & s) const {
        auto s2 = binary::tree_append(strings, s);
        return IntermediateState<decltype(s2), Ints...,
            IsUnresolvedType | Strings::size>{s2};
    }


    template<uint... Add>
    constexpr IntermediateState<Strings, Ints..., Add...> add() const
    { return { strings }; }

    static constexpr std::index_sequence<Ints ...> sequence = {};
};

/**
 * Iterate over all the items of a tree and call the Generator::generate function
 *
 * The first parameter f the function is the IntermediateState, and it returns a new
 * InterMediateState with all the information from the tree added to it.
 *
 * The 'Ofst' template parameter is the offset in the integer array to which new data can be added.
 *
 * The Generator template type has a method generate which will be called for every element of the
 * tree with the IntermediateState. It is called with the offset as a template parameter.
 *
 * The Generator also has an offset() method that returns how much things will be added later in
 * the array for this element.
 */
template<typename Generator, int Ofst, typename State>
constexpr auto generate(State s, binary::tree<>)
{ return s; }
template<typename Generator, int Ofst, typename State, typename Tree>
constexpr auto generate(State s, Tree t) {
    return generate<Generator, Ofst + Generator::template offset<binary::tree_element_t<0,Tree>>()>(
        Generator::template generate<Ofst>(s, binary::tree_head(t)), binary::tree_tail(t));
}

/**
 * Helper comparator that compare function pointers and return true if they are the same or
 * false if they are different. If they are of different type, they are different */
template <typename T1, typename T2> constexpr bool getSignalIndexHelperCompare(T1, T2) { return false; }
template <typename T> constexpr bool getSignalIndexHelperCompare(T f1, T f2) { return f1 == f2; }

/** Helper to get information bout the notify signal of the property with index Idx of the object T */
template<typename T, int Idx, typename = make_index_sequence<w_SignalState(w_number<>{},static_cast<T**>(nullptr)).size>>
struct ResolveNotifySignal;
template<typename T, int Idx, size_t ...I>
struct ResolveNotifySignal<T, Idx, index_sequence<I...>> {
private:
    static constexpr auto propertyInfo = w_PropertyState(w_number<>{},static_cast<T**>(nullptr));
    static constexpr auto property = binary::get<Idx>(propertyInfo);
    static constexpr auto signalState = w_SignalState(w_number<>{},static_cast<T**>(nullptr));

    // We need to use SFINAE because of GCC bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69681
    // For some reason, GCC fails to consider f1==f2 as a constexpr if f1 and f2 are pointer to
    // different function of the same type. Fortunately, when both are pointing to the same function
    // it compiles fine, so we can use SFINAE for it.
    template<int SigIdx>
    static constexpr std::enable_if_t<getSignalIndexHelperCompare(binary::get<SigIdx>(signalState).func, property.notify) || true, int>
        helper(int)
    { return getSignalIndexHelperCompare(binary::get<SigIdx>(signalState).func, property.notify) ? SigIdx : -1; }
    template<int SigIdx> static constexpr int helper(...) { return -1; }
public:
    static constexpr bool hasNotify = !std::is_same<decltype(property.notify), std::nullptr_t>::value;
    static constexpr int signalIndex = !hasNotify ? -1 : std::max({-1, helper<I>(0)...});
    static_assert(signalIndex >= 0 || !hasNotify, "NOTIFY signal not registered as a signal");
};

/** Add the notify signal index to the intermediate state, for each properties  */
template <typename T, typename State, std::size_t... I>
static constexpr auto generateNotifySignals(State s, std::true_type, std::index_sequence<I...>)
{ return s.template add<std::max(0, ResolveNotifySignal<T, I>::signalIndex)...>(); }
template <typename T, typename State, std::size_t... I>
static constexpr auto generateNotifySignals(State s, std::false_type, std::index_sequence<I...>)
{ return s; }
/** returns true if the object T has at least one property with a notify signal */
template <typename T, std::size_t... I>
static constexpr bool hasNotifySignal(std::index_sequence<I...>)
{ return sums(ResolveNotifySignal<T, I>::hasNotify...); }


/** Holds information about a class, including all the properties and methods */
template<int NameLength, typename Methods, typename Constructors, typename Properties,
            typename Enums, typename ClassInfos, typename Interfaces, int SignalCount>
struct ObjectInfo {
    StaticString<NameLength> name;
    Methods methods;
    Constructors constructors;
    Properties properties;
    Enums enums;
    ClassInfos classInfos;
    Interfaces interfaces;

    static constexpr int methodCount = Methods::size;
    static constexpr int constructorCount = Constructors::size;
    static constexpr int propertyCount = Properties::size;
    static constexpr int enumCount = Enums::size;
    static constexpr int classInfoCount = ClassInfos::size;
    static constexpr int interfaceCount = Interfaces::size;
    static constexpr int signalCount = SignalCount;
};

/**
 * Constructs a ObjectInfo for the object T.
 * Pass the (qualified) name as a static string.
 * Called from W_OBJECT_IMPL
 */
template<typename T, int N>
static constexpr auto makeObjectInfo(StaticStringArray<N> &name) {
    constexpr auto sigState = w_SignalState(w_number<>{}, static_cast<T**>(nullptr));
    constexpr auto methodInfo = binary::tree_cat(sigState, w_SlotState(w_number<>{}, static_cast<T**>(nullptr)),
                                                    w_MethodState(w_number<>{}, static_cast<T**>(nullptr)));
    constexpr auto constructorInfo = w_ConstructorState(w_number<>{}, static_cast<T**>(nullptr));
    constexpr auto propertyInfo = w_PropertyState(w_number<>{}, static_cast<T**>(nullptr));
    constexpr auto enumInfo = w_EnumState(w_number<>{}, static_cast<T**>(nullptr));
    constexpr auto classInfo = w_ClassInfoState(w_number<>{}, static_cast<T**>(nullptr));
    constexpr auto interfaceInfo = w_InterfaceState(w_number<>{}, static_cast<T**>(nullptr));
    constexpr int sigCount = sigState.size;
    return ObjectInfo<N, decltype(methodInfo), decltype(constructorInfo), decltype(propertyInfo),
                        decltype(enumInfo), decltype(classInfo), decltype(interfaceInfo), sigCount>
        { {name}, methodInfo, constructorInfo, propertyInfo, enumInfo, classInfo, interfaceInfo };
}

// Generator for Q_CLASSINFO to be used in generate<>()
struct ClassInfoGenerator {
    template<typename> static constexpr int offset() { return 0; }
    template<int, typename State, typename CI>
    static constexpr auto generate(State s, CI ci) {
        return s.addString(ci.first).addString(ci.second);
    }
};

// Generator for methods to be used in generate<>()
struct MethodGenerator {
    template<typename Method> static constexpr int offset() { return 1 + Method::argCount * 2; }
    template<int ParamIndex, typename State, typename Method>
    static constexpr auto generate(State s, Method method) {
        return s.addString(method.name). // name
                template add<Method::argCount,
                             ParamIndex, //parameters
                             1, //tag, always \0
                             adjustFlags(Method::flags)>();
    }
    // because public and private are inverted
    static constexpr uint adjustFlags(uint f) {
        return (f & W_Access::Protected.value) ? f : (f ^ W_Access::Private.value);
    }
};

/** Helper that computes if type T is a builtin QMetaType  */
template <typename T, typename = void> struct MetaTypeIdIsBuiltIn : std::false_type {};
template <typename T> struct MetaTypeIdIsBuiltIn<T, typename std::enable_if<QMetaTypeId2<T>::IsBuiltIn>::type> : std::true_type{};

/**
 * Helper to generate the type information of type 'T':
 * If T is a builtin QMetaType, its meta type id need to be added in the state.
 * If it's not builtin, then it would be an reference to T's name in the string array.
 */
template<typename T, bool Builtin = MetaTypeIdIsBuiltIn<T>::value>
struct HandleType {
    template<typename S, typename TypeStr = int>
    static constexpr auto result(const S&s, TypeStr = {})
    { return s.template add<QMetaTypeId2<T>::MetaType>(); }
};
template<typename T>
struct HandleType<T, false> {
    template<typename Strings, typename TypeStr = int>
    static constexpr auto result(const Strings &ss, TypeStr = {}) {
        return ss.addTypeString(W_TypeRegistery<T>::name);
        static_assert(W_TypeRegistery<T>::registered, "Please Register T with W_REGISTER_ARGTYPE");
    }
    template<typename Strings, int N>
    static constexpr auto result(const Strings &ss, StaticString<N> typeStr,
                                    typename std::enable_if<(N>1),int>::type=0) {
        return ss.addTypeString(typeStr);
    }
};

// Generator for properties to be used in generate<>()
struct PropertyGenerator {
    template<typename> static constexpr int offset() { return 0; }
    template<int, typename State, typename Prop>
    static constexpr auto generate(State s, Prop prop) {
        auto s2 = s.addString(prop.name);
        auto s3 = HandleType<typename Prop::PropertyType>::result(s2, prop.typeStr);
        constexpr uint moreFlags = (QtPrivate::IsQEnumHelper<typename Prop::PropertyType>::Value
            ? uint(PropertyFlags::EnumOrFlag) : 0);
        constexpr uint defaultFlags = 0 | PropertyFlags::Stored | PropertyFlags::Scriptable
            | PropertyFlags::Designable;
        return s3.template add<Prop::flags | moreFlags | defaultFlags>();
    }
};

// Generator for enums to be used in generate<>()
struct EnumGenerator {
    template<typename Enum> static constexpr int offset() { return Enum::count * 2; }
    template<int DataIndex, typename State, typename Enum>
    static constexpr auto generate(State s, Enum e) {
        return s.addString(e.name).template add< //name
                Enum::flags,
                Enum::count,
                DataIndex
            >();
    }
};

// Generator for enum values
struct EnumValuesGenerator {
    template<typename Strings>
    static constexpr auto generateSingleEnumValues(const Strings &s, std::index_sequence<>, binary::tree<>)
    { return s; }

    template<typename Strings, std::size_t Value, std::size_t... I, typename Names>
    static constexpr auto generateSingleEnumValues(const Strings &s, std::index_sequence<Value, I...>, Names names) {
        auto s2 = s.addString(binary::tree_head(names)).template add<uint(Value)>();
        return generateSingleEnumValues(s2, std::index_sequence<I...>{}, binary::tree_tail(names));
    }

    template<typename> static constexpr int offset() { return 0; }
    template<int, typename State, typename Enum>
    static constexpr auto generate(State s, Enum e) {
        return generateSingleEnumValues(s, typename Enum::Values{}, e.names);
    }
};

/**
 * Helper classes for MethodParametersGenerator::generateSingleMethodParameter:
 * generate the parameter array
 */
template<typename ...Args> struct HandleArgsHelper {
    template<typename Strings, typename ParamTypes>
    static constexpr auto result(const Strings &ss, const ParamTypes&)
    { return ss; }
};
template<typename A, typename... Args>
struct HandleArgsHelper<A, Args...> {
    template<typename Strings, typename ParamTypes>
    static constexpr auto result(const Strings &ss, const ParamTypes &paramTypes) {
        using Type = typename QtPrivate::RemoveConstRef<A>::Type;
        auto typeStr = binary::tree_head(paramTypes);
        using ts_t = decltype(typeStr);
        // This way, the overload of result will not pick the StaticString one if it is a tuple (because registered types have the priority)
        auto typeStr2 = std::conditional_t<std::is_same<A, Type>::value, ts_t, std::tuple<ts_t>>{typeStr};
        auto r1 = HandleType<Type>::result(ss, typeStr2);
        return HandleArgsHelper<Args...>::result(r1, binary::tree_tail(paramTypes));
    }
};
template<int N> struct HandleArgNames {
    template<typename Strings, typename Str>
    static constexpr auto result(const Strings &ss, StaticStringList<Str> pn)
    {
        auto s2 = ss.addString(binary::tree_head(pn));
        auto tail = binary::tree_tail(pn);
        return HandleArgNames<N-1>::result(s2, tail);
    }
    template<typename Strings> static constexpr auto result(const Strings &ss, StaticStringList<> pn)
    { return HandleArgNames<N-1>::result(ss.template add<1>(), pn); }
};
template<> struct HandleArgNames<0> {
    template<typename Strings, typename PN> static constexpr auto result(const Strings &ss, PN)
    { return ss; }
};

// Generator for method parameters to be used in generate<>()
struct MethodParametersGenerator {
private:
    // non-const function
    template<typename Strings, typename ParamTypes, typename ParamNames, typename Obj, typename Ret, typename... Args>
    static constexpr auto generateSingleMethodParameter(const Strings &ss, Ret (Obj::*)(Args...),
                                                        const ParamTypes &paramTypes, const ParamNames &paramNames ) {
        auto types = HandleArgsHelper<Ret, Args...>::result(ss, binary::tree_prepend(paramTypes, 0));
        return HandleArgNames<sizeof...(Args)>::result(types, paramNames);
    }
    template<typename Strings, typename ParamTypes, typename ParamNames, typename Obj, typename Ret, typename... Args>
    // const function
    static constexpr auto generateSingleMethodParameter(const Strings &ss, Ret (Obj::*)(Args...) const,
                                                 const ParamTypes &paramTypes, const ParamNames &paramNames ) {
        auto types = HandleArgsHelper<Ret, Args...>::result(ss, binary::tree_prepend(paramTypes, 0));
        return HandleArgNames<sizeof...(Args)>::result(types, paramNames);
    }
    // static member functions
    template<typename Strings, typename ParamTypes, typename ParamNames, typename Ret, typename... Args>
    static constexpr auto generateSingleMethodParameter(const Strings &ss, Ret (*)(Args...),
                                                 const ParamTypes &paramTypes, const ParamNames &paramNames ) {
        auto types = HandleArgsHelper<Ret, Args...>::result(ss, binary::tree_prepend(paramTypes, 0));
        return HandleArgNames<sizeof...(Args)>::result(types, paramNames);
    }
public:

    template<typename> static constexpr int offset() { return 0; }
    template<int, typename State, typename Method>
    static constexpr auto generate(State s, Method method) {
        // Dispatch either  non const function, const function or static function
        return generateSingleMethodParameter(s, method.func, method.paramTypes, method.paramNames);
    }
};

// Generator for constructor parameter to be used in generate<>()
struct ConstructorParametersGenerator {
    template<typename> static constexpr int offset() { return 0; }
    template<int, typename State, int N, typename... Args>
    static constexpr auto generate(State s, MetaConstructorInfo<N,Args...>) {
        auto s2 = s.template add<IsUnresolvedType | 1>();
        auto s3 = HandleArgsHelper<Args...>::result(s2, binary::tree<>{});
        return s3.template add<((void)sizeof(Args),1)...>(); // all the names are 1 (for "\0")
    }
};

/** Given method, a binary::tree containing information about methods or constructor,
 *  return the amount of item it will add in the int array. */
template<typename Methods, std::size_t... I>
constexpr int paramOffset(std::index_sequence<I...>)
{ return sums(int(1 + binary::tree_element_t<I, Methods>::argCount * 2)...); }

/**
 * Generate the integer array and the lists of string
 */
template<typename T, typename ObjI>
constexpr auto generateDataArray(const ObjI &objectInfo) {

    constexpr bool hasNotify = hasNotifySignal<T>(make_index_sequence<ObjI::propertyCount>{});
    constexpr int classInfoOffstet = 14;
    constexpr int methodOffset = classInfoOffstet + ObjI::classInfoCount * 2;
    constexpr int propertyOffset = methodOffset + ObjI::methodCount * 5;
    constexpr int enumOffset = propertyOffset + ObjI::propertyCount * (hasNotify ? 4: 3);
    constexpr int constructorOffset = enumOffset + ObjI::enumCount* 4;
    constexpr int paramIndex = constructorOffset + ObjI::constructorCount * 5 ;
    constexpr int constructorParamIndex = paramIndex +
        paramOffset<decltype(objectInfo.methods)>(make_index_sequence<ObjI::methodCount>{});
    constexpr int enumValueOffset = constructorParamIndex +
        paramOffset<decltype(objectInfo.constructors)>(make_index_sequence<ObjI::constructorCount>{});

    auto stringData = binary::tree_append(binary::tree_append(binary::tree<>{} , objectInfo.name), StaticString<1>(""));
    IntermediateState<decltype(stringData),
            7,       // revision
            0,       // classname
            ObjI::classInfoCount,  classInfoOffstet, // classinfo
            ObjI::methodCount,   methodOffset, // methods
            ObjI::propertyCount,    propertyOffset, // properties
            ObjI::enumCount,    enumOffset, // enums/sets
            ObjI::constructorCount, constructorOffset, // constructors
            0x4 /* PropertyAccessInStaticMetaCall */,   // flags
            ObjI::signalCount
        > header = { stringData };

    auto classInfos = generate<ClassInfoGenerator, paramIndex>(header , objectInfo.classInfos);
    auto methods = generate<MethodGenerator, paramIndex>(classInfos , objectInfo.methods);
    auto properties = generate<PropertyGenerator, 0>(methods, objectInfo.properties);
    auto notify = generateNotifySignals<T>(properties, std::integral_constant<bool, hasNotify>{},
                                            make_index_sequence<ObjI::propertyCount>{});
    auto enums = generate<EnumGenerator, enumValueOffset>(notify, objectInfo.enums);
    auto constructors = generate<MethodGenerator, constructorParamIndex>(enums, objectInfo.constructors);
    auto parametters = generate<MethodParametersGenerator, 0>(constructors, objectInfo.methods);
    auto parametters2 = generate<ConstructorParametersGenerator, 0>(parametters, objectInfo.constructors);
    auto enumValues = generate<EnumValuesGenerator, 0>(parametters2, objectInfo.enums);
    return std::make_pair(enumValues.strings, enumValues.sequence);
}

/**
 * Holder for the string data.  Just like in the moc generated code.
 */
template<int N, int L> struct qt_meta_stringdata_t {
     QByteArrayData data[N];
     char stringdata[L];
};

/** Builds the string data
 * \param S: a index_sequence that goes from 0 to the full size of the strings
 * \param I: a index_sequence that goes from 0 to the number of string
 * \param O: a index_sequence of the offsets
 * \param N: a index_sequence of the size of each strings
 * \param T: the W_MetaObjectCreatorHelper
 */
template<typename S, typename I, typename O, typename N, typename T> struct BuildStringDataHelper;
template<std::size_t... S, std::size_t... I, std::size_t... O, std::size_t...N, typename T>
struct BuildStringDataHelper<std::index_sequence<S...>, std::index_sequence<I...>, std::index_sequence<O...>, std::index_sequence<N...>, T> {
    using meta_stringdata_t = const qt_meta_stringdata_t<sizeof...(I), sizeof...(S)>;
    static meta_stringdata_t qt_meta_stringdata;
};
template<std::size_t... S, std::size_t... I, std::size_t... O, std::size_t...N, typename T>
const qt_meta_stringdata_t<sizeof...(I), sizeof...(S)>
BuildStringDataHelper<std::index_sequence<S...>, std::index_sequence<I...>, std::index_sequence<O...>, std::index_sequence<N...>, T>::qt_meta_stringdata = {
    {Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(N-1,
            qptrdiff(offsetof(meta_stringdata_t, stringdata) + O - I * sizeof(QByteArrayData)) )...},
    { concatenate(T::string_data)[S]... }
};

/**
 * Given N a list of string sizes, compute the list offsets to each of the strings.
 */
template<std::size_t... N> struct ComputeOffsets;
template<> struct ComputeOffsets<> {
    using Result = std::index_sequence<>;
};
template<std::size_t H, std::size_t... T> struct ComputeOffsets<H, T...> {
    template<std::size_t ... I> static std::index_sequence<0, (I+H)...> func(std::index_sequence<I...>);
    using Result = decltype(func(typename ComputeOffsets<T...>::Result()));
};

/**
 * returns the string data suitable for the QMetaObject from a list of string
 * T is W_MetaObjectCreatorHelper<ObjectType>
 */
// N... are all the sizes of the strings
template<typename T, int... N>
constexpr const QByteArrayData *build_string_data()  {
    return BuildStringDataHelper<make_index_sequence<sums(N...)>,
                                 make_index_sequence<sizeof...(N)>,
                                 typename ComputeOffsets<N...>::Result,
                                 std::index_sequence<N...>,
                                  T>
        ::qt_meta_stringdata.data;
}
template<typename T, std::size_t... I>
constexpr const QByteArrayData *build_string_data(std::index_sequence<I...>)  {
    return build_string_data<T, decltype(binary::get<I>(T::string_data))::size...>();
}


/**
 * returns a pointer to an array of string built at compile time.
 */
template<typename I> struct build_int_data;
template<std::size_t... I> struct build_int_data<std::index_sequence<I...>> {
    static const uint data[sizeof...(I)];
};
template<std::size_t... I> const uint build_int_data<std::index_sequence<I...>>::data[sizeof...(I)] = { uint(I)... };

/** Returns the QMetaObject* of the base type. Use SFINAE to only return it if it exists */
template<typename T>
static constexpr auto parentMetaObject(int) W_RETURN(&T::W_BaseType::staticMetaObject)
template<typename T>
static constexpr const QMetaObject *parentMetaObject(...) { return nullptr; }

// Bunch of helpers for propertyOperation
// this uses SFINAE of the return value to work either with F a pointer to member, or to member function
// or nullptr.
template <typename F, typename O, typename T>
inline auto propSet(F f, O *o, const T &t) W_RETURN(((o->*f)(t),0))
template <typename F, typename O, typename T>
inline auto propSet(F f, O *o, const T &t) W_RETURN(o->*f = t)
template <typename O, typename T>
inline void propSet(std::nullptr_t, O *, const T &) {}

template <typename F, typename O, typename T>
inline auto propGet(F f, O *o, T &t) W_RETURN(t = (o->*f)())
template <typename F, typename O, typename T>
inline auto propGet(F f, O *o, T &t) W_RETURN(t = o->*f)
template <typename O, typename T>
inline void propGet(std::nullptr_t, O *, T &) {}

template <typename F, typename M, typename O>
inline auto propNotify(F f, M m, O *o) W_RETURN(((o->*f)(o->*m),0))
template <typename F, typename M, typename O>
inline auto propNotify(F f, M, O *o) W_RETURN(((o->*f)(),0))
template <typename... T>
inline void propNotify(T...) {}

template <typename F, typename O>
inline auto propReset(F f, O *o) W_RETURN(((o->*f)(),0))
template <typename... T>
inline void propReset(T...) {}

struct FriendHelper {
/** Instancies the QMetaObject for class T */
template<typename T>
static constexpr QMetaObject createMetaObject()
{
    using Creator = typename T::W_MetaObjectCreatorHelper;
    auto string_data = build_string_data<Creator>(make_index_sequence<Creator::string_data.size>());
    auto int_data = build_int_data<typename std::remove_const<decltype(Creator::int_data)>::type>::data;
    return { { parentMetaObject<T>(0) , string_data , int_data,  T::qt_static_metacall, {}, {} }  };
}

/** Implementation of qt_metacall */
template<typename T> static int qt_metacall_impl(T *_o, QMetaObject::Call _c, int _id, void** _a) {
    using Creator = typename T::W_MetaObjectCreatorHelper;
    _id = _o->T::W_BaseType::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod || _c == QMetaObject::RegisterMethodArgumentMetaType) {
        constexpr int methodCount = Creator::objectInfo.methodCount;
        if (_id < methodCount)
            T::qt_static_metacall(_o, _c, _id, _a);
        _id -= methodCount;
    } else if ((_c >= QMetaObject::ReadProperty && _c <= QMetaObject::QueryPropertyUser)
                || _c == QMetaObject::RegisterPropertyMetaType) {
        constexpr int propertyCount = Creator::objectInfo.propertyCount;
        if (_id < propertyCount)
            T::qt_static_metacall(_o, _c, _id, _a);
        _id -= propertyCount;
    }
    return _id;
}

/**
 * Helper for implementation of qt_static_metacall for QMetaObject::IndexOfMethod
 * T is the class, and I is the index of a method.
 * Returns I if the argument is equal to the pointer to member function of the signal of index 'I'
 */
template<typename T, int I>
static int indexOfMethod(void **func) {
    constexpr auto f = binary::get<I>(T::W_MetaObjectCreatorHelper::objectInfo.methods).func;
    using Ms = decltype(T::W_MetaObjectCreatorHelper::objectInfo.methods);
    if ((binary::tree_element_t<I,Ms>::flags & 0xc) == W_MethodType::Signal.value
        && f == *reinterpret_cast<decltype(f)*>(func))
        return I;
    return -1;
}

/**
 * Helper for implementation of qt_static_metacall for QMetaObject::InvokeMetaMethod
 * T is the class, and I is the index of a method.
 * Invoke the method with index I if id == I.
 */
template <typename T, int I>
static void invokeMethod(T *_o, int _id, void **_a) {
    if (_id == I) {
        constexpr auto f = binary::get<I>(T::W_MetaObjectCreatorHelper::objectInfo.methods).func;
        using P = QtPrivate::FunctionPointer<std::remove_const_t<decltype(f)>>;
        P::template call<typename P::Arguments, typename P::ReturnType>(f, _o, _a);
    }
}

/**
 * Helper for implementation of qt_static_metacall for QMetaObject::RegisterMethodArgumentMetaType
 * T is the class, and I is the index of a method.
 */
template <typename T, int I>
static void registerMethodArgumentType(int _id, void **_a) {
    if (_id == I) {
        constexpr auto f = binary::get<I>(T::W_MetaObjectCreatorHelper::objectInfo.methods).func;
        using P = QtPrivate::FunctionPointer<std::remove_const_t<decltype(f)>>;
        auto _t = QtPrivate::ConnectionTypes<typename P::Arguments>::types();
        uint arg = *reinterpret_cast<int*>(_a[1]);
        *reinterpret_cast<int*>(_a[0]) = _t && arg < P::ArgumentCount ?
                _t[arg] : -1;
    }
}

/**
 * Helper for implementation of qt_static_metacall for any of the operations in a property
 * T is the class, and I is the index of a property.
 */
template<typename T, int I>
static void propertyOperation(T *_o, QMetaObject::Call _c, int _id, void **_a) {
    if (_id != I)
        return;
    constexpr auto p = binary::get<I>(T::W_MetaObjectCreatorHelper::objectInfo.properties);
    using Type = typename decltype(p)::PropertyType;
    switch(+_c) {
    case QMetaObject::ReadProperty:
        if (p.getter != nullptr) {
            propGet(p.getter, _o, *reinterpret_cast<Type*>(_a[0]));
        } else if (p.member != nullptr) {
            propGet(p.member, _o, *reinterpret_cast<Type*>(_a[0]));
        }
        break;
    case QMetaObject::WriteProperty:
        if (p.setter != nullptr) {
            propSet(p.setter, _o, *reinterpret_cast<Type*>(_a[0]));
        } else if (p.member != nullptr) {
            propSet(p.member, _o, *reinterpret_cast<Type*>(_a[0]));
            propNotify(p.notify, p.member, _o);
        }
        break;
    case QMetaObject::ResetProperty:
        if (p.reset != nullptr) {
            propReset(p.reset, _o);
        }
        break;
    case QMetaObject::RegisterPropertyMetaType:
        *reinterpret_cast<int*>(_a[0]) = QtPrivate::QMetaTypeIdHelper<Type>::qt_metatype_id();
    }
}

/**
 * Helper for implementation of qt_static_metacall for QMetaObject::CreateInstance
 * T is the class, and I is the index of a constructor.
 */
template<typename T, int I>
static void createInstance(int _id, void** _a) {
    if (_id == I) {
        constexpr auto m = binary::get<I>(T::W_MetaObjectCreatorHelper::objectInfo.constructors);
        m.template createInstance<T>(_a, make_index_sequence<decltype(m)::argCount>{});
    }
}

/**
 * Function that does nothing but helps to expand template packs.
 */
template<typename...Ts> static constexpr void nop(Ts...) {}

/**
 * Implementation of qt_static_metacall for W_OBJECT_IMPL which should be called with
 * std::index_sequence for the amount of method, constructor, and properties.
 */
template<typename T, size_t...MethI, size_t ...ConsI, size_t...PropI>
static void qt_static_metacall_impl(QObject *_o, QMetaObject::Call _c, int _id, void** _a,
                        std::index_sequence<MethI...>, std::index_sequence<ConsI...>, std::index_sequence<PropI...>) {
    Q_UNUSED(_id) Q_UNUSED(_o) Q_UNUSED(_a)
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(T::staticMetaObject.cast(_o));
        nop((invokeMethod<T, MethI>(static_cast<T*>(_o), _id, _a),0)...);
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        nop((registerMethodArgumentType<T,MethI>(_id, _a),0)...);
    } else if (_c == QMetaObject::IndexOfMethod) {
        *reinterpret_cast<int *>(_a[0]) = sums((1+indexOfMethod<T,MethI>(reinterpret_cast<void **>(_a[1])))...)-1;
    } else if (_c == QMetaObject::CreateInstance) {
        nop((createInstance<T, ConsI>(_id, _a),0)...);
    } else if ((_c >= QMetaObject::ReadProperty && _c <= QMetaObject::QueryPropertyUser)
            || _c == QMetaObject::RegisterPropertyMetaType) {
        nop((propertyOperation<T,PropI>(static_cast<T*>(_o), _c, _id, _a),0)...);
    }
}

/** Same for W_GADGET */
template<typename T, size_t...MethI, size_t ...ConsI, size_t...PropI>
static void qt_static_metacall_impl(T *_o, QMetaObject::Call _c, int _id, void** _a,
                        std::index_sequence<MethI...>, std::index_sequence<ConsI...>, std::index_sequence<PropI...>) {
    Q_UNUSED(_id) Q_UNUSED(_o) Q_UNUSED(_a)
    if (_c == QMetaObject::InvokeMetaMethod) {
        nop((invokeMethod<T, MethI>(_o, _id, _a),0)...);
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        nop((registerMethodArgumentType<T,MethI>(_id, _a),0)...);
    } else if (_c == QMetaObject::IndexOfMethod) {
        Q_ASSERT_X(false, "qt_static_metacall", "IndexOfMethod called on a Q_GADGET");
    } else if (_c == QMetaObject::CreateInstance) {
        nop((createInstance<T, ConsI>(_id, _a),0)...);
    } else if ((_c >= QMetaObject::ReadProperty && _c <= QMetaObject::QueryPropertyUser)
            || _c == QMetaObject::RegisterPropertyMetaType) {
        nop((propertyOperation<T,PropI>(_o, _c, _id, _a),0)...);
    }
}

/** helper for the implementation of qt_metacast */
template <typename Interface, typename T>
static void interfaceMetaCast(void *&result, T *o, const char *clname) {
    const char *iid = qobject_interface_iid<Interface>();
    if (iid && !strcmp(clname, iid))
        result = static_cast<Interface>(o);
}

/** implementation of qt_metacast */
template<typename T, size_t... InterfaceI>
static void *qt_metacast_impl(T *o, const char *_clname, std::index_sequence<InterfaceI...>) {
    if (!_clname)
        return nullptr;
    const QByteArrayDataPtr sd = { const_cast<QByteArrayData*>(T::staticMetaObject.d.stringdata) };
    if (_clname == QByteArray(sd))
        return o;
    void *result = nullptr;
    nop((interfaceMetaCast<decltype(binary::get<InterfaceI>(T::W_MetaObjectCreatorHelper::objectInfo.interfaces))>(result, o, _clname),0)...);
    return result ? result : o->T::W_BaseType::qt_metacast(_clname);
}

}; // FriendHelper

// Forward to the FriendHelper
template<typename T> constexpr auto createMetaObject() {  return FriendHelper::createMetaObject<T>(); }
template<typename T, typename... Ts> auto qt_metacall_impl(Ts &&...args)
{ return FriendHelper::qt_metacall_impl<T>(std::forward<Ts>(args)...); }
template<typename T> auto qt_metacast_impl(T *o, const char *_clname) {
    using ObjI = decltype(T::W_MetaObjectCreatorHelper::objectInfo);
    return FriendHelper::qt_metacast_impl<T>(o, _clname,
                                              make_index_sequence<ObjI::interfaceCount>{});
}
template<typename T, typename... Ts> auto qt_static_metacall_impl(Ts &&... args) {
    using ObjI = decltype(T::W_MetaObjectCreatorHelper::objectInfo);
    return FriendHelper::qt_static_metacall_impl<T>(std::forward<Ts>(args)...,
                                                     make_index_sequence<ObjI::methodCount>{},
                                                     make_index_sequence<ObjI::constructorCount>{},
                                                     make_index_sequence<ObjI::propertyCount>{});
}
} // w_internal

#ifdef Q_OS_WIN
// On Windows, taking the address of exported symbols is not a constexpr. (At least with MinGW 5.3)
// So the staticMetaObject can't be constexpr because of the pointer to the base class QMetaObject
#define W_STATICMETAOBJECT_CONSTEXPR
#else
#define W_STATICMETAOBJECT_CONSTEXPR constexpr
#endif

#define W_OBJECT_IMPL_COMMON(TYPE, ...) \
    __VA_ARGS__ struct W_MACRO_REMOVEPAREN(TYPE)::W_MetaObjectCreatorHelper { \
        static constexpr auto objectInfo = \
            w_internal::makeObjectInfo<W_MACRO_REMOVEPAREN(TYPE)>(W_MACRO_STRIGNIFY(W_MACRO_REMOVEPAREN(TYPE))); \
        static constexpr auto data = w_internal::generateDataArray<W_MACRO_REMOVEPAREN(TYPE)>(objectInfo); \
        static constexpr auto string_data = data.first; \
        static constexpr auto int_data = data.second; \
    }; \
    __VA_ARGS__ W_STATICMETAOBJECT_CONSTEXPR const QMetaObject W_MACRO_REMOVEPAREN(TYPE)::staticMetaObject = \
        w_internal::createMetaObject<W_MACRO_REMOVEPAREN(TYPE)>();

#define W_OBJECT_IMPL(TYPE, ...) \
    W_OBJECT_IMPL_COMMON(TYPE, __VA_ARGS__) \
    __VA_ARGS__ void W_MACRO_REMOVEPAREN(TYPE)::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void** _a) \
    { w_internal::qt_static_metacall_impl<W_MACRO_REMOVEPAREN(TYPE)>(_o, _c, _id, _a); } \
    __VA_ARGS__ const QMetaObject *W_MACRO_REMOVEPAREN(TYPE)::metaObject() const  { return &staticMetaObject; } \
    __VA_ARGS__ void *W_MACRO_REMOVEPAREN(TYPE)::qt_metacast(const char *_clname) \
    { return w_internal::qt_metacast_impl<W_MACRO_REMOVEPAREN(TYPE)>(this, _clname); } \
    __VA_ARGS__ int W_MACRO_REMOVEPAREN(TYPE)::qt_metacall(QMetaObject::Call _c, int _id, void** _a) \
    { return w_internal::qt_metacall_impl<W_MACRO_REMOVEPAREN(TYPE)>(this, _c, _id, _a); }


#define W_GADGET_IMPL(TYPE, ...) \
    W_OBJECT_IMPL_COMMON(TYPE, __VA_ARGS__) \
    __VA_ARGS__ void W_MACRO_REMOVEPAREN(TYPE)::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void** _a) \
    { w_internal::qt_static_metacall_impl<W_MACRO_REMOVEPAREN(TYPE)>(reinterpret_cast<W_MACRO_REMOVEPAREN(TYPE)*>(_o), _c, _id, _a); }
