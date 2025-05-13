#include "monteCarlo.h"
#include <iostream>
#include <future>
#include <vector>
#include <cmath>
#include <utility>
#include <thread>
#include <fstream>

Simulation::Simulation(const std::map<std::string, int>& initialParams, size_t threads)
    : params(initialParams), threadCount(threads), rd() {
    // initialize parameter bounds
    bounds["betP"] = {2, 8};
    bounds["numOfDiceP"] = {1, 6};
    bounds["maxTriesP"] = {1, 20};
    // no‐score window radius around the middle of possible sums
    {
        int D = params.at("numOfDiceP");
        int minSum = D;
        int maxSum = D * 6;
        int halfRange = (maxSum - minSum) / 2;  // max radius
        bounds["noWinRangeP"] = {0, halfRange};
    }
    // yard thresholds (sum range 1-18)
    bounds["yardsPerStep1P"] = {1, 18};
    bounds["yardsPerStep2P"] = {1, 18};
    bounds["yardsPerStep3P"] = {1, 18};
    bounds["yardsPerStep4P"] = {1, 18};
    // payouts
    bounds["payoutPerStep1P"] = {1, 20};
    bounds["payoutPerStep2P"] = {1, 20};
    bounds["payoutPerStep3P"] = {1, 20};
    bounds["payoutPerStep4P"] = {2, 20};
    bounds["payoutPerStep5P"] = {2, 20};
}

std::map<std::string, int> Simulation::getParams() const {
    return params;
}

void Simulation::setParams(const std::map<std::string, int>& param) {
    params = param;
}

