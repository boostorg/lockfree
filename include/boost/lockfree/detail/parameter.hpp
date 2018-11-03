// boost lockfree
//
// Copyright (C) 2011, 2016 Tim Blechmann
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_DETAIL_PARAMETER_HPP
#define BOOST_LOCKFREE_DETAIL_PARAMETER_HPP

#include <boost/lockfree/policies.hpp>
#include <boost/parameter/parameters.hpp>
#include <boost/parameter/binding.hpp>

#include <boost/type_traits/conditional.hpp>
#include <boost/type_traits/is_void.hpp>

#include <boost/lockfree/detail/allocator_rebind_helper.hpp>


namespace boost {
namespace lockfree {
namespace detail {

template <typename bound_args, typename tag_type>
struct has_arg
{
    typedef typename parameter::binding<bound_args, tag_type, void>::type type;
    static const bool value = !boost::is_void<type>::value;
};


template <typename bound_args>
struct extract_capacity
{
    static const bool has_capacity = has_arg<bound_args, tag::capacity>::value;

    static const std::size_t capacity =
        boost::conditional<has_capacity,
                           typename has_arg<bound_args, tag::capacity>::type,
                           boost::integral_constant< size_t, 0 >
                          >::type::value;
};


template <typename bound_args, typename T>
struct extract_allocator
{
    static const bool has_allocator = has_arg<bound_args, tag::allocator>::value;

    typedef typename boost::conditional<has_allocator,
                                        typename has_arg<bound_args, tag::allocator>::type,
                                        std::allocator<T>
                                       >::type allocator_arg;

    typedef typename detail::allocator_rebind_helper<allocator_arg, T>::type type;
};

template <typename bound_args, bool default_ = false>
struct extract_fixed_sized
{
    static const bool has_fixed_sized = has_arg<bound_args, tag::fixed_sized>::value;

    static const bool value =
        boost::conditional<has_fixed_sized,
                           typename has_arg<bound_args, tag::fixed_sized>::type,
                           boost::integral_constant<bool, default_>
                          >::type::value;
};


} /* namespace detail */
} /* namespace lockfree */
} /* namespace boost */

#endif /* BOOST_LOCKFREE_DETAIL_PARAMETER_HPP */
