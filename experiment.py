import numpy as np
import matplotlib.pyplot as plt
from collections import Counter

# Game parameters (from final_params1.txt)
params = {
    "maxRolls": 5,
    "noWinRangeP": 2,
    "numOfDiceP": 3,
    "payIn": 3,
    "payoutPerStep1P": 1,
    "payoutPerStep2P": 2,
    "payoutPerStep3P": 4,
    "payoutPerStep4P": 6,
    "payoutPerStep5P": 9,
    "yardsPerStep1P": 3,
    "yardsPerStep2P": 7,
    "yardsPerStep3P": 14,
    "yardsPerStep4P": 16,
    "yardsPerStep5P": 18,
}

def next_step(sum_, cur):
    D = params["numOfDiceP"]
    minSum = D
    maxSum = D * 6
    mid = (minSum + maxSum) / 2.0
    r = params["noWinRangeP"]
    failMin = max(minSum, int(np.ceil(mid - r)))
    failMax = min(maxSum, int(np.floor(mid + r)))
    if failMin <= sum_ <= failMax:
        return 0
    if sum_ <= params["yardsPerStep1P"]:
        return 1
    if sum_ <= params["yardsPerStep2P"]:
        return 2
    if sum_ <= params["yardsPerStep3P"]:
        return 3
    if sum_ <= params["yardsPerStep4P"]:
        return 4
    return 5

def payout(step):
    if step < 1 or step > 5:
        return 0
    return params[f"payoutPerStep{step}P"]

def play_game():
    rolls_left = params["maxRolls"]
    step = 0
    paid_in = params["payIn"]
    paid_out = 0
    rng = np.random.default_rng()
    while rolls_left > 0:
        rolls_left -= 1
        dice = rng.integers(1, 7, size=params["numOfDiceP"])
        sum_ = dice.sum()
        next_s = next_step(sum_, step)
        if next_s == 0:
            return -paid_in  # bust
        if next_s < step:
            next_s = step
        step = next_s
        paid_out = payout(step)
        # Modified policy: stop if at step 3 or higher, or at step 2 with 1 roll left
        if step >= 3:
            break
        if step == 2 and rolls_left == 1:
            break
        # Otherwise, continue unless last roll
        if rolls_left == 0:
            break
    return paid_out - paid_in

# 1. Tally chart with collected experiment results from 50 games
results = [play_game() for _ in range(50)]
tally = Counter(results)

print("Tally chart (profit: count):")
for profit in sorted(tally):
    print(f"{profit:>3}: {tally[profit]}")
print()

# 2. Experimental probability of each possible game outcome
total_games = len(results)
exp_probs = {k: v / total_games for k, v in tally.items()}
print("Experimental probability (profit: probability):")
for profit in sorted(exp_probs):
    print(f"{profit:>3}: {exp_probs[profit]:.4f}")
print()

# 3. Graph of the experimental distribution
possible_profits = [
    -params["payIn"],  # bust
    params["payoutPerStep1P"] - params["payIn"],
    params["payoutPerStep2P"] - params["payIn"],
    params["payoutPerStep3P"] - params["payIn"],
    params["payoutPerStep4P"] - params["payIn"],
    params["payoutPerStep5P"] - params["payIn"],
]
profits = sorted(set(tally.keys()) | set(possible_profits))
probs = [exp_probs.get(k, 0.0) for k in profits]

plt.figure(figsize=(8,5))
bars = plt.bar(profits, probs, width=0.7, color='orange', edgecolor='k')
plt.xlabel('Net Profit (tokens)', fontsize=12)
plt.ylabel('Experimental Probability', fontsize=12)
plt.title('Experimental Distribution of Game Outcomes (50 games)', fontsize=14)
plt.xticks(profits, [f"{int(x)}" for x in profits])
plt.grid(axis='y', alpha=0.3)
for bar, prob in zip(bars, probs):
    plt.text(bar.get_x() + bar.get_width()/2, bar.get_height(), f"{prob:.2%}",
             ha='center', va='bottom', fontsize=10)
plt.tight_layout()
plt.savefig("histogram_experimental.png", dpi=300)
plt.show()

# 4. Experimental return per game
exp_return = np.mean(results)
print(f"Experimental average return per game: {exp_return:.4f} tokens")

# Sample calculation for experimental return:
# Suppose you had 50 games with the following tally:
# -3: 30 times, 1: 8 times, 3: 7 times, 6: 5 times
# Experimental return = (30*-3 + 8*1 + 7*3 + 5*6) / 50 = (-90 + 8 + 21 + 30) / 50 = (-31) / 50 = -