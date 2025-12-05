#!/usr/bin/env python3
"""
Train RL agents by playing against each other in the real Antiyoy game.
The game will be visible on screen.
"""

import os
import sys
import time
import subprocess
import signal
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent import DQNAgent
from antiyoy_env import AntiyoyEnv

# Training configuration
NUM_GAMES = 10
LEARNING_RATE = 1e-4
GAMMA = 0.99
EPSILON_START = 0.3  # Lower exploration for visible games
EPSILON_END = 0.05
EPSILON_DECAY = 0.95
BATCH_SIZE = 32
UPDATE_FREQUENCY = 2

OBS_SIZE = 850
ACTION_SIZE = 5201

game_process = None


def start_game():
    """Start the Antiyoy game executable."""
    game_path = "/home/mikolaj/work/antijoy-clone/build/anti"
    work_dir = "/home/mikolaj/work/antijoy-clone"
    
    if not os.path.exists(game_path):
        print(f"ERROR: Game executable not found at {game_path}")
        print("Please build the game first:")
        print("  cd /home/mikolaj/work/antijoy-clone/build")
        print("  cmake .. && make")
        return None
    
    print(f"Starting Antiyoy game...")
    print(f"  Executable: {game_path}")
    print(f"  Working directory: {work_dir}")
    
    # Start game in background from project root directory (to find config.txt)
    process = subprocess.Popen(
        [game_path],
        cwd=work_dir,  # Run from project root to find config.txt
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        preexec_fn=os.setsid
    )
    
    # Wait for game to start
    print("Waiting for game to initialize (3 seconds)...")
    time.sleep(3)
    
    # Check if still running
    if process.poll() is not None:
        print("ERROR: Game failed to start")
        return None
    
    print("✓ Game started successfully")
    return process


def stop_game(process):
    """Stop the game process."""
    if process:
        print("\nStopping game...")
        os.killpg(os.getpgid(process.pid), signal.SIGTERM)
        time.sleep(1)
        print("✓ Game stopped")


def select_action_with_valid_only(agent, obs, env, training=True):
    """Select action from valid actions only."""
    valid_actions = env.get_valid_actions()
    
    if len(valid_actions) <= 1:
        return 0  # Only END_TURN available
    
    if training and np.random.random() < agent.epsilon:
        # Explore: random valid action
        return np.random.choice(valid_actions)
    else:
        # Exploit: best valid action
        import torch
        with torch.no_grad():
            q_values = agent.q_network(
                torch.tensor(obs, dtype=torch.float32).unsqueeze(0).to(agent.device)
            ).cpu().numpy()[0]
        
        # Mask invalid actions
        masked_q = np.full_like(q_values, -np.inf)
        masked_q[valid_actions] = q_values[valid_actions]
        
        return np.argmax(masked_q)


