//  hazard pointer domain for lock-free safe memory reclamation
//  (M. Michael, "Hazard Pointers: Safe Memory Reclamation for Lock-Free Objects")
//
//  Copyright (C) 2026 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_DETAIL_HAZARD_POINTER_HPP_INCLUDED
#define BOOST_LOCKFREE_DETAIL_HAZARD_POINTER_HPP_INCLUDED

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

#include <boost/config.hpp>
#include <boost/static_assert.hpp>

namespace boost { namespace lockfree { namespace detail {

// Hazard-pointer domain with K hazard slots per participating thread.
//
// Each container instance owns one hp_domain. Threads participating in
// operations on the domain claim one record per domain; the mapping from
// (thread, domain) to record is cached in thread-local storage. Records
// are never unlinked, so a published hazard pointer can be dereferenced
// safely at any time.
//
// Retired nodes that are not protected by any hazard slot are destroyed
// and deallocated straight via the allocator.
//
// Requirements on T: complete type with accessible destructor.
// Alloc: rebound allocator for T, supporting allocate(1)/deallocate(p, 1).

template < typename T, typename Alloc, std::size_t K >
class hp_domain
{
    BOOST_STATIC_ASSERT( K > 0 );

public:
    static constexpr std::size_t hazard_slots = K;

private:
    struct record
    {
        record() :
            active( true ),
            next( NULL ),
            scan_threshold( 32 )
        {
            for ( std::size_t i = 0; i != K; ++i )
                hazard[ i ].store( NULL, std::memory_order_relaxed );
        }

        std::atomic< void* > hazard[ K ];
        std::vector< void* > retired; // owned by the claiming thread, except under control_block::mutex
        bool                 active;  // under control_block::mutex
        record*              next;    // under control_block::mutex
        std::size_t          scan_threshold;
    };

    struct control_block
    {
        explicit control_block( Alloc const& alloc ) :
            head( NULL ),
            record_count( 0 ),
            alive( true ),
            alloc( alloc )
        {}

        // records are never freed before the control block itself
        ~control_block()
        {
            for ( record* r = head; r != NULL; ) {
                record* n = r->next;
                delete r;
                r = n;
            }
        }

        std::mutex           mutex;
        record*              head;         // under mutex
        std::size_t          record_count; // under mutex
        std::vector< void* > orphans;      // retired nodes of exited threads, under mutex
        std::atomic< bool >  alive;
        Alloc                alloc;
    };

    // Thread-local cache mapping domains to records. One instance per
    // (T, Alloc, K) instantiation, i.e. shared by all domains of the same
    // node type. Entries keep the control block alive so record pointers
    // stay valid; entries for dead domains are swept opportunistically.
    struct thread_cache
    {
        struct entry
        {
            entry() :
                raw( NULL ),
                rec( NULL )
            {}

            std::shared_ptr< control_block > keepalive;
            control_block*                   raw;
            record*                          rec;
        };

        ~thread_cache()
        {
            clear_entry( fast );
            for ( typename std::vector< entry >::iterator it = slots.begin(); it != slots.end(); ++it )
                clear_entry( *it );
        }

        static void clear_entry( entry& e )
        {
            if ( e.rec == NULL )
                return;
            for ( std::size_t i = 0; i != K; ++i )
                e.rec->hazard[ i ].store( NULL, std::memory_order_relaxed );
            {
                std::lock_guard< std::mutex > lock( e.raw->mutex );
                e.raw->orphans.insert( e.raw->orphans.end(), e.rec->retired.begin(), e.rec->retired.end() );
                e.rec->retired.clear();
                e.rec->retired.shrink_to_fit();
                e.rec->active = false;
            }
            e.keepalive.reset();
            e.raw = NULL;
            e.rec = NULL;
        }

        entry                fast;
        std::vector< entry > slots;
    };

    static thread_cache& get_thread_cache()
    {
        static thread_local thread_cache cache;
        return cache;
    }

    static const std::size_t max_cached_domains = 8;

