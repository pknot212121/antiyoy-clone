#!/usr/bin/env python3
"""
Train RL agents for 1000 games using rule-enforced gameplay.
Fast training without visualization.
"""

import os
import sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent import DQNAgent
from antiyoy_env import AntiyoyEnv

# Training configuration
NUM_GAMES = 1000
LEARNING_RATE = 1e-4
GAMMA = 0.99
EPSILON_START = 1.0
EPSILON_END = 0.05
EPSILON_DECAY = 0.9995
BATCH_SIZE = 32
UPDATE_FREQUENCY = 4

OBS_SIZE = 850
ACTION_SIZE = 5201


def select_action_with_valid_only(agent, obs, env, training=True):
    """Select action from valid actions only."""
    valid_actions = env.get_valid_actions()
    
    if len(valid_actions) <= 1:
        return 0
    
    if training and np.random.random() < agent.epsilon:
        return np.random.choice(valid_actions)
    else:
        import torch
        with torch.no_grad():
            q_values = agent.q_network(
                torch.tensor(obs, dtype=torch.float32).unsqueeze(0).to(agent.device)
            ).cpu().numpy()[0]
        
        masked_q = np.full_like(q_values, -np.inf)
        masked_q[valid_actions] = q_values[valid_actions]
        
        return np.argmax(masked_q)


def play_training_game(agent1, agent2, env, game_num):
    """Play one training game using local board state."""
    # Use local simulation (no real game) for fast training
    env.connected = True
    env.board_state = env._generate_board(seed=game_num + 2000)
    
    # Initialize player money
    env.player_money = {}
    for player_id in range(1, 3):
        castle_count = sum(1 for cell in env.board_state.values()
                         if cell.get('owner_id') == player_id and 
                         cell.get('resident', '').lower() == 'castle')
        if castle_count > 0:
            env.player_money[player_id] = 100 * castle_count
    
    env.current_player_id = 1
    env.step_count = 0
    
    agent1_rewards = []
    agent2_rewards = []
    max_turns = 50
    
    # Track initial territories
    territory1_prev = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 1)
    territory2_prev = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 2)
    
    while env.step_count < max_turns:
        # Player 1 turn
        env.current_player_id = 1
        obs1 = env._encode_observation()
        valid_actions1 = env.get_valid_actions()
        
        if len(valid_actions1) <= 1:
            action1 = 0
            reward1 = -0.1  # Penalty for having no moves
        else:
            action1 = select_action_with_valid_only(agent1, obs1, env, training=True)
            
            # Decode action to see what type it is
            action_type, x, y, param = env._decode_action(action1)
            
            # Calculate territory before action
            territory1_before = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 1)
            
            # Reward based on action type
            if action1 == 0:
                reward1 = -0.05  # Small penalty for ending turn
            elif action_type == "move":
                # HEAVILY reward warrior movements (these expand territory!)
                reward1 = 0.5
            elif action_type == "place":
                # Check what's being placed
                from socket_client import Resident
                if param in [Resident.WARRIOR1.value, Resident.WARRIOR2.value, 
                           Resident.WARRIOR3.value, Resident.WARRIOR4.value]:
                    # Reward building warriors (they can capture territory)
                    reward1 = 0.3
                elif param == Resident.FARM.value:
                    # Reward building farms (economy)
                    reward1 = 0.2
                else:
                    # Towers/buildings
                    reward1 = 0.1
            else:
                reward1 = 0.0
            
            # BONUS: Reward territorial expansion
            territory1_after = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 1)
            territory_gain = territory1_after - territory1_before
            if territory_gain > 0:
                reward1 += territory_gain * 2.0  # Big bonus for capturing land!
        
        agent1_rewards.append(reward1)
        agent1.remember(obs1, action1, reward1, obs1, done=False)
        
        env.step_count += 1
        if env.step_count >= max_turns:
            break
        
        # Player 2 turn (same logic)
        env.current_player_id = 2
        obs2 = env._encode_observation()
        valid_actions2 = env.get_valid_actions()
        
        if len(valid_actions2) <= 1:
            action2 = 0
            reward2 = -0.1
        else:
            action2 = select_action_with_valid_only(agent2, obs2, env, training=True)
            
            action_type, x, y, param = env._decode_action(action2)
            territory2_before = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 2)
            
            if action2 == 0:
                reward2 = -0.05
            elif action_type == "move":
                reward2 = 0.5  # Heavily reward moves
            elif action_type == "place":
                from socket_client import Resident
                if param in [Resident.WARRIOR1.value, Resident.WARRIOR2.value,
                           Resident.WARRIOR3.value, Resident.WARRIOR4.value]:
                    reward2 = 0.3
                elif param == Resident.FARM.value:
                    reward2 = 0.2
                else:
                    reward2 = 0.1
            else:
                reward2 = 0.0
            
            territory2_after = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 2)
            territory_gain = territory2_after - territory2_before
            if territory_gain > 0:
                reward2 += territory_gain * 2.0
        
        agent2_rewards.append(reward2)
        agent2.remember(obs2, action2, reward2, obs2, done=False)
        
        env.step_count += 1
    
    return sum(agent1_rewards), sum(agent2_rewards)


