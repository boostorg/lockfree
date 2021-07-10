//  Copyright (C) 2011 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/lockfree/spsc_queue.hpp>

#define BOOST_TEST_MAIN
#ifdef BOOST_LOCKFREE_INCLUDE_TESTS
#include <boost/test/included/unit_test.hpp>
#else
#include <boost/test/unit_test.hpp>
#endif

#include <iostream>
#include <memory>
#include <utility>

#include "test_helpers.hpp"
#include "test_common.hpp"

using namespace boost;
using namespace boost::lockfree;
using namespace std;

BOOST_AUTO_TEST_CASE( simple_spsc_queue_test )
{
    spsc_queue<int, capacity<64> > f;

    BOOST_REQUIRE(f.empty());
    f.push(1);
    f.push(2);

    int i1(0), i2(0);

    BOOST_REQUIRE(f.pop(i1));
    BOOST_REQUIRE_EQUAL(i1, 1);

    BOOST_REQUIRE(f.pop(i2));
    BOOST_REQUIRE_EQUAL(i2, 2);
    BOOST_REQUIRE(f.empty());
}

BOOST_AUTO_TEST_CASE( simple_spsc_queue_test_compile_time_size )
{
    spsc_queue<int> f(64);

    BOOST_REQUIRE(f.empty());
    f.push(1);
    f.push(2);

    int i1(0), i2(0);

    BOOST_REQUIRE(f.pop(i1));
    BOOST_REQUIRE_EQUAL(i1, 1);

    BOOST_REQUIRE(f.pop(i2));
    BOOST_REQUIRE_EQUAL(i2, 2);
    BOOST_REQUIRE(f.empty());
}

BOOST_AUTO_TEST_CASE( ranged_push_test )
{
    spsc_queue<int> stk(64);

    int data[2] = {1, 2};

    BOOST_REQUIRE_EQUAL(stk.push(data, data + 2), data + 2);

    int out;
    BOOST_REQUIRE(stk.pop(out)); BOOST_REQUIRE_EQUAL(out, 1);
    BOOST_REQUIRE(stk.pop(out)); BOOST_REQUIRE_EQUAL(out, 2);
    BOOST_REQUIRE(!stk.pop(out));
}

BOOST_AUTO_TEST_CASE( spsc_queue_consume_one_test )
{
    spsc_queue<int> f(64);

    BOOST_WARN(f.is_lock_free());
    BOOST_REQUIRE(f.empty());

    f.push(1);
    f.push(2);

#ifdef BOOST_NO_CXX11_LAMBDAS
    bool success1 = f.consume_one(test_equal(1));
    bool success2 = f.consume_one(test_equal(2));
#else
    bool success1 = f.consume_one([] (int i) {
        BOOST_REQUIRE_EQUAL(i, 1);
    });

    bool success2 = f.consume_one([] (int i) {
        BOOST_REQUIRE_EQUAL(i, 2);
    });
#endif

    BOOST_REQUIRE(success1);
    BOOST_REQUIRE(success2);

    BOOST_REQUIRE(f.empty());
}

BOOST_AUTO_TEST_CASE( spsc_queue_consume_all_test )
{
    spsc_queue<int> f(64);

    BOOST_WARN(f.is_lock_free());
    BOOST_REQUIRE(f.empty());

    f.push(1);
    f.push(2);

#ifdef BOOST_NO_CXX11_LAMBDAS
    size_t consumed = f.consume_all(dummy_functor());
#else
    size_t consumed = f.consume_all([] (int i) {
    });
#endif

    BOOST_REQUIRE_EQUAL(consumed, 2u);

    BOOST_REQUIRE(f.empty());
}

enum {
    pointer_and_size,
    reference_to_array,
    iterator_pair,
    output_iterator_
};

BOOST_AUTO_TEST_CASE( spsc_queue_capacity_test )
{
    spsc_queue<int, capacity<2> > f;

    BOOST_REQUIRE(f.push(1));
    BOOST_REQUIRE(f.push(2));
    BOOST_REQUIRE(!f.push(3));

    spsc_queue<int> g(2);

    BOOST_REQUIRE(g.push(1));
    BOOST_REQUIRE(g.push(2));
    BOOST_REQUIRE(!g.push(3));
}

