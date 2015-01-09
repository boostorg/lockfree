//  Copyright (C) 2014 Roman Odaisky
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_DETAIL_MOVE_HPP
#define BOOST_LOCKFREE_DETAIL_MOVE_HPP

#include <boost/config.hpp>

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

    #include <utility>

    #define BOOST_LOCKFREE_MOVE_RREF_OR_REF &&
    #define BOOST_LOCKFREE_MOVE_MOVE(_value_) std::move(_value_)

    #define BOOST_LOCKFREE_MOVE_MKFWD_(_type_) ForwardTo ## _type_
    #define BOOST_LOCKFREE_MOVE_MKFWD(_type_) BOOST_LOCKFREE_MOVE_MKFWD_(_type_)

    #define BOOST_LOCKFREE_MOVE_TEMPLATE(_type_) template <class BOOST_LOCKFREE_MOVE_MKFWD(_type_)>
    #define BOOST_LOCKFREE_MOVE_ARG_TYPE(_type_) BOOST_LOCKFREE_MOVE_MKFWD(_type_) &&
    #define BOOST_LOCKFREE_MOVE_FORWARD(_type_, _value_) std::forward<BOOST_LOCKFREE_MOVE_MKFWD(_type_)>((_value_))

#else

    #define BOOST_LOCKFREE_MOVE_RREF_OR_REF &
    #define BOOST_LOCKFREE_MOVE_MOVE(_value_) (_value_)

    #define BOOST_LOCKFREE_MOVE_TEMPLATE(_type_)
    #define BOOST_LOCKFREE_MOVE_ARG_TYPE(_type_) _type_ const &
    #define BOOST_LOCKFREE_MOVE_ARG(_type_, _value_) _value_

#endif

namespace boost {
namespace lockfree {
namespace detail {


}}}

#endif /* BOOST_LOCKFREE_DETAIL_MOVE_HPP */

