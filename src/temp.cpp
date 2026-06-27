#include <iostream>
#include <vector>
#include <chrono>
#include <cstddef>
#include <cassert>
#include <list>
#include <algorithm>
#include <memory>
#include <new>
#include <utility>
#include "include/CustomDLL.hpp"

// High-resolution timer
uint64_t get_time_nanoseconds() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

int main() {
    std::cout << "Custom Memory Pool and Linked List Performance Test\n";
    std::cout << "==================================================\n\n";

    const int NUM_OPERATIONS = 100000;

    // Test 1: Custom LinkedList with Memory Pool
    {
        std::cout << "Testing Custom LinkedList with Memory Pool:\n";
        CustomLinkedList<std::pair<int,int>> custom_list;

        std::vector<uint64_t> push_times;
        push_times.reserve(NUM_OPERATIONS);

        uint64_t start_time = get_time_nanoseconds();

        for (int i = 0; i < NUM_OPERATIONS; ++i) {
            uint64_t push_start = get_time_nanoseconds();
            custom_list.emplace_back(i, i+1); // emplace two ints into pair
            uint64_t push_end = get_time_nanoseconds();
            push_times.push_back(push_end - push_start);
        }

        uint64_t end_time = get_time_nanoseconds();
        uint64_t total_time = end_time - start_time;

        std::cout << "Total time: " << total_time << " ns\n";
        std::cout << "Average time per emplace_back: " << (double)total_time / NUM_OPERATIONS << " ns\n";
        std::cout << "List size: " << custom_list.size() << "\n";
        std::cout << "Pool capacity: " << custom_list.pool_capacity() << " nodes\n";
        std::cout << "Pool chunks: " << custom_list.pool_chunks() << "\n\n";

        // Calculate percentiles
        std::sort(push_times.begin(), push_times.end());
        size_t n = push_times.size();

        std::cout << "Emplace operation latency percentiles:\n";
        std::cout << "50th (median): " << push_times[n * 50 / 100] << " ns\n";
        std::cout << "95th: " << push_times[n * 95 / 100] << " ns\n";
        std::cout << "99th: " << push_times[n * 99 / 100] << " ns\n";
        std::cout << "99.9th: " << push_times[n * 999 / 1000] << " ns\n\n";
    }

    // Test 2: std::list for comparison
    {
        std::cout << "Testing std::list (for comparison):\n";
        std::list<std::pair<int,int>> std_list;

        std::vector<uint64_t> push_times;
        push_times.reserve(NUM_OPERATIONS);

        uint64_t start_time = get_time_nanoseconds();

        for (int i = 0; i < NUM_OPERATIONS; ++i) {
            uint64_t push_start = get_time_nanoseconds();
            std_list.emplace_back(i, i+1);
            uint64_t push_end = get_time_nanoseconds();
            push_times.push_back(push_end - push_start);
        }

        uint64_t end_time = get_time_nanoseconds();
        uint64_t total_time = end_time - start_time;

        std::cout << "Total time: " << total_time << " ns\n";
        std::cout << "Average time per emplace_back: " << (double)total_time / NUM_OPERATIONS << " ns\n";
        std::cout << "List size: " << std_list.size() << "\n\n";

        // Calculate percentiles
        std::sort(push_times.begin(), push_times.end());
        size_t n = push_times.size();

        std::cout << "Emplace operation latency percentiles:\n";
        std::cout << "50th (median): " << push_times[n * 50 / 100] << " ns\n";
        std::cout << "95th: " << push_times[n * 95 / 100] << " ns\n";
        std::cout << "99th: " << push_times[n * 99 / 100] << " ns\n";
        std::cout << "99.9th: " << push_times[n * 999 / 1000] << " ns\n\n";
    }

    // Test 3: Memory pool stress test
    {
        std::cout << "Memory Pool Stress Test:\n";
        CustomLinkedList<int> stress_list;

        // Push many elements to trigger pool growth
        const int STRESS_OPERATIONS = 100000;

        uint64_t start_time = get_time_nanoseconds();

        for (int i = 0; i < STRESS_OPERATIONS; ++i) {
            stress_list.emplace_back(i);
        }

        uint64_t end_time = get_time_nanoseconds();
        uint64_t total_time = end_time - start_time;

        std::cout << "Pushed " << STRESS_OPERATIONS << " elements\n";
        std::cout << "Total time: " << total_time << " ns\n";
        std::cout << "Average time per emplace_back: " << (double)total_time / STRESS_OPERATIONS << " ns\n";
        std::cout << "Final list size: " << stress_list.size() << "\n";
        std::cout << "Final pool capacity: " << stress_list.pool_capacity() << " nodes\n";
        std::cout << "Final pool chunks: " << stress_list.pool_chunks() << "\n";

        // // pop_front/pop_back test
        // for (int i = 0; i < STRESS_OPERATIONS / 2; ++i) stress_list.pop_front();
        // for (int i = 0; i < STRESS_OPERATIONS / 2; ++i) stress_list.pop_back();
        std::cout << "After pops, list size: " << stress_list.size() << "\n";

        // Test iteration
        uint64_t iter_start = get_time_nanoseconds();
        long long sum = 0;
        for (const auto& val : stress_list) {
            sum += val;
        }
        uint64_t iter_end = get_time_nanoseconds();

        std::cout << "Iteration time: " << (iter_end - iter_start) << " ns\n";
        std::cout << "Sum of all elements: " << sum << "\n";
    }

    return 0;
}