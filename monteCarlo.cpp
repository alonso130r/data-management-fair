#include "monteCarlo.h"
#include <iostream>
#include <future>
#include <vector>
#include <cmath>
#include <utility>
#include <thread>
#include <fstream>
#include <tbb/tbb.h>
#include <tbb/global_control.h>

Simulation::Simulation(const std::map<std::string, int>& initialParams, size_t threads)
    : params(initialParams), threadCount(threads), rd() {
    // initialize parameter bounds
    // fixed game parameters: one-time pay-in and number of rolls
    bounds["payIn"] = {3, 3};
    bounds["maxRolls"] = {8, 8};
    // fix number of dice to 3 for consistent sum ranges
    bounds["numOfDiceP"] = {3, 3};
    // no‐score window radius around the middle of possible sums
    {
        int D = params.at("numOfDiceP");
        int minSum = D;
        int maxSum = D * 6;
        int halfRange = (maxSum - minSum) / 2;  // max radius
        bounds["noWinRangeP"] = {1, halfRange-2};
    }
    // yard thresholds (sum range 1-18)
    // possible sums for 3 dice are 3..18, so thresholds must lie within that range
    bounds["yardsPerStep1P"] = {3, 18};
    bounds["yardsPerStep2P"] = {3, 18};
    bounds["yardsPerStep3P"] = {3, 18};
    bounds["yardsPerStep4P"] = {3, 18};
    // payouts
    bounds["payoutPerStep1P"] = {1, 10};
    bounds["payoutPerStep2P"] = {1, 10};
    bounds["payoutPerStep3P"] = {1, 10};
    bounds["payoutPerStep4P"] = {2, 10};
    bounds["payoutPerStep5P"] = {2, 10};
}

std::map<std::string, int> Simulation::getParams() const {
    return params;
}

void Simulation::setParams(const std::map<std::string, int>& param) {
    params = param;
}

