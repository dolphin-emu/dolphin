////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

namespace priv
{
// Base class for abstract thread functions
struct ThreadFunc
{
    virtual ~ThreadFunc() {}
    virtual void run() = 0;
};

// Specialization using a functor (including free functions) with no argument
template <typename T>
struct ThreadFunctor : ThreadFunc
{
    ThreadFunctor(T functor) : m_functor(functor) {}
    virtual void run() {m_functor();}
    T m_functor;
};

// Specialization using a functor (including free functions) with one argument
template <typename F, typename A>
struct ThreadFunctorWithArg : ThreadFunc
{
    ThreadFunctorWithArg(F function, A arg) : m_function(function), m_arg(arg) {}
    virtual void run() {m_function(m_arg);}
    F m_function;
    A m_arg;
};

// Specialization using a member function
template <typename C>
struct ThreadMemberFunc : ThreadFunc
{
    ThreadMemberFunc(void(C::*function)(), C* object) : m_function(function), m_object(object) {}
    virtual void run() {(m_object->*m_function)();}
    void(C::*m_function)();
    C* m_object;
};

} // namespace priv


////////////////////////////////////////////////////////////
template <typename F>
Thread::Thread(F functor) :
m_impl      (NULL),
m_entryPoint(new priv::ThreadFunctor<F>(functor))
{
}


////////////////////////////////////////////////////////////
template <typename F, typename A>
Thread::Thread(F function, A argument) :
m_impl      (NULL),
m_entryPoint(new priv::ThreadFunctorWithArg<F, A>(function, argument))
{
}


////////////////////////////////////////////////////////////
template <typename C>
Thread::Thread(void(C::*function)(), C* object) :
m_impl      (NULL),
m_entryPoint(new priv::ThreadMemberFunc<C>(function, object))
{
}
