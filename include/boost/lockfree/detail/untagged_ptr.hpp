//  untagged pointer, for use with hazard-pointer reclamation (no ABA tag needed)
//
//  Copyright (C) 2026 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_DETAIL_UNTAGGED_PTR_HPP_INCLUDED
#define BOOST_LOCKFREE_DETAIL_UNTAGGED_PTR_HPP_INCLUDED

#include <cstddef>
#include <cstdint>

#include <boost/config.hpp>

namespace boost { namespace lockfree { namespace detail {

// Drop-in shape-compatible replacement for tagged_ptr<T>, used when
// hazard pointers provide ABA safety and no tag needs to be stored.
// All tag operations are no-ops returning 0 so call sites of the form
// tagged_node_handle(ptr, old.get_next_tag()) keep compiling verbatim
// and fold away at compile time.

template < class T >
class untagged_ptr
{
public:
    typedef std::uint16_t tag_t;

    /** uninitialized constructor */
    untagged_ptr( void ) noexcept //: ptr(0)
    {}

    untagged_ptr( untagged_ptr const& p ) = default;

    explicit untagged_ptr( T* p, tag_t = 0 ) :
        ptr( p )
    {}

    /** unsafe set operation */
    /* @{ */
    untagged_ptr& operator=( untagged_ptr const& p ) = default;

    void set( T* p, tag_t /* t */ )
    {
        ptr = p;
    }
    /* @} */

    /** comparing semantics */
    /* @{ */
    bool operator==( untagged_ptr const& p ) const
    {
        return ptr == p.ptr;
    }

    bool operator==( volatile untagged_ptr const& p ) const volatile
    {
        return ptr == p.ptr;
    }

    bool operator==( volatile untagged_ptr const& p ) const
    {
        return ptr == p.ptr;
    }

    bool operator!=( untagged_ptr const& p ) const
    {
        return !operator==( p );
    }

    bool operator!=( volatile untagged_ptr const& p ) const volatile
    {
        return !operator==( p );
    }

    bool operator!=( volatile untagged_ptr const& p ) const
    {
        return !operator==( p );
    }
    /* @} */

    /** pointer access */
    /* @{ */
    T* get_ptr( void ) const
    {
        return ptr;
    }

    T* get_ptr( void ) const volatile
    {
        return ptr;
    }

    void set_ptr( T* p )
    {
        ptr = p;
    }
    /* @} */

    /** tag access (no-ops, tags are always 0) */
    /* @{ */
    tag_t get_tag() const
    {
        return 0;
    }

    tag_t get_tag() const volatile
    {
        return 0;
    }

    tag_t get_next_tag() const
    {
        return 0;
    }

    tag_t get_next_tag() const volatile
    {
        return 0;
    }

    void set_tag( tag_t /* t */ )
    {}
    /* @} */

    /** smart pointer support  */
    /* @{ */
    T& operator*() const
    {
        return *ptr;
    }

    T* operator->() const
    {
        return ptr;
    }

    operator bool( void ) const
    {
        return ptr != 0;
    }
    /* @} */

protected:
    T* ptr;
};

}}} // namespace boost::lockfree::detail

#endif /* BOOST_LOCKFREE_DETAIL_UNTAGGED_PTR_HPP_INCLUDED */
