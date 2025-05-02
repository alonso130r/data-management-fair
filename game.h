#include <cmath>
#include <vector>
#include <atomic>
#include <mutex>
#include <random>
#include <deque>

// todo: change all to be normal int
class RazzleGame {
private:
    // learnable game paramters (P means parameter)
    int betP;
    int numOfDiceP;
    int minNoWinP;
    int maxNoWinP;

    int yardsPerStep1P;
    int yardsPerStep2P;
    int yardsPerStep3P;
    int yardsPerStep4P;
    int yardsPerStep5P; 

    int payoutPerStep1P;
    int payoutPerStep2P;
    int payoutPerStep3P;
    int payoutPerStep4P;
    int payoutPerStep5P;

    int maxTriesP;

    // game constants
    const int minSum;
    const int maxSum;

    // game mechanic storage
    std::mt19937 engine;

    std::deque<std::unique_ptr<std::atomic<int>>> outcomeStorageProfit;

    bool mapRollToYard(int S, int current) const;
    int mapStepToPayout(int current) const;
public:
    // stupid ass constructor
    RazzleGame(int bet, int numOfDice, int minNoWin, int maxNoWin, 
                int yardsPerStep1, int yardsPerStep2, int yardsPerStep3, int yardsPerStep4, int yardsPerStep5,
                int payoutPerStep1, int payoutPerStep2, int payoutPerStep3, int payoutPerStep4, int payoutPerStep5,
                int maxTries, std::random_device rnd);

    void runGame();
};