#include "game.h"
#include <thread>
#include <random>
#include <map>
#include <string>
#include <cstddef>

class Simulation {
public:
    // initialize with starting parameters and optional thread count
    Simulation(const std::map<std::string, int>& initialParams, size_t threads = std::thread::hardware_concurrency());
    
    // get or set current parameters
    std::map<std::string, int> getParams() const;
    void setParams(const std::map<std::string, int>& param);

    // run discrete gradient descent for numOfRuns per evaluation
    void run(size_t numOfRuns);

private:
    std::map<std::string, int> params;
    size_t threadCount;
    std::random_device rd;
    // bounds for each parameter [min, max]
    std::map<std::string, std::pair<int,int>> bounds;
};