template <typename QueueType>
void spsc_queue_avail_test_run(QueueType & q)
{
    BOOST_REQUIRE_EQUAL( q.write_available(), 16 );
    BOOST_REQUIRE_EQUAL( q.read_available(),   0 );

    for (size_t i = 0; i != 8; ++i) {
        BOOST_REQUIRE_EQUAL( q.write_available(), 16 - i );
        BOOST_REQUIRE_EQUAL( q.read_available(),       i );

        q.push( 1 );
    }

    // empty queue
    int dummy;
    while (q.pop(dummy))
    {}

    for (size_t i = 0; i != 16; ++i) {
        BOOST_REQUIRE_EQUAL( q.write_available(), 16 - i );
        BOOST_REQUIRE_EQUAL( q.read_available(),       i );

        q.push( 1 );
    }
}

BOOST_AUTO_TEST_CASE( spsc_queue_avail_test )
{
    spsc_queue<int, capacity<16> > f;
    spsc_queue_avail_test_run(f);

    spsc_queue<int> g(16);
    spsc_queue_avail_test_run(g);
}


template <int EnqueueMode>
void spsc_queue_buffer_push_return_value(void)
{
    const size_t xqueue_size = 64;
    const size_t buffer_size = 100;
    spsc_queue<int, capacity<100> > rb;

    int data[xqueue_size];
    for (size_t i = 0; i != xqueue_size; ++i)
        data[i] = (int)i*2;

    switch (EnqueueMode) {
    case pointer_and_size:
        BOOST_REQUIRE_EQUAL(rb.push(data, xqueue_size), xqueue_size);
        break;

    case reference_to_array:
        BOOST_REQUIRE_EQUAL(rb.push(data), xqueue_size);
        break;

    case iterator_pair:
        BOOST_REQUIRE_EQUAL(rb.push(data, data + xqueue_size), data + xqueue_size);
        break;

    default:
        assert(false);
    }

    switch (EnqueueMode) {
    case pointer_and_size:
        BOOST_REQUIRE_EQUAL(rb.push(data, xqueue_size), buffer_size - xqueue_size);
        break;

    case reference_to_array:
        BOOST_REQUIRE_EQUAL(rb.push(data), buffer_size - xqueue_size);
        break;

    case iterator_pair:
        BOOST_REQUIRE_EQUAL(rb.push(data, data + xqueue_size), data + buffer_size - xqueue_size);
        break;

    default:
        assert(false);
    }
}

BOOST_AUTO_TEST_CASE( spsc_queue_buffer_push_return_value_test )
{
    spsc_queue_buffer_push_return_value<pointer_and_size>();
    spsc_queue_buffer_push_return_value<reference_to_array>();
    spsc_queue_buffer_push_return_value<iterator_pair>();
}

template <int EnqueueMode,
          int ElementCount,
          int BufferSize,
          int NumberOfIterations
         >
void spsc_queue_buffer_push(void)
{
    const size_t xqueue_size = ElementCount;
    spsc_queue<int, capacity<BufferSize> > rb;

    int data[xqueue_size];
    for (size_t i = 0; i != xqueue_size; ++i)
        data[i] = (int)i*2;

    std::vector<int> vdata(data, data + xqueue_size);

    for (int i = 0; i != NumberOfIterations; ++i) {
        BOOST_REQUIRE(rb.empty());
        switch (EnqueueMode) {
        case pointer_and_size:
            BOOST_REQUIRE_EQUAL(rb.push(data, xqueue_size), xqueue_size);
            break;

        case reference_to_array:
            BOOST_REQUIRE_EQUAL(rb.push(data), xqueue_size);
            break;

        case iterator_pair:
            BOOST_REQUIRE_EQUAL(rb.push(data, data + xqueue_size), data + xqueue_size);
            break;

        default:
            assert(false);
        }

        int out[xqueue_size];
        BOOST_REQUIRE_EQUAL(rb.pop(out, xqueue_size), xqueue_size);
        for (size_t i = 0; i != xqueue_size; ++i)
            BOOST_REQUIRE_EQUAL(data[i], out[i]);
    }
}

