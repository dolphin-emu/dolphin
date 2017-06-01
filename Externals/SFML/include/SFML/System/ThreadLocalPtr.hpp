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

#ifndef SFML_THREADLOCALPTR_HPP
#define SFML_THREADLOCALPTR_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/System/ThreadLocal.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
/// \brief Pointer to a thread-local variable
///
////////////////////////////////////////////////////////////
template <typename T>
class ThreadLocalPtr : private ThreadLocal
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// \param value Optional value to initialize the variable
    ///
    ////////////////////////////////////////////////////////////
    ThreadLocalPtr(T* value = NULL);

    ////////////////////////////////////////////////////////////
    /// \brief Overload of unary operator *
    ///
    /// Like raw pointers, applying the * operator returns a
    /// reference to the pointed-to object.
    ///
    /// \return Reference to the thread-local variable
    ///
    ////////////////////////////////////////////////////////////
    T& operator *() const;

    ////////////////////////////////////////////////////////////
    /// \brief Overload of operator ->
    ///
    /// Similarly to raw pointers, applying the -> operator
    /// returns the pointed-to object.
    ///
    /// \return Pointer to the thread-local variable
    ///
    ////////////////////////////////////////////////////////////
    T* operator ->() const;

    ////////////////////////////////////////////////////////////
    /// \brief Conversion operator to implicitly convert the
    ///        pointer to its raw pointer type (T*)
    ///
    /// \return Pointer to the actual object
    ///
    ////////////////////////////////////////////////////////////
    operator T*() const;

    ////////////////////////////////////////////////////////////
    /// \brief Assignment operator for a raw pointer parameter
    ///
    /// \param value Pointer to assign
    ///
    /// \return Reference to self
    ///
    ////////////////////////////////////////////////////////////
    ThreadLocalPtr<T>& operator =(T* value);

    ////////////////////////////////////////////////////////////
    /// \brief Assignment operator for a ThreadLocalPtr parameter
    ///
    /// \param right ThreadLocalPtr to assign
    ///
    /// \return Reference to self
    ///
    ////////////////////////////////////////////////////////////
    ThreadLocalPtr<T>& operator =(const ThreadLocalPtr<T>& right);
};

} // namespace sf

#include <SFML/System/ThreadLocalPtr.inl>


#endif // SFML_THREADLOCALPTR_HPP


////////////////////////////////////////////////////////////
/// \class sf::ThreadLocalPtr
/// \ingroup system
///
/// sf::ThreadLocalPtr is a type-safe wrapper for storing
/// pointers to thread-local variables. A thread-local
/// variable holds a different value for each different
/// thread, unlike normal variables that are shared.
///
/// Its usage is completely transparent, so that it is similar
/// to manipulating the raw pointer directly (like any smart pointer).
///
/// Usage example:
/// \code
/// MyClass object1;
/// MyClass object2;
/// sf::ThreadLocalPtr<MyClass> objectPtr;
///
/// void thread1()
/// {
///     objectPtr = &object1; // doesn't impact thread2
///     ...
/// }
///
/// void thread2()
/// {
///     objectPtr = &object2; // doesn't impact thread1
///     ...
/// }
///
/// int main()
/// {
///     // Create and launch the two threads
///     sf::Thread t1(&thread1);
///     sf::Thread t2(&thread2);
///     t1.launch();
///     t2.launch();
///
///     return 0;
/// }
/// \endcode
///
/// ThreadLocalPtr is designed for internal use; however you
/// can use it if you feel like it fits well your implementation.
///
////////////////////////////////////////////////////////////
