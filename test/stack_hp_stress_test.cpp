//  Copyright (C) 2026 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

// Stress coverage for stack with hazard-pointer reclamation, reusing the
// container-agnostic testers from test_common.hpp.

#include <boost/lockfree/stack.hpp>

#define BOOST_TEST_MAIN
#ifdef BOOST_LOCKFREE_INCLUDE_TESTS
#    include <boost/test/included/unit_test.hpp>
#else
#    include <boost/test/unit_test.hpp>
#endif

#include "test_common.hpp"

namespace {

typedef boost::lockfree::stack< long, boost::lockfree::reclamation< boost::lockfree::reclamation_scheme::hazard_pointers > >
    hp_stack;

} // namespace

BOOST_AUTO_TEST_CASE( stack_hp_stress_unbounded )
{
    typedef queue_stress_tester< false > tester_type;
    std::unique_ptr< tester_type >       tester( new tester_type( 4, 4 ) );

    hp_stack q( 128 );
    tester->run( q );
}

BOOST_AUTO_TEST_CASE( stack_hp_stress_unbounded_asymmetric )
{
    typedef queue_stress_tester< false > tester_type;
    std::unique_ptr< tester_type >       tester( new tester_type( 1, 4 ) );

    hp_stack q( 128 );
    tester->run( q );
}

BOOST_AUTO_TEST_CASE( stack_hp_stress_consume_all_atomic )
{
    typedef stack_consume_all_atomic_stress_tester< false > tester_type;
    std::unique_ptr< tester_type >                          tester( new tester_type( 2, 4 ) );

    hp_stack q( 128 );
    tester->run_atomic( q );
}

BOOST_AUTO_TEST_CASE( stack_hp_stress_consume_all_atomic_reversed )
{
    typedef stack_consume_all_atomic_stress_tester< false > tester_type;
    std::unique_ptr< tester_type >                          tester( new tester_type( 2, 4 ) );

    hp_stack q( 128 );
    tester->run_atomic_reversed( q );
}

BOOST_AUTO_TEST_CASE( stack_hp_comprehensive_stress_4_consumers )
{
    comprehensive_stack_stress_tester< 10, 4 > tester;
    hp_stack                                   s( 128 );
    tester.run( s );
}
