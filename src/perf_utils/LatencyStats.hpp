#pragma once

#include <algorithm>
#include <fstream>
#include <cmath>

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>
#include <string>

struct LatencyStats {
	size_t samples;
	uint64_t p50;
	uint64_t p95;
	uint64_t p96;
	uint64_t p97;
	uint64_t p98;
	uint64_t p99;
	uint64_t p9999;
	double avg;
};



static inline uint64_t percentileNearestRank(const std::vector<uint64_t>& sorted, double pct) {
	size_t n = sorted.size();
	if (n == 0) return 0;
	double rank = std::ceil((pct / 100.0) * static_cast<double>(n));
	size_t idx = (rank >= 1.0) ? static_cast<size_t>(rank) - 1 : 0;
	if (idx >= n) idx = n - 1;
	return sorted[idx];
}

inline LatencyStats computeLatencyStats(const std::vector<uint64_t>& latencies) {
	LatencyStats stats{0, 0, 0, 0, 0, 0, 0, 0, 0.0};
	if (latencies.empty()) return stats;
	std::vector<uint64_t> sorted = latencies;
	std::sort(sorted.begin(), sorted.end());
	stats.samples = sorted.size();
	const unsigned __int128 total = std::accumulate(latencies.begin(), latencies.end(), static_cast<unsigned __int128>(0));
	stats.avg = static_cast<double>(total) / static_cast<double>(stats.samples);
	stats.p50 = percentileNearestRank(sorted, 50.0);
	stats.p95 = percentileNearestRank(sorted, 95.0);
	stats.p96 = percentileNearestRank(sorted, 96.0);
	stats.p97 = percentileNearestRank(sorted, 97.0);
	stats.p98 = percentileNearestRank(sorted, 98.0);
	stats.p99 = percentileNearestRank(sorted, 99.0);
	stats.p9999 = percentileNearestRank(sorted, 99.99);
	return stats;
}

#include <iostream>
inline void appendLatencyStatsToFile(const LatencyStats& stats, const std::string filepath="") {
	if (filepath.empty()) {
		std::cout << "samples: " << stats.samples << "\n";
		std::cout << "average:  " << stats.avg << "\n";
		std::cout << "median:  " << stats.p50 << "\n";
		std::cout << "95th:    " << stats.p95 << "\n";
		std::cout << "96th:    " << stats.p96 << "\n";
		std::cout << "97th:    " << stats.p97 << "\n";
		std::cout << "98th:    " << stats.p98 << "\n";
		std::cout << "99th:    " << stats.p99 << "\n";
		std::cout << "99.99th: " << stats.p9999 << "\n";
		std::cout.flush();
	} else {
		std::ofstream ofs(filepath, std::ios::out | std::ios::app);
		if (!ofs) return;
		ofs << "samples: " << stats.samples << "\n";
		ofs << "average:  " << stats.avg << "\n";
		ofs << "median:  " << stats.p50 << "\n";
		ofs << "95th:    " << stats.p95 << "\n";
		ofs << "96th:    " << stats.p96 << "\n";
		ofs << "97th:    " << stats.p97 << "\n";
		ofs << "98th:    " << stats.p98 << "\n";
		ofs << "99th:    " << stats.p99 << "\n";
		ofs << "99.99th: " << stats.p9999 << "\n";
		ofs.flush();
	}
}






