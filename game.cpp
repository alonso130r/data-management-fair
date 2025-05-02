#include <game.h>

bool RazzleGame::mapRollToYard(int S, int current) const {
     // check failure band
    int failMin = minNoWinP;
    int failMax = maxNoWinP;
    if (S >= failMin && S <= failMax) {
        return false;
    }
    // calculate the yard value based on the roll
    int yardValue;
    if (S <= yardsPerStep1P) {
        yardValue = 1;
    } else if (S <= yardsPerStep2P) {
        yardValue = 2;
    } else if (S <= yardsPerStep3P) {
        yardValue = 3;
    } else if (S <= yardsPerStep4P) {
        yardValue = 4;
    } else {
        yardValue = 5;
    }
    // return true if the yard value is greater than the current step
    return yardValue > current;
}

int RazzleGame::mapStepToPayout(int current) const {
    if (current == 1) {
        return payoutPerStep1P;
    } else if (current == 2) {
        return payoutPerStep2P;
    } else if (current == 3) {
        return payoutPerStep3P;
    } else if (current == 4) {
        return payoutPerStep4P;
    } else if (current == 5) {
        return payoutPerStep5P;
    }
    return 0;
}

// fill sumProb
void RazzleGame::computeSumDistribution() {
    int D = numOfDiceP;
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
            
            // get actual yard value
            int nextYard;
            if (roll >= minNoWinP && roll <= maxNoWinP) {
                nextYard = 0;
            } else if (roll <= yardsPerStep1P) {
                nextYard = 1;
            } else if (roll <= yardsPerStep2P) {
                nextYard = 2;
            } else if (roll <= yardsPerStep3P) {
                nextYard = 3;
            } else if (roll <= yardsPerStep4P) {
                nextYard = 4;
            } else {
                nextYard = 5;
            }
            
            // you can only advance, not drop below 0, if it's not > s then it's effectively staying at s (or bust if maxTries has been exceeded)
            if (nextYard <= s) nextYard = (roll >= minNoWinP && roll <= maxNoWinP) ? 0 : s;
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

    const double B = static_cast<double>(betP);
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
            double contEV = -static_cast<double>(betP);
            for(int sp = 0; sp <= 5; ++sp) {
                contEV += T[s][sp] * V[sp];
            }
            policy[s] = (contEV > stopEV);
        }
    }
}

RazzleGame::RazzleGame(int bet, int numOfDice, int minNoWin, int maxNoWin, 
                        int yardsPerStep1, int yardsPerStep2, int yardsPerStep3, int yardsPerStep4, int yardsPerStep5,
                        int payoutPerStep1, int payoutPerStep2, int payoutPerStep3, int payoutPerStep4, int payoutPerStep5,
                        int maxTries, std::random_device& rnd) :
    betP(bet),
    numOfDiceP(numOfDice),
    minNoWinP(minNoWin),
    maxNoWinP(maxNoWin),
    yardsPerStep1P(yardsPerStep1),
    yardsPerStep2P(yardsPerStep2),
    yardsPerStep3P(yardsPerStep3),
    yardsPerStep4P(yardsPerStep4),
    yardsPerStep5P(yardsPerStep5),
    payoutPerStep1P(payoutPerStep1),
    payoutPerStep2P(payoutPerStep2),
    payoutPerStep3P(payoutPerStep3),
    payoutPerStep4P(payoutPerStep4),
    payoutPerStep5P(payoutPerStep5),
    maxTriesP(maxTries),
    minSum(numOfDice),
    maxSum(numOfDice * 6),
    engine(rnd()),
    outcomeStorageProfit() {
        computeSumDistribution();
        buildTransitionMatrix();
        solveOptimalStopping();
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

void RazzleGame::runGame() {
    std::uniform_int_distribution<int> dist(1, 6);
    int numberOfAttempts = maxTriesP;
    int step = 0;
    int paidIn = 0;
    int paidOut = 0;
    while (numberOfAttempts > 0) {
        paidIn += betP;
        numberOfAttempts--;

        int sum = 0;
        for (int i = 0; i < numOfDiceP; i++) sum += dist(engine);

        bool advance = mapRollToYard(sum, step);
        if (advance) {
            step++;
            paidOut += mapStepToPayout(step);
        }

        // (recompute yardValue exactly as in buildTransitionMatrix)
        int newStep = step;
        if (sum <= yardsPerStep1P) newStep = std::max(step, 1);
        else if (sum <= yardsPerStep2P) newStep = std::max(step, 2);
        else if (sum <= yardsPerStep3P) newStep = std::max(step, 3);
        else if (sum <= yardsPerStep4P) newStep = std::max(step, 4);
        else newStep = 5;

        step = newStep;
        paidOut = mapStepToPayout(step);

        // decide whether to roll again
        if (!shouldContinue(step)) {
            // cash out here
            break;
        }
    }

    // store profit = paidOut - paidIn, or profit = 0 if numberOfAttempts == maxTriesP
    if (numberOfAttempts < maxTriesP) {
        outcomeStorageProfit.push_back(
            std::make_unique<std::atomic<int>>(paidOut - paidIn)
        );
    } else {
        outcomeStorageProfit.push_back(
            std::make_unique<std::atomic<int>>(0)
        ); 
    }
}