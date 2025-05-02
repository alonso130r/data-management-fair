#include <game.h>

bool RazzleGame::mapRollToYard(int S, int current) const {
     // 1) check failure band
    int failMin = minNoWinP;
    int failMax = maxNoWinP;
    if (S >= failMin && S <= failMax) {
        return false;
    }
    // 2) calculate the yard value based on the roll
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
    // 3) return true if the yard value is greater than the current step
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

RazzleGame::RazzleGame(int bet, int numOfDice, int minNoWin, int maxNoWin, 
                        int yardsPerStep1, int yardsPerStep2, int yardsPerStep3, int yardsPerStep4, int yardsPerStep5,
                        int payoutPerStep1, int payoutPerStep2, int payoutPerStep3, int payoutPerStep4, int payoutPerStep5,
                        int maxTries, std::random_device rnd) :
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
    outcomeStorageProfit() 
{}

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
            paidOut += mapStepToPayout(step);
            // todo: implement realistic cash out/continue playing logic to produce good data
        }
    }
}