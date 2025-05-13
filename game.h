#include <cmath>
#include <vector>
#include <mutex>
#include <random>
#include <deque>
#include <map>
#include <numeric>
#include <tbb/concurrent_vector.h>

class RazzleGame {
private:
    // learnable game paramters 
    std::map<std::string, int> params;

    // game constants
    const int minSum;
    const int maxSum;

    // game mechanic storage
    std::mt19937 engine;
    std::map<int, double> sumProb;               // probability distribution
    std::array<std::array<double, 6>, 6> T;      // transition probabilities
    std::array<bool, 6> policy;                  // true=CONTINUE, false=STOP
    std::array<double, 6> V;                     // value function

    tbb::concurrent_vector<int> outcomeStorageProfit;

    bool mapRollToYard(int S, int current) const;
    int mapStepToPayout(int current) const;
    bool shouldContinue(int step) const;

    void computeSumDistribution();
    void buildTransitionMatrix();
    void solveOptimalStopping();
    void recomputePolicy();
public:
    // not so stupid ass constructor
    RazzleGame(const std::map<std::string, int>& paramsMap, std::random_device& rnd);

    // run a single game and return profit (paidOut - paidIn)
    int runGame();

    // access parameters
    const std::map<std::string, int>& getParameters() const;
};