import numpy as np
import matplotlib.pyplot as plt

# Game parameters (from final_params1.txt)
params = {
    "payIn": 3,
    "numOfDiceP": 3,
    "noWinRangeP": 2,
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

def get_sum_probabilities(num_dice=3, faces=6):
    from itertools import product
    sums = [sum(x) for x in product(range(1, faces+1), repeat=num_dice)]
    values, counts = np.unique(sums, return_counts=True)
    prob = np.zeros(num_dice*faces+1)
    prob[values] = counts / counts.sum()
    return prob

sum_prob = get_sum_probabilities(params["numOfDiceP"])

D = params["numOfDiceP"]
minSum = D
maxSum = D * 6
mid = (minSum + maxSum) / 2.0
r = params["noWinRangeP"]
failMin = max(minSum, int(np.ceil(mid - r)))
failMax = min(maxSum, int(np.floor(mid + r)))

def next_step(sum_):
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

# Compute the distribution for a single roll
hist = {}
for sum_ in range(minSum, maxSum+1):
    p = sum_prob[sum_]
    if p == 0:
        continue
    step = next_step(sum_)
    if step == 0:
        profit = -params["payIn"]
    else:
        profit = payout(step) - params["payIn"]
    hist[profit] = hist.get(profit, 0) + p

# Ensure all possible profit values are present (for nice x-axis)
possible_profits = [
    -params["payIn"],  # bust
    params["payoutPerStep1P"] - params["payIn"],
    params["payoutPerStep2P"] - params["payIn"],
    params["payoutPerStep3P"] - params["payIn"],
    params["payoutPerStep4P"] - params["payIn"],
    params["payoutPerStep5P"] - params["payIn"],
]
profits = np.array(sorted(set(hist.keys()) | set(possible_profits)))
probs = np.array([hist.get(k, 0.0) for k in profits])

# Plot
plt.figure(figsize=(8,5))
bars = plt.bar(profits, probs, width=0.7, color='seagreen', edgecolor='k')

plt.xlabel('Net Profit (tokens)', fontsize=12)
plt.ylabel('Probability', fontsize=12)
plt.title('Distribution of Game Outcomes (One Roll)', fontsize=14)
plt.xticks(profits, [f"{int(x)}" for x in profits])
plt.grid(axis='y', alpha=0.3)

# Annotate bars with probabilities (always show value)
for bar, prob in zip(bars, probs):
    plt.text(bar.get_x() + bar.get_width()/2, bar.get_height(), f"{prob:.4%}", 
             ha='center', va='bottom', fontsize=10)

plt.tight_layout()
plt.savefig("histogram_one_roll.png", dpi=300)
plt.show()