void Simulation::run(size_t numOfRuns) {
    // throttle TBB to user-specified threads
    tbb::global_control ctl(tbb::global_control::max_allowed_parallelism, static_cast<int>(threadCount));
    // target average profit per game: -0.25 tokens
    const double targetProfit = -0.75;
    const double targetWinRate = 0.4;                 // desired win rate
    const double initialLambda = 0.0;                  // ignore win rate to focus on profit
    const double profitWeight = 100.0;                 // heavy penalty on profit deviation (squared)

    // evaluate purely on analytical EV (no Monte Carlo)
    auto evaluate = [&](const std::map<std::string, int>& testParams) {
        double theoEV = computeTheoreticalEV(testParams);
        double winRatePlaceholder = 0.0;  // not used when focusing on theory
        return std::make_pair(theoEV, winRatePlaceholder);
    };

    // use simulated annealing to escape local minima and approach target
    int iteration = 0;
    // more iterations for finer search
    const int maxIterations = 5000;                   // extended search
    const double T0 = 1.0, T_end = 0.1;                // slower cooling (higher final temp)
    double temperature = T0;
    double alpha = std::pow(T_end / T0, 1.0 / maxIterations);
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> unif(0.0, 1.0);
    // evaluate initial empirical metrics and theoretical EV
    auto [bestAvgProfit, bestWinRate] = evaluate(params);
    double bestTheoEV = computeTheoreticalEV(params);
    // weight for theoretical EV closeness term (increased)
    const double theoWeight = 5.0;                      // moderate penalty on theory alignment
    // include theoretical vs empirical alignment penalty using squared profit error
    double currLoss = profitWeight * std::pow(bestAvgProfit - targetProfit, 2) +
                      initialLambda * std::pow(bestWinRate - targetWinRate, 2) +
                      theoWeight * std::abs(bestTheoEV - bestAvgProfit);
    double bestLoss = currLoss;
    auto bestParams = params;
    std::vector<std::string> keys;
    for (auto& kv : params) keys.push_back(kv.first);
    // allow a wide range of neighbor jumps for exploration
    const std::vector<int> deltas = {1, -1, 2, -2, 3, -3, 4, -4, 5, -5};
    // early stopping parameters
    const int earlyStopPatience = 1000;       // iterations without improvement
    int noImprovementCount = 0;
    const double profitTolerance = 1e-3;     // stop if avgProfit within this of target

    while (iteration < maxIterations) {
        bool improvedThisIter = false;       // reset flag for this iteration
        // generate and score candidates purely on theoretical EV
        const size_t batchSize = threadCount;
        struct Cand { std::map<std::string,int> params; double avg, loss; };
        std::vector<Cand> candidates(batchSize);
        for (size_t i = 0; i < batchSize; ++i) {
            auto cp = params;
            const auto& keyi = keys[rng() % keys.size()];
            int di = deltas[rng() % deltas.size()];
            if (bestAvgProfit < targetProfit && keyi.rfind("payoutPerStep",0)==0) di = std::abs(di);
            else if (bestAvgProfit > targetProfit && keyi.rfind("payoutPerStep",0)==0) di = -std::abs(di);
            int oldVal = cp[keyi];
            int val = oldVal + di;
            auto rg = bounds[keyi];
            if (val < rg.first || val > rg.second) val = oldVal;
            cp[keyi] = val;
            // enforce monotonic yard thresholds
            if (keyi.rfind("yardsPerStep", 0) == 0) {
                if (!(cp["yardsPerStep1P"] < cp["yardsPerStep2P"] &&
                      cp["yardsPerStep2P"] < cp["yardsPerStep3P"] &&
                      cp["yardsPerStep3P"] < cp["yardsPerStep4P"])) {
                    cp[keyi] = oldVal;
                }
            }
            // enforce monotonic payout per step
            if (keyi.rfind("payoutPerStep", 0) == 0) {
                if (!(cp["payoutPerStep1P"] < cp["payoutPerStep2P"] &&
                      cp["payoutPerStep2P"] < cp["payoutPerStep3P"] &&
                      cp["payoutPerStep3P"] < cp["payoutPerStep4P"] &&
                      cp["payoutPerStep4P"] < cp["payoutPerStep5P"])) {
                    cp[keyi] = oldVal;
                }
            }
            double theoEV = computeTheoreticalEV(cp);
            candidates[i] = {std::move(cp), theoEV,
                             profitWeight * std::pow(theoEV - targetProfit, 2)};
        }
        // select best candidate based on theoretical EV loss
        auto bestIt = std::min_element(candidates.begin(), candidates.end(),
            [](auto& a, auto& b){ return a.loss < b.loss; });
        // simulated annealing acceptance
        double testLoss = bestIt->loss;
        double probAccept = (testLoss < currLoss)
                          ? 1.0
                          : std::exp((currLoss - testLoss) / temperature);
        if (unif(rng) < probAccept) {
            params = bestIt->params;
            currLoss = testLoss;
            if (testLoss < bestLoss) {
                bestLoss = testLoss;
                bestParams = bestIt->params;
                bestAvgProfit = bestIt->avg;
                improvedThisIter = true;
            }
        }
        // periodic progress logging
        if (++iteration % 2000 == 0) {
            std::cout << "Iter " << iteration
                      << ", currLoss=" << currLoss
                      << ", bestAvgProfit=" << bestAvgProfit
                      << ", bestTheoEV=" << bestTheoEV
                      << std::endl;
        }
        temperature *= alpha;
        // occasional reset to best to avoid drifting
        if (iteration > 0 && iteration % 10000 == 0) {
            params = bestParams;
            currLoss = bestLoss;
        }
        // early stopping check
        if (improvedThisIter) {
            noImprovementCount = 0;  // reset counter
        } else {
            noImprovementCount++;
            if (noImprovementCount >= earlyStopPatience) {
                std::cout << "Early stopping: no improvement for " << earlyStopPatience << " iterations." << std::endl;
                break;
            }
        }
        if (std::abs(bestAvgProfit - targetProfit) < profitTolerance) {
            std::cout << "Early stopping: avgProfit within tolerance of target." << std::endl;
            break;
        }
    }
    params = bestParams;  // adopt optimized parameters

    auto [finalAvgProfit, finalWinRate] = evaluate(params);
    double finalTheoEV = computeTheoreticalEV(params);
    std::cout << "Optimization complete. Final avgProfit=" << finalAvgProfit
              << ", theoreticalEV=" << finalTheoEV << std::endl;
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
            // center no-score window using floating-point midpoint
            int D = params.at("numOfDiceP");
            int minSum = D;
            int maxSum = D * 6;
            double mid = (minSum + maxSum) / 2.0;
            int r = params.at("noWinRangeP");
            int failMin = std::max(minSum, static_cast<int>(std::ceil(mid - r)));
            int failMax = std::min(maxSum, static_cast<int>(std::floor(mid + r)));
            ofs << "noScoreWindow=[" << failMin << "," << failMax << "]\n";
            std::cout << "No-score window: [" << failMin << ", " << failMax << "]" << std::endl;
        }
        ofs.close();
        std::cout << "Parameters written to final_params.txt" << std::endl;
    } else {
        std::cerr << "Error: could not open final_params.txt for writing" << std::endl;
    }
}