BOOST_AUTO_TEST_CASE( spsc_queue_buffer_push_test )
{
    spsc_queue_buffer_push<pointer_and_size, 7, 16, 64>();
    spsc_queue_buffer_push<reference_to_array, 7, 16, 64>();
    spsc_queue_buffer_push<iterator_pair, 7, 16, 64>();
}

template <int EnqueueMode,
          int ElementCount,
          int BufferSize,
          int NumberOfIterations
         >
void spsc_queue_buffer_pop(void)
{
    const size_t xqueue_size = ElementCount;
    spsc_queue<int, capacity<BufferSize> > rb;

    int data[xqueue_size];
    for (size_t i = 0; i != xqueue_size; ++i)
        data[i] = (int)i*2;

    std::vector<int> vdata(data, data + xqueue_size);

    for (int i = 0; i != NumberOfIterations; ++i) {
        BOOST_REQUIRE(rb.empty());
        BOOST_REQUIRE_EQUAL(rb.push(data), xqueue_size);

        int out[xqueue_size];
        vector<int> vout;

        switch (EnqueueMode) {
        case pointer_and_size:
            BOOST_REQUIRE_EQUAL(rb.pop(out, xqueue_size), xqueue_size);
            break;

        case reference_to_array:
            BOOST_REQUIRE_EQUAL(rb.pop(out), xqueue_size);
            break;

        case output_iterator_:
            BOOST_REQUIRE_EQUAL(rb.pop(std::back_inserter(vout)), xqueue_size);
            break;

        default:
            assert(false);
        }

        if (EnqueueMode == output_iterator_) {
            BOOST_REQUIRE_EQUAL(vout.size(), xqueue_size);
            for (size_t i = 0; i != xqueue_size; ++i)
                BOOST_REQUIRE_EQUAL(data[i], vout[i]);
        } else {
            for (size_t i = 0; i != xqueue_size; ++i)
                BOOST_REQUIRE_EQUAL(data[i], out[i]);
        }
    }
}

BOOST_AUTO_TEST_CASE( spsc_queue_buffer_pop_test )
{
    spsc_queue_buffer_pop<pointer_and_size, 7, 16, 64>();
    spsc_queue_buffer_pop<reference_to_array, 7, 16, 64>();
    spsc_queue_buffer_pop<output_iterator_, 7, 16, 64>();
}

// Test front() and pop()
template < typename Queue >
void spsc_queue_front_pop(Queue& queue)
{
    queue.push(1);
    queue.push(2);
    queue.push(3);

    // front as ref and const ref
    int& rfront = queue.front();
    const int& crfront = queue.front();

    BOOST_REQUIRE_EQUAL(1, rfront);
    BOOST_REQUIRE_EQUAL(1, crfront);

    int front = 0;

    // access element pushed first
    front = queue.front();
    BOOST_REQUIRE_EQUAL(1, front);

    // front is still the same
    front = queue.front();
    BOOST_REQUIRE_EQUAL(1, front);

    queue.pop();

    front = queue.front();
    BOOST_REQUIRE_EQUAL(2, front);

    queue.pop(); // pop 2

    bool pop_ret = queue.pop(); // pop 3
    BOOST_REQUIRE(pop_ret);

    pop_ret = queue.pop(); // pop on empty queue
    BOOST_REQUIRE( ! pop_ret);
}

BOOST_AUTO_TEST_CASE( spsc_queue_buffer_front_and_pop_runtime_sized_test )
{
    spsc_queue<int, capacity<64> > queue;
    spsc_queue_front_pop(queue);
}

BOOST_AUTO_TEST_CASE( spsc_queue_buffer_front_and_pop_compiletime_sized_test )
{
    spsc_queue<int> queue(64);
    spsc_queue_front_pop(queue);
}

BOOST_AUTO_TEST_CASE( spsc_queue_reset_test )
{
    spsc_queue<int, capacity<64> > f;

    BOOST_REQUIRE(f.empty());
    f.push(1);
    f.push(2);

    f.reset();

    BOOST_REQUIRE(f.empty());
}

struct immovable_type
{
    int value;

