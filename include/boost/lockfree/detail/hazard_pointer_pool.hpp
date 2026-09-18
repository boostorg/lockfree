//  memory pool with hazard-pointer reclamation
//
//  Allocates nodes directly from the allocator and returns retired nodes
//  straight back to it once no hazard pointer protects them. Presents the
//  same interface as the freelist-based pools so queue and stack can use
//  either reclamation scheme without code branches.
//
//  Copyright (C) 2026 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_DETAIL_HAZARD_POINTER_POOL_HPP_INCLUDED
#define BOOST_LOCKFREE_DETAIL_HAZARD_POINTER_POOL_HPP_INCLUDED

#include <cstddef>
#include <memory>

#include <boost/config.hpp>

#include <boost/lockfree/detail/atomic.hpp>
#include <boost/lockfree/detail/hazard_pointer.hpp>
#include <boost/lockfree/detail/untagged_ptr.hpp>

namespace boost { namespace lockfree { namespace detail {

// Pool adapter with hazard-pointer reclamation. Nodes are allocated
// directly from the allocator; destruct<true> retires them through the
// per-container hazard-pointer domain, destruct<false> destroys and
// deallocates immediately (for never-published nodes and destruction).
template < typename T, typename Alloc, std::size_t HazardSlots >
class hazard_pointer_pool
{
    typedef hp_domain< T, Alloc, HazardSlots > domain_type;

public:
    typedef untagged_ptr< T >           tagged_node_handle;
    typedef T*                          handle_type;
    typedef T*                          index_t;
    typedef typename domain_type::guard guard;

    template < typename Allocator >
    hazard_pointer_pool( Allocator const& alloc, std::size_t = 0 ) :
        alloc_( alloc ),
        domain_( alloc_ )
    {}

    // no buffering: reserve is a no-op so that reserve()/bounded_push()
    // stay source-compatible (bounded_push behaves like push)
    template < bool ThreadSafe >
    void reserve( std::size_t )
    {}

    template < bool ThreadSafe, bool Bounded >
    T* construct( void )
    {
        T* node = alloc_.allocate( 1 );
        if ( node )
            new ( node ) T();
        return node;
    }

    template < bool ThreadSafe, bool Bounded, typename ArgumentType >
    T* construct( ArgumentType&& arg )
    {
        T* node = alloc_.allocate( 1 );
        if ( node )
            new ( node ) T( std::forward< ArgumentType >( arg ), typename tagged_node_handle::tag_t() );
        return node;
    }

    template < bool ThreadSafe, bool Bounded, typename ArgumentType1, typename ArgumentType2 >
    T* construct( ArgumentType1&& arg1, ArgumentType2&& arg2 )
    {
        T* node = alloc_.allocate( 1 );
        if ( node )
            new ( node ) T( std::forward< ArgumentType1 >( arg1 ),
                            std::forward< ArgumentType2 >( arg2 ),
                            typename tagged_node_handle::tag_t() );
        return node;
    }

    template < bool ThreadSafe >
    void destruct( tagged_node_handle const& handle )
    {
        T* n = handle.get_ptr();
        if ( n == NULL )
            return;
        n->~T();
        if ( ThreadSafe )
            domain_.retire( n );
        else {
            Alloc alloc( alloc_ );
            alloc.deallocate( n, 1 );
        }
    }

    template < bool ThreadSafe >
    void destruct( T* n )
    {
        destruct< ThreadSafe >( tagged_node_handle( n, 0 ) );
    }

    ~hazard_pointer_pool( void )
    {
        // domain_ destructor reclaims all retired nodes
    }

    bool is_lock_free( void ) const
    {
        // links are plain pointers; allocator calls may block
        return true;
    }

    handle_type get_handle( T* pointer ) const
    {
        return pointer;
    }

    handle_type get_handle( tagged_node_handle const& handle ) const
    {
        return get_pointer( handle );
    }

    T* get_pointer( tagged_node_handle const& handle ) const
    {
        return handle.get_ptr();
    }

    T* get_pointer( T* pointer ) const
    {
        return pointer;
    }

    T* null_handle( void ) const
    {
        return NULL;
    }

    // hazard-pointer protection interface (see freelist.hpp for the no-op
    // counterparts used with freelist reclamation)
    guard acquire()
    {
        return domain_.acquire();
    }

    template < std::size_t Idx, typename AtomicHandle >
    tagged_node_handle protect( guard& g, AtomicHandle const& src )
    {
        return domain_.template protect< Idx >( g, src );
    }

    void flush( void )
    {
        domain_.flush_all();
    }

private:
    Alloc       alloc_;
    domain_type domain_;
};

}}} // namespace boost::lockfree::detail

#endif /* BOOST_LOCKFREE_DETAIL_HAZARD_POINTER_POOL_HPP_INCLUDED */