    // must be called with ctl->mutex held
    record* find_inactive_record_locked()
    {
        for ( record* r = ctl_->head; r != NULL; r = r->next ) {
            if ( !r->active ) {
                r->active = true;
                return r;
            }
        }
        return NULL;
    }

    void sweep_dead_entries( thread_cache& tc )
    {
        for ( std::size_t i = 0; i != tc.slots.size(); ) {
            typename thread_cache::entry& e    = tc.slots[ i ];
            bool                          dead = ( e.raw != ctl_.get() )
                        && ( e.keepalive.get() == NULL || !e.keepalive->alive.load( std::memory_order_acquire ) );
            if ( dead ) {
                thread_cache::clear_entry( e );
                e = tc.slots.back();
                tc.slots.pop_back();
            } else
                ++i;
        }
        if ( tc.fast.raw != NULL && tc.fast.raw != ctl_.get()
             && !tc.fast.keepalive->alive.load( std::memory_order_acquire ) ) {
            thread_cache::clear_entry( tc.fast );
        }
    }

    // returns the calling thread's record for this domain, claiming or
    // creating one if necessary
    record* get_record()
    {
        thread_cache& tc = get_thread_cache();

        if ( tc.fast.raw == ctl_.get() )
            return tc.fast.rec;

        for ( typename std::vector< typename thread_cache::entry >::iterator it = tc.slots.begin(); it != tc.slots.end();
              ++it ) {
            if ( it->raw == ctl_.get() ) {
                // promote hit to fast path, demote previous fast entry
                typename thread_cache::entry hit = *it;
                if ( tc.fast.raw != NULL )
                    *it = tc.fast;
                else {
                    *it = tc.slots.back();
                    tc.slots.pop_back();
                }
                tc.fast = hit;
                return tc.fast.rec;
            }
        }

        // slow path: claim or create a record
        record* rec = NULL;
        {
            std::lock_guard< std::mutex > lock( ctl_->mutex );
            rec = find_inactive_record_locked();
            if ( rec == NULL ) {
                rec        = new record();
                rec->next  = ctl_->head;
                ctl_->head = rec;
                ++ctl_->record_count;
            }
            rec->scan_threshold = 2 * K * ctl_->record_count + 32;
        }

        // make room in the cache, sweeping dead domains first
        if ( tc.fast.raw != NULL ) {
            if ( tc.slots.size() >= max_cached_domains )
                sweep_dead_entries( tc );
            if ( tc.slots.size() >= max_cached_domains + 4 ) {
                // still full of live domains: drop the oldest slot entry.
                // its retired nodes move to the orphan list so nothing leaks.
                thread_cache::clear_entry( tc.slots.front() );
                tc.slots.erase( tc.slots.begin() );
            }
            tc.slots.push_back( tc.fast );
        }
        tc.fast.keepalive = ctl_;
        tc.fast.raw       = ctl_.get();
        tc.fast.rec       = rec;
        return rec;
    }

    void destroy_nodes( std::vector< void* >& nodes )
    {
        for ( std::vector< void* >::iterator it = nodes.begin(); it != nodes.end(); ++it ) {
            T* p = static_cast< T* >( *it );
            p->~T();
            ctl_->alloc.deallocate( p, 1 );
        }
    }

    // scan the calling thread's retired list plus orphans against all
    // published hazard pointers; destroy and deallocate the unprotected ones
    void scan( record* rec )
    {
        std::vector< void* > hazards;
        std::vector< void* > pending;
        {
            std::lock_guard< std::mutex > lock( ctl_->mutex );
            for ( record* r = ctl_->head; r != NULL; r = r->next ) {
                if ( !r->active )
                    continue;
                for ( std::size_t i = 0; i != K; ++i ) {
                    void* h = r->hazard[ i ].load( std::memory_order_acquire );
                    if ( h != NULL )
                        hazards.push_back( h );
                }
            }
            pending.swap( rec->retired );
            pending.insert( pending.end(), ctl_->orphans.begin(), ctl_->orphans.end() );
            ctl_->orphans.clear();
            rec->scan_threshold = 2 * K * ctl_->record_count + 32;
        }

        if ( pending.empty() )
            return;

        std::sort( hazards.begin(), hazards.end() );
        std::vector< void* > survivors;
        survivors.reserve( pending.size() );
        std::vector< void* > garbage;
        garbage.reserve( pending.size() );
        for ( std::vector< void* >::iterator it = pending.begin(); it != pending.end(); ++it ) {
            if ( std::binary_search( hazards.begin(), hazards.end(), *it ) )
                survivors.push_back( *it );
            else
                garbage.push_back( *it );
        }
        if ( !survivors.empty() )
            rec->retired.insert( rec->retired.end(), survivors.begin(), survivors.end() );
        destroy_nodes( garbage );
    }

public:
    explicit hp_domain( Alloc const& alloc = Alloc() ) :
        ctl_( new control_block( alloc ) )
    {}

