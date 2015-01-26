//  boost lockfree: copy_payload helper
//
//  Copyright (C) 2011 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_DETAIL_COPY_PAYLOAD_HPP_INCLUDED
#define BOOST_LOCKFREE_DETAIL_COPY_PAYLOAD_HPP_INCLUDED

#include <boost/lockfree/detail/move.hpp>

namespace boost    {
namespace lockfree {
namespace detail   {

template <typename Target>
struct consume_and_store
{
    consume_and_store(Target& target):
        target_(target)
    {}

    template <typename Source>
    void operator()(Source BOOST_LOCKFREE_MOVE_RREF_OR_REF source)
    {
        target_ = BOOST_LOCKFREE_MOVE_MOVE(source);
    }

    Target& target_;
};

struct consume_noop
{
    template <typename U>
    void operator()(const U &)
    {
    }
};


}}}

#endif  /* BOOST_LOCKFREE_DETAIL_COPY_PAYLOAD_HPP_INCLUDED */
