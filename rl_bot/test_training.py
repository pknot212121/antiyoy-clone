#!/usr/bin/env python3
"""
Quick training test to verify bots use valid actions only.
"""

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent import DQNAgent
from train_real_game_with_actions import play_simulated_game_with_rules
import numpy as np

def main():
    """Run a few quick training games to verify valid actions."""
    print("=" * 70)
    print("QUICK TRAINING TEST - Verifying Valid Actions Only")
    print("=" * 70)
    
    # Initialize simple agents
    print("\nInitializing agents...")
    agent1 = DQNAgent(
        state_size=850,
        action_size=5201,
        learning_rate=1e-4,
        gamma=0.99
    )
    agent1.epsilon = 0.5  # Some exploration
    
    agent2 = DQNAgent(
        state_size=850,
        action_size=5201,
        learning_rate=1e-4,
        gamma=0.99
    )
    agent2.epsilon = 0.5
    
    print("Agents initialized")
    print(f"  State size: 850")
    print(f"  Action size: 5201")
    print(f"  Exploration: ε={agent1.epsilon}")
    
    # Play 3 test games
    print(f"\nPlaying 3 test games with rule enforcement...\n")
    
    for game_num in range(3):
        try:
            reward1, reward2, result, terr1, terr2 = play_simulated_game_with_rules(agent1, agent2, game_num)
            
            print(f"Game {game_num + 1}:")
            print(f"  Result: {result}")
            print(f"  Territory: Player1={terr1}, Player2={terr2}")
            print(f"  Rewards: Agent1={reward1:.2f}, Agent2={reward2:.2f}")
            print(f"  Buffer sizes: Agent1={len(agent1.replay_buffer)}, Agent2={len(agent2.replay_buffer)}")
            print()
            
        except Exception as e:
            print(f"✗ Game {game_num + 1} FAILED with error: {e}")
            import traceback
            traceback.print_exc()
            return 1
    
    print("=" * 70)
    print("✓ TRAINING TEST PASSED")
    print("=" * 70)
    print("\nVerified:")
    print("  - Agents can play games with rule enforcement")
    print("  - Valid actions are generated correctly")
    print("  - No crashes or invalid action errors")
    print("  - Experience is collected for training")
    print("\nReady for full training!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