// compute theoretical EV given parameters (analytical outcome-tree)
double Simulation::computeTheoreticalEV(const std::map<std::string,int>& p) const {
    int D = p.at("numOfDiceP");
    int minSum = D, maxSum = D * 6;
    double r = p.at("noWinRangeP");
    double mid = (minSum + maxSum) / 2.0;
    int failMin = std::max(minSum, (int)std::ceil(mid - r));
    int failMax = std::min(maxSum, (int)std::floor(mid + r));
    int maxRolls = p.at("maxRolls");
    double payIn = p.at("payIn");
    // compute sum distribution once
    static thread_local std::map<int, std::vector<double>> cache;
    std::vector<double> sumProb;
    auto itC = cache.find(D);
    if (itC == cache.end()) {
        // build raw distribution
        std::vector<double> dist(maxSum + 1);
        for (int i = 1; i <= 6; i++) dist[i] = 1.0;
        for (int dice = 2; dice <= D; dice++) {
            std::vector<double> next(maxSum + 1);
            for (int s = 0; s <= (dice-1)*6; s++) if (dist[s] > 0) for (int f = 1; f <= 6; f++) next[s+f] += dist[s];
            dist.swap(next);
        }
        double total = std::accumulate(dist.begin(), dist.end(), 0.0);
        sumProb.assign(maxSum+1, 0.0);
        for (int s = minSum; s <= maxSum; s++) sumProb[s] = dist[s] / total;
        cache[D] = sumProb;
    } else {
        sumProb = itC->second;
    }
    // build transition matrix T[s][s']
    std::array<std::array<double,6>,6> T{};
    for (int s = 0; s <= 5; s++) for (int sp = 0; sp <= 5; sp++) T[s][sp] = 0.0;
    for (int s = 0; s <= 5; s++) {
        for (int sum = minSum; sum <= maxSum; sum++) {
            double pSum = sumProb[sum]; if (pSum == 0) continue;
            int next = (sum >= failMin && sum <= failMax) ? 0
                     : (sum <= p.at("yardsPerStep1P")) ? 1
                     : (sum <= p.at("yardsPerStep2P")) ? 2
                     : (sum <= p.at("yardsPerStep3P")) ? 3
                     : (sum <= p.at("yardsPerStep4P")) ? 4 : 5;
            if (next != 0 && next < s) next = s;
            T[s][next] += pSum;
        }
    }
    // value iteration
    std::array<double,6> V;
    for (int s = 0; s <= 5; s++) {
        V[s] = (s > 0 && s <=5) ? p.at("payoutPerStep"+std::to_string(s)+"P") : 0.0;
    }
    for (int iter = 0; iter < 100; iter++) {
        std::array<double,6> Vnew = V;
        for (int s = 0; s < 5; s++) {
            double cont = 0.0;
            for (int sp = 0; sp <=5; sp++) cont += T[s][sp] * V[sp];
            double stopVal = V[s];
            Vnew[s] = std::max(stopVal, cont);
        }
        V = Vnew;
    }
    return V[0] - payIn;
}

int main(int argc, char* argv[]) {
    // seed with theoretical-optimal parameters (from final_params3)
    std::map<std::string,int> initialParams = {
        {"numOfDiceP", 3},
        {"noWinRangeP", 2},
        {"payIn", 3},
        {"maxRolls", 5},
        {"yardsPerStep1P", 4},
        {"yardsPerStep2P", 12},
        {"yardsPerStep3P", 14},
        {"yardsPerStep4P", 18},
        {"payoutPerStep1P", 1},
        {"payoutPerStep2P", 4},
        {"payoutPerStep3P", 5},
        {"payoutPerStep4P", 7},
        {"payoutPerStep5P", 10}
    };
    // increased Monte Carlo trials to reduce noise; default 300k, override via CLI
    size_t totalRuns = 50000;
    if (argc > 1) totalRuns = std::stoul(argv[1]);
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