def main():
    """Train agents for 1000 games."""
    print("="*70)
    print("ANTIYOY RL TRAINING - 1000 GAMES")
    print("="*70)
    print()
    
    # Initialize agents
    print("Initializing agents...")
    agent1 = DQNAgent(
        state_size=OBS_SIZE,
        action_size=ACTION_SIZE,
        learning_rate=LEARNING_RATE,
        gamma=GAMMA,
        epsilon_start=EPSILON_START,
        epsilon_end=EPSILON_END,
        epsilon_decay=EPSILON_DECAY
    )
    agent1.epsilon = EPSILON_START
    
    agent2 = DQNAgent(
        state_size=OBS_SIZE,
        action_size=ACTION_SIZE,
        learning_rate=LEARNING_RATE,
        gamma=GAMMA,
        epsilon_start=EPSILON_START,
        epsilon_end=EPSILON_END,
        epsilon_decay=EPSILON_DECAY
    )
    agent2.epsilon = EPSILON_START
    
    print("✓ Agents initialized")
    print(f"  Training games: {NUM_GAMES}")
    print(f"  Learning rate: {LEARNING_RATE}")
    print(f"  Gamma: {GAMMA}")
    print(f"  Exploration: ε={EPSILON_START} → {EPSILON_END}")
    print()
    
    # Create environment (no connection needed for training)
    env = AntiyoyEnv()
    
    # Training loop
    print("Starting training...")
    print()
    
    rewards_history = {'agent1': [], 'agent2': []}
    
    for game_num in range(NUM_GAMES):
        reward1, reward2 = play_training_game(agent1, agent2, env, game_num)
        
        rewards_history['agent1'].append(reward1)
        rewards_history['agent2'].append(reward2)
        
        # Train both agents
        if len(agent1.replay_buffer) >= BATCH_SIZE:
            for _ in range(UPDATE_FREQUENCY):
                loss1 = agent1.train_step(BATCH_SIZE)
                loss2 = agent2.train_step(BATCH_SIZE)
        
        # Decay exploration
        agent1.decay_epsilon()
        agent2.decay_epsilon()
        
        # Update target networks periodically
        if (game_num + 1) % 10 == 0:
            agent1.update_target_network()
            agent2.update_target_network()
        
        # Print progress
        if (game_num + 1) % 100 == 0:
            avg_r1 = np.mean(list(rewards_history['agent1'])[-100:])
            avg_r2 = np.mean(list(rewards_history['agent2'])[-100:])
            print(f"Game {game_num+1:4d}/{NUM_GAMES} | "
                  f"A1 avg: {avg_r1:6.2f} | A2 avg: {avg_r2:6.2f} | "
                  f"ε={agent1.epsilon:.4f} | "
                  f"Buffer: {len(agent1.replay_buffer)}/{len(agent2.replay_buffer)}")
        
        # Save checkpoints
        if (game_num + 1) % 250 == 0:
            agent1.save(f"/home/mikolaj/work/antijoy-clone/rl_bot/models/agent1_checkpoint_{game_num+1}.pth")
            agent2.save(f"/home/mikolaj/work/antijoy-clone/rl_bot/models/agent2_checkpoint_{game_num+1}.pth")
            print(f"  → Checkpoint saved at game {game_num+1}")
    
    print()
    print("="*70)
    print("TRAINING COMPLETE")
    print("="*70)
    print()
    print(f"Agent1 avg reward: {np.mean(rewards_history['agent1']):.2f}")
    print(f"Agent2 avg reward: {np.mean(rewards_history['agent2']):.2f}")
    print(f"Final exploration: ε={agent1.epsilon:.4f}")
    print()
    
    # Save final models
    agent1.save("/home/mikolaj/work/antijoy-clone/rl_bot/models/agent1_final.pth")
    agent2.save("/home/mikolaj/work/antijoy-clone/rl_bot/models/agent2_final.pth")
    print("✓ Final models saved:")
    print("  - models/agent1_final.pth")
    print("  - models/agent2_final.pth")
    print()
    print("Ready to watch agents play! Run:")
    print("  python3 watch_trained_agents.py")


if __name__ == "__main__":
    main()
