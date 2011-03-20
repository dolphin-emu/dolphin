/////////////////////////////////////////////////////////////////////////////
// Name:        wx/sharedptr.h
// Purpose:     Shared pointer based on the counted_ptr<> template, which
//              is in the public domain
// Author:      Robert Roebling, Yonat Sharon
// RCS-ID:      $Id: sharedptr.h 67232 2011-03-18 15:10:15Z DS $
// Copyright:   Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SHAREDPTR_H_
#define _WX_SHAREDPTR_H_

#include "wx/defs.h"
#include "wx/atomic.h"

// ----------------------------------------------------------------------------
// wxSharedPtr: A smart pointer with non-intrusive reference counting.
// ----------------------------------------------------------------------------

template <class T>
class wxSharedPtr
{
public:
    typedef T element_type;

    wxEXPLICIT wxSharedPtr( T* ptr = NULL )
        : m_ref(NULL)
    {
        if (ptr)
            m_ref = new reftype(ptr);
    }

    ~wxSharedPtr()                           { Release(); }
    wxSharedPtr(const wxSharedPtr& tocopy)   { Acquire(tocopy.m_ref); }

    wxSharedPtr& operator=( const wxSharedPtr& tocopy )
    {
        if (this != &tocopy)
        {
            Release();
            Acquire(tocopy.m_ref);
        }
        return *this;
    }

    wxSharedPtr& operator=( T* ptr )
    {
        if (get() != ptr)
        {
            Release();
            if (ptr)
                m_ref = new reftype(ptr);
        }
        return *this;
    }

    // test for pointer validity: defining conversion to unspecified_bool_type
    // and not more obvious bool to avoid implicit conversions to integer types
    typedef T *(wxSharedPtr<T>::*unspecified_bool_type)() const;
    operator unspecified_bool_type() const
    {
        if (m_ref && m_ref->m_ptr)
           return  &wxSharedPtr<T>::get;
        else
           return NULL;
    }

    T& operator*() const
    {
        wxASSERT(m_ref != NULL);
        wxASSERT(m_ref->m_ptr != NULL);
        return *(m_ref->m_ptr);
    }

    T* operator->() const
    {
        wxASSERT(m_ref != NULL);
        wxASSERT(m_ref->m_ptr != NULL);
        return m_ref->m_ptr;
    }

    T* get() const
    {
        return m_ref ? m_ref->m_ptr : NULL;
    }

    void reset( T* ptr = NULL )
    {
        Release();
        if (ptr)
            m_ref = new reftype(ptr);
    }

    bool unique()   const    { return (m_ref ? m_ref->m_count == 1 : true); }
    long use_count() const   { return (m_ref ? (long)m_ref->m_count : 0); }

private:

    struct reftype
    {
        reftype( T* ptr = NULL, unsigned count = 1 ) : m_ptr(ptr), m_count(count) {}
        T*          m_ptr;
        wxAtomicInt m_count;
    }* m_ref;

    void Acquire(reftype* ref)
    {
        m_ref = ref;
        if (ref)
            wxAtomicInc( ref->m_count );
    }

    void Release()
    {
        if (m_ref)
        {
            wxAtomicDec( m_ref->m_count );
            if (m_ref->m_count == 0)
            {
                delete m_ref->m_ptr;
                delete m_ref;
            }
            m_ref = NULL;
        }
    }
};

template <class T, class U>
bool operator == (wxSharedPtr<T> const &a, wxSharedPtr<U> const &b )
{
    return a.get() == b.get();
}

template <class T, class U>
bool operator != (wxSharedPtr<T> const &a, wxSharedPtr<U> const &b )
{
    return a.get() != b.get();
}

#endif // _WX_SHAREDPTR_H_
