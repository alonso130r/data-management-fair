#include <cmath>
#include <vector>
#include <atomic>
#include <mutex>
#include <random>
#include <deque>
#include <map>
#include <numeric>
#include <tbb/concurrent_vector.h>

class RazzleGame {
private:
    // learnable game paramters 
    std::map<std::string,int> params;

    // game constants
    const int minSum;
    const int maxSum;

    // game mechanic storage
    std::mt19937 engine;
    std::map<int, double> sumProb;               // probability distribution
    std::array<std::array<double, 6>, 6> T;       // transition probabilities
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
    // stupid ass constructor
    RazzleGame(const std::map<std::string,int>& params, std::random_device& rnd);

    void runGame();

    std::map<std::string,int> getParameters() const;
    void setParameters(const std::map<std::string,int>& updates);
};