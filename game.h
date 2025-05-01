#include <cmath>
#include <vector>
#include <atomic>
#include <mutex>
#include <random>

class RazzleGame {
private:
    // learnable game paramters (P means parameter)
    std::atomic<int> betP;
    std::atomic<int> numOfDiceP;
    std::atomic<int> minNoWinP;
    std::atomic<int> maxNoWinP;
    std::vector<std::atomic<int>> yardsPerStepP; // const size 5
    std::vector<std::atomic<int>> payoutPerStepP; // const size 5
    std::atomic<int> maxTriesP;

    // game constants
    const int minSum;
    const int maxSum;

    // game mechanic storage
    int rollSum;
    std::mt19937 engine;

    int mapRollToYard(int S) const {
        // 1) check failure band
        int failMin = minNoWinP.load();
        int failMax = maxNoWinP.load();
        if (S >= failMin && S <= failMax) {
            return 0;
        }

        // 2) piecewise thresholds for steps 1..5
        for (size_t i = 0; i < yardsPerStepP.size(); ++i) {
            if (S <= yardsPerStepP[i].load()) {
                return static_cast<int>(i + 1);
            }
        }

        // 3) anything above the last threshold still counts as step 5
        return static_cast<int>(yardsPerStepP.size());
    }

};