void Simulation::run(size_t numOfRuns) {
    // target average profit per game: -0.25 tokens
    const double targetProfit = -0.25;
    const double targetWinRate = 0.4;                 // desired win rate
    const double lambda = 1.0;                         // penalty weight for win rate deviation
    bool improved = true;
    int iteration = 0;
    const int maxIterations = 1000;

    auto evaluate = [&](const std::map<std::string, int>& testParams) {
        size_t runsPerThread = numOfRuns / threadCount;
        long long totalProfitSum = 0;
        size_t totalWinCount = 0;
        std::vector<std::future<std::pair<long long, size_t>>> futures;

        for (size_t t = 0; t < threadCount; ++t) {
            futures.emplace_back(std::async(std::launch::async,
                [this, testParams, runsPerThread]() {
                    RazzleGame game(testParams, rd);
                    long long profitSum = 0;
                    size_t winCount = 0;
                    for (size_t i = 0; i < runsPerThread; ++i) {
                        int profit = game.runGame();
                        profitSum += profit;
                        if (profit > 0) ++winCount;
                    }
                    return std::make_pair(profitSum, winCount);
                }
            ));
        }
        for (auto& fut : futures) {
            auto [sum, win] = fut.get();
            totalProfitSum += sum;
            totalWinCount += win;
        }
        double totalRuns = static_cast<double>(runsPerThread * threadCount);
        double avgProfit = static_cast<double>(totalProfitSum) / totalRuns;
        double winRate = static_cast<double>(totalWinCount) / totalRuns;
        return std::make_pair(avgProfit, winRate);
    };

    while (improved && iteration < maxIterations) {
        improved = false;
        auto [currAvgProfit, currWinRate] = evaluate(params);
        // combined loss: profit deviation + lambda * (winRate deviation)^2
        double bestLoss = std::abs(currAvgProfit - targetProfit)
                        + lambda * std::pow(currWinRate - targetWinRate, 2);
        std::cout << "Iteration " << iteration
                  << ", avgProfit=" << currAvgProfit
                  << ", winRate=" << currWinRate
                  << ", loss=" << bestLoss << std::endl;

        for (auto& kv : params) {
            const std::string key = kv.first;
            int orig = kv.second;
            for (int delta : {1, -1}) {
                int newVal = orig + delta;
                // enforce parameter bounds
                auto range = bounds[key];
                if (newVal < range.first || newVal > range.second) continue;
                auto testParams = params;
                testParams[key] = newVal;
                // enforce monotonic yardsPerStep thresholds
                {
                    int y1 = testParams["yardsPerStep1P"], y2 = testParams["yardsPerStep2P"],
                        y3 = testParams["yardsPerStep3P"], y4 = testParams["yardsPerStep4P"];
                    if (!(y1 <= y2 && y2 <= y3 && y3 <= y4)) continue;
                }
                // enforce monotonic payouts
                {
                    int p1 = testParams["payoutPerStep1P"], p2 = testParams["payoutPerStep2P"],
                        p3 = testParams["payoutPerStep3P"], p4 = testParams["payoutPerStep4P"],
                        p5 = testParams["payoutPerStep5P"];
                    if (!(p1 <= p2 && p2 <= p3 && p3 <= p4 && p4 <= p5)) continue;
                }
                auto [testAvgProfit, testWinRate] = evaluate(testParams);
                double newLoss = std::abs(testAvgProfit - targetProfit)
                                + lambda * std::pow(testWinRate - targetWinRate, 2);
                if (newLoss < bestLoss) {
                    params = testParams;
                    bestLoss = newLoss;
                    improved = true;
                    std::cout << " Improved " << key << " to " << newVal
                              << ", avgProfit=" << testAvgProfit
                              << ", winRate=" << testWinRate
                              << ", loss=" << newLoss << std::endl;
                    break;
                }
            }
        }
        iteration++;
    }
    auto [finalAvgProfit, finalWinRate] = evaluate(params);
    std::cout << "Optimization complete. Final avgProfit=" << finalAvgProfit << std::endl;
    std::cout << "Final parameters:" << std::endl;
    for (auto& kv : params) {
        std::cout << kv.first << "=" << kv.second << " ";
    }
    std::cout << std::endl;
    // write final parameters to file for later analysis
    std::ofstream ofs("final_params.txt");
    if (ofs) {
        ofs << "Final avgProfit=" << finalAvgProfit << "\n";
        ofs << "Final winRate=" << finalWinRate << "\n";
        for (const auto& kv : params) {
            ofs << kv.first << "=" << kv.second << "\n";
        }
        // compute and write no-score window range
        {
            int D = params.at("numOfDiceP");
            int minSum = D;
            int maxSum = D * 6;
            int mid = (minSum + maxSum) / 2;
            int r = params.at("noWinRangeP");
            int failMin = std::max(minSum, mid - r);
            int failMax = std::min(maxSum, mid + r);
            ofs << "noScoreWindow=[" << failMin << "," << failMax << "]\n";
            std::cout << "No-score window: [" << failMin << ", " << failMax << "]" << std::endl;
        }
        ofs.close();
        std::cout << "Parameters written to final_params.txt" << std::endl;
    } else {
        std::cerr << "Error: could not open final_params.txt for writing" << std::endl;
    }
}

int main() {
    // initial game parameters -- adjust as needed
    std::map<std::string,int> initialParams = {
        {"numOfDiceP", 3},
        {"maxTriesP", 10},
        {"betP", 2},  // enforce min pay-in of 2 tokens per play
        {"noWinRangeP", 2},  // no-score window radius around midpoint
        {"yardsPerStep1P", 4},
        {"yardsPerStep2P", 9},
        {"yardsPerStep3P", 14},
        {"yardsPerStep4P", 18},
        {"payoutPerStep1P", 1},
        {"payoutPerStep2P", 2},
        {"payoutPerStep3P", 3},
        {"payoutPerStep4P", 4},
        {"payoutPerStep5P", 5}
    };
    size_t totalRuns = 50000;
    size_t threads = std::thread::hardware_concurrency();

    Simulation sim(initialParams, threads);
    sim.run(totalRuns);
    return 0;
}

/*
g++ -std=c++17 game.cpp monteCarlo.cpp \
    -I$(brew --prefix tbb)/include \
    -L$(brew --prefix tbb)/lib -ltbb \
    -pthread -O2 -o rc_opt
*/
// ./rc_opt