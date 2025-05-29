
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <cmath>
#include <iomanip>
#include <functional>
#include <numeric>
#include <algorithm>

int main() {
    // load parameters
    std::ifstream ifs("final_params.txt");
    if (!ifs) { std::cerr << "Error: cannot open final_params.txt" << std::endl; return 1; }
    std::map<std::string,double> params;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.rfind("Final",0)==0) continue;
        auto pos = line.find('='); if (pos==std::string::npos) continue;
        auto key = line.substr(0,pos), val = line.substr(pos+1);
        if (key=="noScoreWindow") continue;
        params[key] = std::stod(val);
    }
    int D = int(params["numOfDiceP"]);
    int minSum = D, maxSum = D*6;
    double r = params["noWinRangeP"];
    double mid = (minSum+maxSum)/2.0;
    int failMin = std::max(minSum, int(std::ceil(mid-r)));
    int failMax = std::min(maxSum, int(std::floor(mid+r)));
    int maxRolls = int(params["maxRolls"]);
    double payIn = params["payIn"];

    // build probability distribution for sum of D dice
    std::vector<double> dist(maxSum+1);
    for(int i=1;i<=6;i++) dist[i]=1.0;
    for(int dice=2;dice<=D;dice++){
        std::vector<double> next(maxSum+1);
        for(int s=0;s<=(dice-1)*6;s++) if(dist[s]>0) for(int f=1;f<=6;f++) next[s+f]+=dist[s];
        dist.swap(next);
    }
    double total = std::accumulate(dist.begin(),dist.end(),0.0);
    std::vector<double> sumProb(maxSum+1);
    for(int s=minSum;s<=maxSum;s++) sumProb[s]=dist[s]/total;

    // Helper to map roll->next step and payout
    auto nextStep = [&](int sum, int cur) {
        if(sum>=failMin && sum<=failMax) return 0;
        if(sum<=params["yardsPerStep1P"]) return 1;
        if(sum<=params["yardsPerStep2P"]) return 2;
        if(sum<=params["yardsPerStep3P"]) return 3;
        if(sum<=params["yardsPerStep4P"]) return 4;
        return 5;
    };
    auto payoutOf = [&](int step) {
        if(step<1||step>5) return 0.0;
        return params["payoutPerStep" + std::to_string(step) + "P"];
    };

    // DP table: map (rolls_left, cur_step) -> map<profit, probability>
    using Hist = std::map<int, double>;
    std::map<std::pair<int,int>, Hist> dp;

    // Recursive DP function
    std::function<Hist(int,int)> rec;
    rec = [&](int rolls_left, int cur_step) -> Hist {
        Hist outcome;
        if (rolls_left == 0) {
            int profit = int(payoutOf(cur_step) - payIn);
            outcome[profit] = 1.0;
            return outcome;
        }
        for (int sum = minSum; sum <= maxSum; ++sum) {
            double p = sumProb[sum];
            if (p == 0) continue;
            int next_s = nextStep(sum, cur_step);
            if (next_s == 0) {
                outcome[-int(payIn)] += p;
            } else {
                if (next_s < cur_step) next_s = cur_step;
                if (rolls_left == 1) {
                    int profit = int(payoutOf(next_s) - payIn);
                    outcome[profit] += p;
                } else {
                    Hist sub = rec(rolls_left-1, next_s);
                    for (const auto& kv : sub) {
                        outcome[kv.first] += p * kv.second;
                    }
                }
            }
        }
        return outcome;
    };

    Hist hist = rec(maxRolls, 0);

    // Print the histogram
    std::cout << "Profit distribution (profit: probability):\n";
    for (const auto& kv : hist) {
        std::cout << std::setw(3) << kv.first << ": " << std::setprecision(6) << kv.second << std::endl;
    }
    double EV = 0.0;
    for (const auto& kv : hist) EV += kv.first * kv.second;
    std::cout << "Theoretical EV (per game): " << std::fixed << std::setprecision(6) << EV << std::endl;
    double totalProb = 0.0;
    for (const auto& kv : hist) totalProb += kv.second;
    std::cout << "Sum of probabilities: " << totalProb << std::endl;
    return 0;
}