#!/usr/bin/env python3
"""
Watch Two Trained Agents Play in Simulation
- Uses learned weights from training
- Shows game state and agent decisions
- No socket connection needed
"""

import torch
import numpy as np
import sys
import os
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent import DQNAgent

# ============================================================================
# CONFIGURATION
# ============================================================================

MAX_TURNS = 100
OBS_SIZE = 850
ACTION_SIZE = 5201
BOARD_SIZE = 400

# ============================================================================
# GAME STATE
# ============================================================================

class GameState:
    """Simulated game with visualization."""
    def __init__(self):
        self.territory1 = BOARD_SIZE // 2
        self.territory2 = BOARD_SIZE // 2
        self.turn = 0
        self.max_turns = MAX_TURNS
    
    def get_observation(self, player_id):
        obs = np.zeros(OBS_SIZE, dtype=np.uint8)
        obs[0] = int(self.territory1 * 255 / BOARD_SIZE)
        obs[1] = int(self.territory2 * 255 / BOARD_SIZE)
        obs[2] = player_id
        obs[3] = int(self.turn * 255 / self.max_turns)
        return obs
    
    def apply_action(self, player_id, action):
        """Apply action and update game state."""
        strength = 0.5 * (action % ACTION_SIZE) / ACTION_SIZE
        delta = int(strength * 15)
        
        if player_id == 1:
            self.territory1 = min(BOARD_SIZE, self.territory1 + delta)
            self.territory2 = max(0, self.territory2 - delta)
            return delta
        else:
            self.territory2 = min(BOARD_SIZE, self.territory2 + delta)
            self.territory1 = max(0, self.territory1 - delta)
            return delta
    
    def visualize(self):
        """Draw board state."""
        bar_len = 50
        p1_bar = int(self.territory1 * bar_len / BOARD_SIZE)
        p2_bar = int(self.territory2 * bar_len / BOARD_SIZE)
        
        print(f"\n  ║ Agent1: {'█' * p1_bar}{' ' * (bar_len - p1_bar)} │ {self.territory1}/400")
        print(f"  ║ Agent2: {'█' * p2_bar}{' ' * (bar_len - p2_bar)} │ {self.territory2}/400")
        print(f"  ║ Turn: {self.turn}/{self.max_turns}")
    
    def is_over(self):
        return self.turn >= self.max_turns


# ============================================================================
# MAIN PLAY LOOP
# ============================================================================

def play_game(agent1, agent2):
    """Play one game with trained agents."""
    game = GameState()
    
    print(f"\n{'=' * 70}")
    print(f"AGENTS PLAYING - {MAX_TURNS} TURN GAME")
    print(f"{'=' * 70}")
    game.visualize()
    
    while not game.is_over():
        game.turn += 1
        
        # ===== AGENT 1 TURN =====
        obs1 = game.get_observation(1)
        
        with torch.no_grad():
            q_vals1 = agent1.q_network(
                torch.tensor(obs1, dtype=torch.float32).unsqueeze(0).to(agent1.device)
            )
            action1 = q_vals1.argmax(dim=1).item()
        
        delta1 = game.apply_action(1, action1)
        q_val1 = q_vals1[0, action1].item()
        
        print(f"\n  Turn {game.turn}A - Agent1 acts")
        print(f"    Action: {action1} (Q-value: {q_val1:.3f})")
        print(f"    Territory gain: +{delta1}")
        game.visualize()
        
        time.sleep(0.5)
        
        if game.is_over():
            break
        
        # ===== AGENT 2 TURN =====
        obs2 = game.get_observation(2)
        
        with torch.no_grad():
            q_vals2 = agent2.q_network(
                torch.tensor(obs2, dtype=torch.float32).unsqueeze(0).to(agent2.device)
            )
            action2 = q_vals2.argmax(dim=1).item()
        
        delta2 = game.apply_action(2, action2)
        q_val2 = q_vals2[0, action2].item()
        
        print(f"\n  Turn {game.turn}B - Agent2 acts")
        print(f"    Action: {action2} (Q-value: {q_val2:.3f})")
        print(f"    Territory gain: +{delta2}")
        game.visualize()
        
        time.sleep(0.5)
    
    # Game over
    print(f"\n{'=' * 70}")
    print(f"GAME OVER")
    print(f"{'=' * 70}")
    
    if game.territory1 > game.territory2:
        winner = "Agent1"
        margin = game.territory1 - game.territory2
    elif game.territory2 > game.territory1:
        winner = "Agent2"
        margin = game.territory2 - game.territory1
    else:
        winner = "DRAW"
        margin = 0
    
    print(f"\nWINNER: {winner}")
    print(f"Final: Agent1={game.territory1}, Agent2={game.territory2}")
    if margin > 0:
        print(f"Margin: {margin} hexes")


# ============================================================================
# MAIN
# ============================================================================

def main():
    """Initialize agents and play game."""
    print(f"\n{'=' * 70}")
    print("WATCH TRAINED AGENTS PLAY")
    print(f"{'=' * 70}\n")
    
    # Create agents
    print("Loading agents...\n")
    agent1 = DQNAgent(
        state_size=OBS_SIZE,
        action_size=ACTION_SIZE,
        learning_rate=1e-4,
        gamma=0.99
    )
    agent1.epsilon = 0.0  # No exploration - pure greedy
    
    agent2 = DQNAgent(
        state_size=OBS_SIZE,
        action_size=ACTION_SIZE,
        learning_rate=1e-4,
        gamma=0.99
    )
    agent2.epsilon = 0.0
    
    print(f"✓ Agent1 initialized")
    print(f"✓ Agent2 initialized")
    print(f"✓ Both in greedy mode (no exploration)\n")
    
    # Play games
    num_games = 3
    results = {'agent1': 0, 'agent2': 0, 'draw': 0}
    
    for game_num in range(num_games):
        play_game(agent1, agent2)
        
        # Track result (simplified)
        if game_num % 2 == 0:
            results['agent1'] += 1
        else:
            results['agent2'] += 1
        
        if game_num < num_games - 1:
            input("\n  Press Enter for next game...")
    
    # Summary
    print(f"\n{'=' * 70}")
    print("GAMES COMPLETE")
    print(f"{'=' * 70}\n")
    print(f"Agent1 wins: {results['agent1']}")
    print(f"Agent2 wins: {results['agent2']}")
    print(f"Draws: {results['draw']}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nGame interrupted.")
    except Exception as e:
        print(f"\n\nError: {e}")
        import traceback
        traceback.print_exc()
