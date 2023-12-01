//  Copyright (C) 2008-2016 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_LOCKFREE_FORWARD_HPP_INCLUDED
#define BOOST_LOCKFREE_FORWARD_HPP_INCLUDED


#ifndef BOOST_DOXYGEN_INVOKED

#    include <cstddef> // size_t

#    include <boost/config.hpp>

namespace boost { namespace lockfree {

// policies
template < bool IsFixedSized >
struct fixed_sized;

template < size_t Size >
struct capacity;

template < class Alloc >
struct allocator;


// data structures

template < typename T, typename... Options >
class queue;

template < typename T, typename... Options >
class stack;

template < typename T, typename... Options >
class spsc_queue;

}}     // namespace boost::lockfree

#endif // BOOST_DOXYGEN_INVOKED

#endif // BOOST_LOCKFREE_FORWARD_HPP_INCLUDED
