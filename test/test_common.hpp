//  Copyright (C) 2011 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include "test_helpers.hpp"
#include <cassert>
#include <iostream>

#include <boost/array.hpp>
#include <boost/test/test_tools.hpp>
#include <boost/thread.hpp>

namespace impl {

using boost::array;
using namespace boost;
using namespace std;

template < bool Bounded = false >
struct queue_stress_tester
{
    static const unsigned int buckets = 1 << 13;
#ifndef BOOST_LOCKFREE_STRESS_TEST
    static const long node_count = 5000;
#else
    static const long node_count = 500000;
#endif
    const int reader_threads;
    const int writer_threads;

    boost::lockfree::detail::atomic< int > writers_finished;

    static_hashed_set< long, buckets > data;
    static_hashed_set< long, buckets > dequeued;
    array< std::set< long >, buckets > returned;

    boost::lockfree::detail::atomic< int > push_count, pop_count;

    queue_stress_tester( int reader, int writer ) :
        reader_threads( reader ),
        writer_threads( writer ),
        push_count( 0 ),
        pop_count( 0 )
    {}

    template < typename queue >
    void add_items( queue& stk )
    {
        for ( long i = 0; i != node_count; ++i ) {
            long id = generate_id< long >();

            bool inserted = data.insert( id );
            assert( inserted );
            (void)inserted;

            if ( Bounded )
                while ( stk.bounded_push( id ) == false ) {
#ifdef __VXWORKS__
                    thread::yield();
#endif
                }
            else
                while ( stk.push( id ) == false ) {
#ifdef __VXWORKS__
                    thread::yield();
#endif
                }
            ++push_count;
        }
        writers_finished += 1;
    }

    boost::lockfree::detail::atomic< bool > running;

    template < typename queue >
    bool consume_element( queue& q )
    {
        long id;
        bool ret = q.pop( id );

        if ( !ret )
            return false;

        bool erased   = data.erase( id );
        bool inserted = dequeued.insert( id );
        (void)erased;
        (void)inserted;
        assert( erased );
        assert( inserted );
        ++pop_count;
        return true;
    }

    template < typename queue >
    void get_items( queue& q )
    {
        for ( ;; ) {
            bool received_element = consume_element( q );
            if ( received_element )
                continue;

            if ( writers_finished.load() == writer_threads )
                break;

#ifdef __VXWORKS__
            thread::yield();
#endif
        }

        while ( consume_element( q ) )
            ;
    }

    template < typename queue >
    void run( queue& stk )
    {
        BOOST_WARN( stk.is_lock_free() );
        writers_finished.store( 0 );

        thread_group writer;
        thread_group reader;

        BOOST_TEST_REQUIRE( stk.empty() );

        for ( int i = 0; i != reader_threads; ++i )
            reader.create_thread( boost::bind( &queue_stress_tester::template get_items< queue >,
                                               this,
                                               boost::ref( stk ) ) );

        for ( int i = 0; i != writer_threads; ++i )
            writer.create_thread( boost::bind( &queue_stress_tester::template add_items< queue >,
                                               this,
                                               boost::ref( stk ) ) );

        std::cout << "threads created" << std::endl;

        writer.join_all();

        std::cout << "writer threads joined, waiting for readers" << std::endl;

        reader.join_all();

        std::cout << "reader threads joined" << std::endl;

        BOOST_TEST_REQUIRE( data.count_nodes() == (size_t)0 );
        BOOST_TEST_REQUIRE( stk.empty() );

        BOOST_TEST_REQUIRE( push_count == pop_count );
        BOOST_TEST_REQUIRE( push_count == writer_threads * node_count );
    }
};

} // namespace impl

using impl::queue_stress_tester;
