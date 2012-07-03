#ifndef BOOST_LOCKFREE_TEST_HELPERS
#define BOOST_LOCKFREE_TEST_HELPERS

#include <set>
#include <boost/array.hpp>
#include <boost/lockfree/detail/atomic.hpp>
#include <boost/thread.hpp>

#include <boost/cstdint.hpp>

template <typename int_type>
int_type generate_id(void)
{
    static boost::lockfree::detail::atomic<int_type> generator(0);
    return ++generator;
}

template <typename int_type, size_t buckets>
class static_hashed_set
{

public:
    // shamelessly copied from
    boost::uint32_t hash(boost::uint32_t a)
    {
        a = (a+0x7ed55d16) + (a<<12);
        a = (a^0xc761c23c) ^ (a>>19);
        a = (a+0x165667b1) + (a<<5);
        a = (a+0xd3a2646c) ^ (a<<9);
        a = (a+0xfd7046c5) + (a<<3);
        a = (a^0xb55a4f09) ^ (a>>16);
        return a;
    }

    boost::uint64_t hash(boost::uint64_t key)
    {
        key = (~key) + (key << 21); // key = (key << 21) - key - 1;
        key = key ^ (key >> 24);
        key = (key + (key << 3)) + (key << 8); // key * 265
        key = key ^ (key >> 14);
        key = (key + (key << 2)) + (key << 4); // key * 21
        key = key ^ (key >> 28);
        key = key + (key << 31);
        return key;
    }

    int calc_index(int_type id)
    {
        return hash((size_t)id) % buckets;
    }

    bool insert(int_type const & id)
    {
        std::size_t index = calc_index(id);

        boost::mutex::scoped_lock lock (ref_mutex[index]);

        std::pair<typename std::set<int_type>::iterator, bool> p;
        p = data[index].insert(id);

        return p.second;
    }

    bool find (int_type const & id)
    {
        std::size_t index = calc_index(id);

        boost::mutex::scoped_lock lock (ref_mutex[index]);

        return data[index].find(id) != data[index].end();
    }

    bool erase(int_type const & id)
    {
        std::size_t index = calc_index(id);

        boost::mutex::scoped_lock lock (ref_mutex[index]);

        if (data[index].find(id) != data[index].end()) {
            data[index].erase(id);
            assert(data[index].find(id) == data[index].end());
            return true;
        }
        else
            return false;
    }

    int count_nodes(void) const
    {
        int ret = 0;
        for (int i = 0; i != buckets; ++i) {
            boost::mutex::scoped_lock lock (ref_mutex[i]);
            ret += data[i].size();
        }
        return ret;
    }

private:
    boost::array<std::set<int_type>, buckets> data;
    mutable boost::array<boost::mutex, buckets> ref_mutex;
};

#endif
