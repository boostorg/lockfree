//  Copyright (C) 2009 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#if __cplusplus < 201103L
int main(){}
#else

//[spsc_queue_example
#include <boost/thread/thread.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <iostream>

#include <atomic> // example requires c++11

int producer_count = 0;
std::atomic_int consumer_count (0);

boost::lockfree::spsc_queue<int, boost::lockfree::capacity<1024> > spsc_queue;

const int iterations = 10000000;

void producer(void)
{
    for (int i = 0; i != iterations; ++i) {
        int value = ++producer_count;
        while (!spsc_queue.push(value))
            ;
    }
}

std::atomic<bool> done (false);

void consumer(void)
{
    int value;
    while (!done) {
        while (spsc_queue.pop(value))
            ++consumer_count;
    }

    while (spsc_queue.pop(value))
        ++consumer_count;
}

int main(int argc, char* argv[])
{
    using namespace std;
    cout << "boost::lockfree::queue is ";
    if (!spsc_queue.is_lock_free())
        cout << "not ";
    cout << "lockfree" << endl;

    boost::thread producer_thread(producer);
    boost::thread consumer_thread(consumer);


    producer_thread.join();
    done = true;
    consumer_thread.join();

    cout << "produced " << producer_count << " objects." << endl;
    cout << "consumed " << consumer_count << " objects." << endl;
}
//]
#endif
