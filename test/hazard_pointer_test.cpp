//  Copyright (C) 2026 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <atomic>
#include <thread>
#include <vector>

#include <boost/lockfree/detail/hazard_pointer.hpp>
#include <boost/lockfree/detail/untagged_ptr.hpp>

#define BOOST_TEST_MAIN
#ifdef BOOST_LOCKFREE_INCLUDE_TESTS
#    include <boost/test/included/unit_test.hpp>
#else
#    include <boost/test/unit_test.hpp>
#endif

namespace {

struct test_node
{
    explicit test_node( int v = 0 ) :
        value( v )
    {
        live.fetch_add( 1, std::memory_order_relaxed );
    }

    ~test_node()
    {
        live.fetch_sub( 1, std::memory_order_relaxed );
    }

    int                       value;
    static std::atomic< int > live;
};

std::atomic< int > test_node::live( 0 );

template < typename T >
struct counting_allocator : std::allocator< T >
{
    template < typename U >
    struct rebind
    {
        typedef counting_allocator< U > other;
    };

    counting_allocator()
    {}

    template < typename U >
    counting_allocator( counting_allocator< U > const& )
    {}

    T* allocate( std::size_t n )
    {
        allocated.fetch_add( static_cast< int >( n ), std::memory_order_relaxed );
        return std::allocator< T >::allocate( n );
    }

    void deallocate( T* p, std::size_t n )
    {
        deallocated.fetch_add( static_cast< int >( n ), std::memory_order_relaxed );
        std::allocator< T >::deallocate( p, n );
    }

    static std::atomic< int > allocated;
    static std::atomic< int > deallocated;
    static void               reset()
    {
        allocated.store( 0 );
        deallocated.store( 0 );
    }
};

template < typename T >
std::atomic< int > counting_allocator< T >::allocated( 0 );

template < typename T >
std::atomic< int > counting_allocator< T >::deallocated( 0 );

typedef boost::lockfree::detail::hp_domain< test_node, counting_allocator< test_node >, 2 > test_domain;
typedef boost::lockfree::detail::untagged_ptr< test_node >                                  test_handle;

test_node* make_node( int v )
{
    counting_allocator< test_node > alloc;
    test_node*                      n = alloc.allocate( 1 );
    new ( n ) test_node( v );
    return n;
}

} // namespace

BOOST_AUTO_TEST_CASE( untagged_ptr_test )
{
    using namespace boost::lockfree::detail;
    int a( 1 ), b( 2 );

    {
        untagged_ptr< int > i( &a, 0 );
        untagged_ptr< int > j( &b, 1 ); // tag is ignored

        BOOST_TEST_REQUIRE( i.get_tag() == 0 );
        BOOST_TEST_REQUIRE( i.get_next_tag() == 0 );
        BOOST_TEST_REQUIRE( j.get_tag() == 0 );

        i = j;
        BOOST_TEST_REQUIRE( i.get_ptr() == &b );
    }

    {
        untagged_ptr< int > i( &a, 0 );
        untagged_ptr< int > j( i );

        BOOST_TEST_REQUIRE( i.get_ptr() == j.get_ptr() );
        BOOST_TEST_REQUIRE( i == j );
        BOOST_TEST_REQUIRE( !( i != j ) );
    }

    {
        untagged_ptr< int > null_node( (int*)0, 0 );
        BOOST_TEST_REQUIRE( !null_node );
        untagged_ptr< int > nn( &a, 0 );
        BOOST_TEST_REQUIRE( (bool)nn );
        BOOST_TEST_REQUIRE( nn.operator->() == &a );
        BOOST_TEST_REQUIRE( *nn == 1 );
    }

    {
        // usable in atomics, like the tagged pointer
        std::atomic< untagged_ptr< int > > at;
        at.store( untagged_ptr< int >( &a, 0 ) );
        BOOST_TEST_REQUIRE( at.load().get_ptr() == &a );
        BOOST_TEST_REQUIRE( at.is_lock_free() );
    }
}

BOOST_AUTO_TEST_CASE( hazard_pointer_retire_and_flush )
{
    counting_allocator< test_node >::reset();
    test_node::live.store( 0 );
    {
        test_domain dom;
        for ( int i = 0; i != 100; ++i )
            dom.retire( make_node( i ) );
        dom.flush_all();
        BOOST_TEST_REQUIRE( test_node::live.load() == 0 );
    }
    BOOST_TEST_REQUIRE( counting_allocator< test_node >::allocated.load()
                        == counting_allocator< test_node >::deallocated.load() );
}