def play_real_game(agent1, agent2, env, game_num):
    """
    Play one game with both agents controlling players in the real game.
    The game is visible on screen.
    """
    print(f"\n{'='*70}")
    print(f"GAME {game_num + 1}")
    print(f"{'='*70}")
    
    obs = env.reset()
    
    print(f"\nBoard initialized:")
    print(f"  Player 1 territory: {env._count_territory(1)} hexes")
    print(f"  Player 2 territory: {env._count_territory(2)} hexes")
    print(f"  Player 1 money: {env.player_money.get(1, 0)}")
    print(f"  Player 2 money: {env.player_money.get(2, 0)}")
    
    agent1_rewards = []
    agent2_rewards = []
    turn = 0
    max_turns = 50
    
    print(f"\nGame started. Watch the Antiyoy window on your screen!")
    print(f"Actions will be shown below:\n")
    
    while turn < max_turns:
        # Player 1 (Agent 1) turn
        env.current_player_id = 1
        obs1 = env._encode_observation()
        
        valid_actions1 = env.get_valid_actions()
        print(f"[Turn {turn}] Player 1: {len(valid_actions1)} valid actions available")
        
        if len(valid_actions1) <= 1:
            print(f"  → Only END_TURN available, ending turn")
            action1 = 0
        else:
            action1 = select_action_with_valid_only(agent1, obs1, env, training=True)
            action_type, x, y, param = env._decode_action(action1)
            print(f"  → Action: {action_type} at ({x},{y})")
        
        obs1_next, reward1, done, info = env.step(action1)
        
        agent1_rewards.append(reward1)
        agent1.remember(obs1, action1, reward1, obs1_next, done)
        
        if info.get('invalid_action'):
            print(f"  ✗ Invalid action!")
        
        if done:
            break
        
        turn += 1
        
        # Player 2 (Agent 2) turn
        env.current_player_id = 2
        obs2 = env._encode_observation()
        
        valid_actions2 = env.get_valid_actions()
        print(f"[Turn {turn}] Player 2: {len(valid_actions2)} valid actions available")
        
        if len(valid_actions2) <= 1:
            print(f"  → Only END_TURN available, ending turn")
            action2 = 0
        else:
            action2 = select_action_with_valid_only(agent2, obs2, env, training=True)
            action_type, x, y, param = env._decode_action(action2)
            print(f"  → Action: {action_type} at ({x},{y})")
        
        obs2_next, reward2, done, info = env.step(action2)
        
        agent2_rewards.append(reward2)
        agent2.remember(obs2, action2, reward2, obs2_next, done)
        
        if info.get('invalid_action'):
            print(f"  ✗ Invalid action!")
        
        if done:
            break
        
        turn += 1
        
        # Small delay so you can see the moves
        time.sleep(0.5)
    
    total_reward1 = sum(agent1_rewards)
    total_reward2 = sum(agent2_rewards)
    
    print(f"\nGame {game_num + 1} complete:")
    print(f"  Turns: {turn}")
    print(f"  Agent1 total reward: {total_reward1:.2f}")
    print(f"  Agent2 total reward: {total_reward2:.2f}")
    
    return total_reward1, total_reward2


def train_with_real_game():
    """Train agents by playing in the real visible game."""
    global game_process
    
    print("="*70)
    print("ANTIYOY RL TRAINING - REAL GAME WITH VISUALIZATION")
    print("="*70)
    print()
    
    # Start the game
    game_process = start_game()
    if not game_process:
        print("\nFailed to start game. Exiting.")
        return
    
    try:
        # Initialize agents
        print("\nInitializing agents...")
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
        print(f"  Exploration rate: ε={EPSILON_START}")
        print()
        
        # Create environment connected to real game
        print("Connecting to game...")
        env = AntiyoyEnv(host="127.0.0.1", port=2137)
        print("✓ Connected to game")
        
        print(f"\nStarting training: {NUM_GAMES} games")
        print("Watch your screen to see the agents play!\n")
        
        # Play games
        rewards_history = {'agent1': [], 'agent2': []}
        
        for game_num in range(NUM_GAMES):
            reward1, reward2 = play_real_game(agent1, agent2, env, game_num)
            
            rewards_history['agent1'].append(reward1)
            rewards_history['agent2'].append(reward2)
            
            # Train both agents
            if len(agent1.replay_buffer) >= BATCH_SIZE:
                for _ in range(UPDATE_FREQUENCY):
                    agent1.train_step(BATCH_SIZE)
                    agent2.train_step(BATCH_SIZE)
            
            # Decay exploration
            agent1.decay_epsilon()
            agent2.decay_epsilon()
            
            # Update target networks periodically
            if (game_num + 1) % 5 == 0:
                agent1.update_target_network()
                agent2.update_target_network()
            
            print(f"  Buffer sizes: A1={len(agent1.replay_buffer)}, A2={len(agent2.replay_buffer)}")
            print(f"  Exploration: ε={agent1.epsilon:.4f}")
        
        print("\n" + "="*70)
        print("TRAINING COMPLETE")
        print("="*70)
        print(f"\nAgent1 avg reward: {np.mean(rewards_history['agent1']):.2f}")
        print(f"Agent2 avg reward: {np.mean(rewards_history['agent2']):.2f}")
        
        # Save models
        agent1.save("/home/mikolaj/work/antijoy-clone/rl_bot/models/agent1_real_game.pth")
        agent2.save("/home/mikolaj/work/antijoy-clone/rl_bot/models/agent2_real_game.pth")
        print("\n✓ Models saved to rl_bot/models/")
        
        env.close()
        
    except KeyboardInterrupt:
        print("\n\nTraining interrupted by user")
    except Exception as e:
        print(f"\n\nError during training: {e}")
        import traceback
        traceback.print_exc()
    finally:
        # Always stop the game
        stop_game(game_process)


if __name__ == "__main__":
    train_with_real_game()
