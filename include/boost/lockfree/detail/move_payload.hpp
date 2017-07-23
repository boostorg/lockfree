//  boost lockfree: move_payload helper
//
//  Copyright (C) 2011 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_DETAIL_MOVE_PAYLOAD_HPP_INCLUDED
#define BOOST_LOCKFREE_DETAIL_MOVE_PAYLOAD_HPP_INCLUDED

#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_convertible.hpp>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4512) // assignment operator could not be generated
#endif

namespace boost    {
namespace lockfree {
namespace detail   {

struct move_convertible
{
    template <typename T, typename U>
    static void move(T & t, U & u)
    {
        u = std::move(t);
    }
};

struct move_constructible_and_assignable
{
    template <typename T, typename U>
    static void move(T & t, U & u)
    {
        u = U(std::move(t));
    }
};

template <typename T, typename U>
void move_payload(T & t, U & u)
{
    typedef typename boost::mpl::if_<typename boost::is_convertible<T, U>::type,
                                     move_convertible,
                                     move_constructible_and_assignable
                                    >::type move_type;
    move_type::move(t, u);
}

template <typename T>
struct consume_via_move
{
    consume_via_move(T & out):
        out_(out)
    {}

    template <typename U>
    void operator()(U & element)
    {
        move_payload(element, out_);
    }

    T & out_;
};


}}}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif  /* BOOST_LOCKFREE_DETAIL_COPY_PAYLOAD_HPP_INCLUDED */