BOOST_AUTO_TEST_CASE( hazard_pointer_protection_blocks_reclaim )
{
    test_node::live.store( 0 );
    {
        test_domain                dom;
        std::atomic< test_handle > src;
        test_node*                 victim = make_node( 42 );
        src.store( test_handle( victim, 0 ) );

        {
            test_domain::guard g = dom.acquire();
            BOOST_TEST_REQUIRE( g.owns_record() );
            test_node* p = dom.protect< 0 >( g, src ).get_ptr();
            BOOST_TEST_REQUIRE( p == victim );

            dom.retire( victim );
            // force scans while the victim is protected
            for ( int i = 0; i != 200; ++i )
                dom.retire( make_node( i ) );

            BOOST_TEST_REQUIRE( victim->value == 42 ); // still valid
        }

        // guard released: further retire traffic must reclaim the victim
        for ( int i = 0; i != 200; ++i )
            dom.retire( make_node( i ) );
        dom.flush_all();
        BOOST_TEST_REQUIRE( test_node::live.load() == 0 );
    }
}

BOOST_AUTO_TEST_CASE( hazard_pointer_multithreaded )
{
    test_node::live.store( 0 );
    {
        test_domain                dom;
        std::atomic< test_handle > src;
        test_node*                 stable = make_node( -1 );
        src.store( test_handle( stable, 0 ) );

        const int                  thread_count = 8;
        const int                  iterations   = 2000;
        std::atomic< bool >        failed( false );
        std::vector< std::thread > threads;
        for ( int t = 0; t != thread_count; ++t )
            threads.push_back( std::thread( [ & ] {
                for ( int i = 0; i != iterations; ++i ) {
                    test_domain::guard g = dom.acquire();
                    test_node*         p = dom.protect< 0 >( g, src ).get_ptr();
                    // no boost.test macros here: logging is not thread-safe
                    if ( p != stable || p->value != -1 )
                        failed.store( true, std::memory_order_relaxed );
                    dom.retire( make_node( i ) );
                }
            } ) );
        for ( std::vector< std::thread >::iterator it = threads.begin(); it != threads.end(); ++it )
            it->join();

        BOOST_TEST_REQUIRE( !failed.load() );

        BOOST_TEST_REQUIRE( stable->value == -1 );
        dom.flush_all();
        BOOST_TEST_REQUIRE( test_node::live.load() == 1 );

        counting_allocator< test_node > alloc;
        stable->~test_node();
        alloc.deallocate( stable, 1 );
        BOOST_TEST_REQUIRE( test_node::live.load() == 0 );
    }
}

BOOST_AUTO_TEST_CASE( hazard_pointer_record_recycling )
{
    // short-lived threads force record claim/release churn
    test_node::live.store( 0 );
    {
        test_domain dom;
        for ( int round = 0; round != 20; ++round ) {
            std::thread t( [ & ] {
                test_domain::guard g = dom.acquire();
                for ( int i = 0; i != 10; ++i )
                    dom.retire( make_node( i ) );
            } );
            t.join();
        }
        dom.flush_all();
        BOOST_TEST_REQUIRE( test_node::live.load() == 0 );
    }
}

BOOST_AUTO_TEST_CASE( hazard_pointer_many_domains_one_thread )
{
    // exercises the thread-local cache overflow/sweep path
    test_node::live.store( 0 );
    counting_allocator< test_node >::reset();
    for ( int d = 0; d != 20; ++d ) {
        test_domain        dom;
        test_domain::guard g = dom.acquire();
        for ( int i = 0; i != 10; ++i )
            dom.retire( make_node( i ) );
        dom.flush_all();
    }
    BOOST_TEST_REQUIRE( test_node::live.load() == 0 );
    BOOST_TEST_REQUIRE( counting_allocator< test_node >::allocated.load()
                        == counting_allocator< test_node >::deallocated.load() );
}

BOOST_AUTO_TEST_CASE( hazard_pointer_domain_destroyed_while_thread_alive )
{
    test_node::live.store( 0 );
    std::atomic< bool > go( false );
    std::atomic< bool > done( false );
    std::thread         worker( [ & ] {
        test_domain        other;
        test_domain::guard g = other.acquire();
        while ( !go.load( std::memory_order_acquire ) )
            std::this_thread::yield();
        for ( int i = 0; i != 50; ++i )
            other.retire( make_node( i ) );
        other.flush_all();
        done.store( true, std::memory_order_release );
    } );
    {
        // same TLS cache family, different domain, destroyed first
        test_domain        dom;
        test_domain::guard g = dom.acquire();
        dom.retire( make_node( 1 ) );
        dom.flush_all();
    }
    go.store( true, std::memory_order_release );
    worker.join();
    BOOST_TEST_REQUIRE( done.load( std::memory_order_acquire ) );
    BOOST_TEST_REQUIRE( test_node::live.load() == 0 );
}
