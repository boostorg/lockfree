//  Copyright (C) 2026 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

// Stress coverage for queue with hazard-pointer reclamation, reusing the
// container-agnostic testers from test_common.hpp.

#include <boost/lockfree/queue.hpp>

#define BOOST_TEST_MAIN
#ifdef BOOST_LOCKFREE_INCLUDE_TESTS
#    include <boost/test/included/unit_test.hpp>
#else
#    include <boost/test/unit_test.hpp>
#endif

#include "test_common.hpp"

namespace {

typedef boost::lockfree::queue< long, boost::lockfree::reclamation< boost::lockfree::reclamation_scheme::hazard_pointers > >
    hp_queue;

} // namespace

BOOST_AUTO_TEST_CASE( queue_hp_stress_unbounded )
{
    typedef queue_stress_tester< false > tester_type;
    std::unique_ptr< tester_type >       tester( new tester_type( 4, 4 ) );

    hp_queue q( 128 );
    tester->run( q );
}

BOOST_AUTO_TEST_CASE( queue_hp_stress_unbounded_asymmetric_many_writers )
{
    typedef queue_stress_tester< false > tester_type;
    std::unique_ptr< tester_type >       tester( new tester_type( 1, 4 ) );

    hp_queue q( 128 );
    tester->run( q );
}

BOOST_AUTO_TEST_CASE( queue_hp_stress_unbounded_asymmetric_many_readers )
{
    typedef queue_stress_tester< false > tester_type;
    std::unique_ptr< tester_type >       tester( new tester_type( 4, 1 ) );

    hp_queue q( 128 );
    tester->run( q );
}

BOOST_AUTO_TEST_CASE( queue_hp_comprehensive_stress_4_consumers )
{
    comprehensive_stress_tester< 10, 4 > tester;
    hp_queue                             q( 128 );
    tester.run( q );
}
