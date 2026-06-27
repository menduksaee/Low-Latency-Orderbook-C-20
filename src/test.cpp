#include <list>
#include <chrono>
#include "perf_utils/LatencyStats.hpp"
#include "include/CustomDLL.hpp"
#include <map>
using namespace std;


uint64_t get_time_nanoseconds() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}


int main(){
    std::vector<uint64_t> init_buy_latencies;
    map<float, CustomLinkedList<double>> d;
    shared_ptr<MemoryPool<ListNode<double>>> pool = make_shared<MemoryPool<ListNode<double>>>();
    init_buy_latencies.reserve(10000 * 100);
    for (int level = 0; level < 10000; ++level) {
        double price = 125-(level/100.0); // e.g. 123, 122.99, ...
        if(!d.contains(price)){
            d[price] = CustomLinkedList<double>(pool);
        }
        for (int j = 0; j < 100; ++j) {

            uint64_t start_t = get_time_nanoseconds();
            d[price].push_back(price);
            uint64_t end_t = get_time_nanoseconds();
            init_buy_latencies.push_back(end_t - start_t);
        }
    
        
    }

    for(auto& [price, list] : d){
        cout<<price<<" "<<list.size()<<endl;
    double sum = 0.0;
    for (const auto& val : list) {
        sum += val;
    }
    cout << "Sum: " << sum << endl;
    }
    auto stats = computeLatencyStats(init_buy_latencies);
    appendLatencyStatsToFile(stats);

}