#include "game.h"
#include <algorithm>  // for std::max/std::min

bool RazzleGame::mapRollToYard(int S, int current) const {
    // compute no-score window around midpoint
    int D = params.at("numOfDiceP");
    int minSum = D;
    int maxSum = D * 6;
    int mid = (minSum + maxSum) / 2;
    int r = 0;
    auto itR = params.find("noWinRangeP");
    if (itR != params.end()) r = itR->second;
    int failMin = std::max(minSum, mid - r);
    int failMax = std::min(maxSum, mid + r);
    // check failure band
    if (S >= failMin && S <= failMax) {
        return false;
    }
    // calculate the yard value based on the roll
    int yardValue;
    if (S <= params.at("yardsPerStep1P")) {
        yardValue = 1;
    } else if (S <= params.at("yardsPerStep2P")) {
        yardValue = 2;
    } else if (S <= params.at("yardsPerStep3P")) {
        yardValue = 3;
    } else if (S <= params.at("yardsPerStep4P")) {
        yardValue = 4;
    } else {
        yardValue = 5;
    }
    // return true if the yard value is greater than the current step
    return yardValue > current;
}

int RazzleGame::mapStepToPayout(int current) const {
    if (current == 1) return params.at("payoutPerStep1P");
    else if (current == 2) return params.at("payoutPerStep2P");
    else if (current == 3) return params.at("payoutPerStep3P");
    else if (current == 4) return params.at("payoutPerStep4P");
    else if (current == 5) return params.at("payoutPerStep5P");
    return 0;
}

// fill sumProb
void RazzleGame::computeSumDistribution() {
    int D = params.at("numOfDiceP");
    int F = 6;
    int sMin = D, sMax = D * F;

    // dist[k] = # ways to get sum=k
    std::vector<double> dist(sMax+1);

    // for 1 die
    for(int i = 1; i <= F; i++) dist[i] = 1;

    // convolve for additional dice
    for(int j = 2; j <= D; j++) {
        std::vector<double> next(sMax+1);
        for(int s = 0; s <= (j - 1) * F; s++) if(dist[s] > 0) {
          for(int f = 1; f <= F; f++) next[s+f] += dist[s];
        }
        dist.swap(next);
    }

    // normalize
    double total = std::accumulate(dist.begin(), dist.end(), 0.0);
    for(int s = sMin; s <= sMax; s++) {
        sumProb[s] = dist[s] / total;
    }
}

// used to build T[s][s']
void RazzleGame::buildTransitionMatrix() {
    // zero out
    for(auto &row: T) row.fill(0.0);

    // for each starting yard s=0-5
    for (int s = 0; s <= 5; s++) {
        // for each possible roll sum
        for (auto& pPair : sumProb) {
            int roll = pPair.first;
            double p = pPair.second;
            
            // compute dynamic failure band around midpoint
            int D = params.at("numOfDiceP");
            int minSum = D;
            int maxSum = D * 6;
            int mid = (minSum + maxSum) / 2;
            int r = 0;
            auto it = params.find("noWinRangeP");
            if (it != params.end()) r = it->second;
            int failMin = std::max(minSum, mid - r);
            int failMax = std::min(maxSum, mid + r);
            
            // map roll to next yard
            int nextYard;
            if (roll >= failMin && roll <= failMax) {
                nextYard = 0;
            } else if (roll <= params.at("yardsPerStep1P")) {
                nextYard = 1;
            } else if (roll <= params.at("yardsPerStep2P")) {
                nextYard = 2;
            } else if (roll <= params.at("yardsPerStep3P")) {
                nextYard = 3;
            } else if (roll <= params.at("yardsPerStep4P")) {
                nextYard = 4;
            } else {
                nextYard = 5;
            }
            // ensure no backward move except bust
            if (nextYard != 0 && nextYard <= s) nextYard = s;
            
            T[s][nextYard] += p;
        }
    }
}

// solve full-horizon problem
void RazzleGame::solveOptimalStopping() {
    // intialize V[s] payout if stopped immediately
    for(int s = 0; s <= 5; s++) {
        V[s] = static_cast<double>(mapStepToPayout(s));
    }

    const double B = static_cast<double>(params.at("betP"));
    // value iter
    for(int _ = 0; _ < 100; _++) {
        std::array<double,6> Vnew = V;
        for(int s = 0; s < 5; s++) {
            // compute continuation EV from s
            double contEV = -B;
            for(int sp = 0; sp <= 5; sp++) {
                contEV += T[s][sp] * V[sp];
            }
            // best of stopping vs. continuing
            Vnew[s] = std::max(static_cast<double>(mapStepToPayout(s)), contEV);
        }
        // V[5] remains mapStepToPayout(5)
        V = Vnew;
    }

    // extract policy: continue if contEV > stopEV
    for(int s = 0; s <= 5; ++s) {
        double stopEV = static_cast<double>(mapStepToPayout(s));
        if (s == 5) {
            policy[s] = false; // you can’t continue from the last yard
        } else {
            double contEV = -static_cast<double>(params.at("betP"));
            for(int sp = 0; sp <= 5; ++sp) {
                contEV += T[s][sp] * V[sp];
            }
            policy[s] = (contEV > stopEV);
        }
    }
}

RazzleGame::RazzleGame(const std::map<std::string,int>& paramsMap, std::random_device& rnd) :
    params(paramsMap),
    minSum(params.at("numOfDiceP")),
    maxSum(params.at("numOfDiceP") * 6),
    engine(rnd()),
    outcomeStorageProfit() {
        recomputePolicy();
    }

bool RazzleGame::shouldContinue(int step) const {
    // lookup the precomputed policy
    return policy[step];
}

void RazzleGame::recomputePolicy() {
    computeSumDistribution();
    buildTransitionMatrix();
    solveOptimalStopping();
}

int RazzleGame::runGame() {
    std::uniform_int_distribution<int> dist(1, 6);
    int numberOfAttempts = params.at("maxTriesP");
    int step = 0;
    int paidIn = 0;
    int paidOut = 0;
    while (numberOfAttempts > 0) {
        paidIn += params.at("betP");
        numberOfAttempts--;

        int sum = 0;
        for (int i = 0; i < params.at("numOfDiceP"); i++) sum += dist(engine);

        bool advance = mapRollToYard(sum, step);
        if (advance) {
            step++;
            paidOut += mapStepToPayout(step);
        }

        // (recompute yardValue exactly as in buildTransitionMatrix)
        int newStep = step;
        if (sum <= params.at("yardsPerStep1P")) newStep = std::max(step, 1);
        else if (sum <= params.at("yardsPerStep2P")) newStep = std::max(step, 2);
        else if (sum <= params.at("yardsPerStep3P")) newStep = std::max(step, 3);
        else if (sum <= params.at("yardsPerStep4P")) newStep = std::max(step, 4);
        else newStep = 5;

        step = newStep;
        paidOut = mapStepToPayout(step);

        // decide whether to roll again
        if (!shouldContinue(step)) {
            // cash out here
            break;
        }
    }

    // compute profit as (paidOut - paidIn) for all games
    int profit = paidOut - paidIn;
    outcomeStorageProfit.push_back(profit);
    return profit;
}

// access parameters
const std::map<std::string,int>& RazzleGame::getParameters() const {
    return params;
}