    immovable_type() : value(0) {}
    immovable_type(int x) : value(x) {}
    immovable_type(const immovable_type& other) : value(other.value) {}

    immovable_type& operator=(const immovable_type& other)
    {
        this->value = other.value;
        return *this;
    }

#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
#ifndef BOOST_NO_CX11_RVALUE_REFERENCES
    immovable_type(immovable_type&&) = delete;
    immovable_type& operator=(immovable_type&&) = delete;
#endif
#endif
};

struct test_immovable_object
{
    test_immovable_object(int i):
        i(i)
    {}

    void operator()(immovable_type arg) const
    {
        BOOST_REQUIRE_EQUAL(arg.value, i);
    }

    int i;
};

BOOST_AUTO_TEST_CASE( spsc_queue_immovable_object_test )
{
    spsc_queue<immovable_type, capacity<64> > f;

    f.emplace( 9001 );
    f.emplace( 9002 );
    f.push( immovable_type(9003) );

    immovable_type x(9004);
    immovable_type push_array[2];
    push_array[0] = x;
    push_array[1] = x;
    f.push(push_array, 2);

    immovable_type pop_array[2];
    f.pop(pop_array, 2);
    BOOST_CHECK_EQUAL( pop_array[0].value, 9001 );
    BOOST_CHECK_EQUAL( pop_array[1].value, 9002 );

    f.consume_one( test_immovable_object(9003) );
    f.consume_all( test_immovable_object(9004) );

    f.emplace( 42 );
    f.emplace( 42 );
    f.reset();
}

#ifndef BOOST_NO_CX11_RVALUE_REFERENCES
struct refcount_handle
{
    int* p_refcount;

    refcount_handle() : p_refcount( nullptr ) {}
    refcount_handle(int* x) : p_refcount(x)
    {
        if (p_refcount != nullptr)
        {
            *p_refcount += 1;
        }
    }
    ~refcount_handle()
    {
        if (p_refcount != nullptr)
        {
            *p_refcount -= 1;
        }
    }

    refcount_handle(const refcount_handle& other) = delete;
    refcount_handle& operator=(const refcount_handle&) = delete;

    refcount_handle& operator=(refcount_handle&& other)
    {
        if (this->p_refcount != nullptr)
        {
            *p_refcount -= 1;
        }

        this->p_refcount = other.p_refcount;
        other.p_refcount = nullptr;
        return *this;
    }
    refcount_handle(refcount_handle&& other)
        : p_refcount( other.p_refcount )
    {
        other.p_refcount = nullptr;
    }
};

struct test_refcount_handle
{
    test_refcount_handle(int* p_refcount):
        p_refcount(p_refcount)
    {}

    void operator()(refcount_handle& arg) const
    {
        BOOST_REQUIRE_EQUAL(arg.p_refcount, p_refcount);
    }

    int* p_refcount;
};

BOOST_AUTO_TEST_CASE( spsc_queue_refcount_test )
{
    spsc_queue<refcount_handle, capacity<64> > f;

    int refcount = 0;

    f.emplace( &refcount );
    f.emplace( &refcount );
    f.emplace( &refcount );
    f.emplace( &refcount );
    BOOST_CHECK_EQUAL( refcount, 4 );

    f.push( std::move( refcount_handle(&refcount) ) );

    BOOST_CHECK_EQUAL( refcount, 5 );

    {
        refcount_handle pop_array[2];
        f.pop(pop_array, 2);
        BOOST_CHECK_EQUAL( pop_array[0].p_refcount, &refcount );
        BOOST_CHECK_EQUAL( pop_array[1].p_refcount, &refcount );

        BOOST_CHECK_EQUAL( refcount, 5 );
    }
    BOOST_CHECK_EQUAL( refcount, 3 );

    f.consume_one( test_refcount_handle(&refcount) );
    BOOST_CHECK_EQUAL( refcount, 2 );
    f.consume_all( test_refcount_handle(&refcount) );
    BOOST_CHECK_EQUAL( refcount, 0 );

    f.emplace( &refcount );
    f.emplace( &refcount );
    BOOST_CHECK_EQUAL( refcount, 2 );
    f.reset();
    BOOST_CHECK_EQUAL( refcount, 0 );
}
#endif