    hp_domain( hp_domain const& )            = delete;
    hp_domain& operator=( hp_domain const& ) = delete;
    hp_domain( hp_domain&& )                 = delete;
    hp_domain& operator=( hp_domain&& )      = delete;

    ~hp_domain()
    {
        flush_all();
        ctl_->alive.store( false, std::memory_order_release );
    }

    // RAII holder for a thread's record; clears all hazard slots on destruction
    class guard
    {
        friend class hp_domain;

        explicit guard( record* r ) :
            rec_( r )
        {}

    public:
        guard() :
            rec_( NULL )
        {}

        guard( guard&& other ) :
            rec_( other.rec_ )
        {
            other.rec_ = NULL;
        }

        guard& operator=( guard&& other )
        {
            if ( this != &other ) {
                release();
                rec_       = other.rec_;
                other.rec_ = NULL;
            }
            return *this;
        }

        guard( guard const& )            = delete;
        guard& operator=( guard const& ) = delete;

        ~guard()
        {
            release();
        }

        void release()
        {
            if ( rec_ != NULL ) {
                for ( std::size_t i = 0; i != K; ++i )
                    rec_->hazard[ i ].store( NULL, std::memory_order_release );
                rec_ = NULL;
            }
        }

        bool owns_record() const
        {
            return rec_ != NULL;
        }

    private:
        record* rec_;
    };

    guard acquire()
    {
        return guard( get_record() );
    }

    // Publish hazard slot Idx for the atomic pointer src and return the
    // protected handle. The returned handle is safe to dereference until
    // the guard is released or destroyed; the corresponding raw pointer can
    // be obtained via get_ptr().
    template < std::size_t Idx, typename AtomicHandle >
    auto protect( guard& g, AtomicHandle const& src ) -> decltype( src.load() )
    {
        BOOST_STATIC_ASSERT( Idx < K );
        for ( ;; ) {
            auto published = src.load( std::memory_order_seq_cst );
            g.rec_->hazard[ Idx ].store( published.get_ptr(), std::memory_order_seq_cst );
            auto validated = src.load( std::memory_order_seq_cst );
            if ( published == validated )
                return validated;
        }
    }

    void retire( T* p )
    {
        record* rec = get_record();
        rec->retired.push_back( p );
        if ( rec->retired.size() >= rec->scan_threshold )
            scan( rec );
    }

    // Reclaim everything reclaimable. May only be called when no thread
    // concurrently operates on this domain (e.g. from the owning
    // container's destructor).
    void flush_all()
    {
        std::vector< void* > pending;
        {
            std::lock_guard< std::mutex > lock( ctl_->mutex );
            for ( record* r = ctl_->head; r != NULL; r = r->next ) {
                pending.insert( pending.end(), r->retired.begin(), r->retired.end() );
                r->retired.clear();
            }
            pending.insert( pending.end(), ctl_->orphans.begin(), ctl_->orphans.end() );
            ctl_->orphans.clear();
        }
        destroy_nodes( pending );
    }

private:
    std::shared_ptr< control_block > ctl_;
};

}}} // namespace boost::lockfree::detail

#endif /* BOOST_LOCKFREE_DETAIL_HAZARD_POINTER_HPP_INCLUDED */
