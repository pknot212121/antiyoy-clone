#!/usr/bin/env python3
"""
GPU-accelerated training with large DQN model.
Uses bigger network architecture and larger replay buffer for better learning.
"""

import os
import sys
import time
import numpy as np
import subprocess
import signal
import torch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent import DQNAgent
from antiyoy_env import AntiyoyEnv

# Training configuration
NUM_GAMES = 1000
LEARNING_RATE = 3e-4  # Higher LR for bigger model
GAMMA = 0.99
BATCH_SIZE = 128  # Bigger batches for GPU
UPDATE_FREQUENCY = 4
REPLAY_BUFFER_SIZE = 50000  # 10x larger buffer
HIDDEN_SIZES = [1024, 1024, 512, 256]  # Much bigger network - ~3M parameters

# Progressive epsilon schedule - start lower so network influences decisions sooner
EPSILON_SCHEDULE = {
    0: 0.5,      # Start at 50% random (was 100%)
    100: 0.3,    # Reduce to 30%
    300: 0.15,   # Further reduce
    600: 0.1     # Minimal exploration
}

# Balancing configuration
POSITION_SWAP_INTERVAL = 10
WIN_RATE_WINDOW = 50
BALANCE_THRESHOLD = 0.70

# Checkpointing
CHECKPOINT_INTERVAL = 100
CHECKPOINT_DIR = "/home/mikolaj/work/antijoy-clone/rl_bot/models/gpu_checkpoints"

OBS_SIZE = 834
ACTION_SIZE = 833


def get_epsilon_for_game(game_num):
    """Get epsilon value based on game number (progressive schedule)."""
    for threshold in sorted(EPSILON_SCHEDULE.keys(), reverse=True):
        if game_num >= threshold:
            return EPSILON_SCHEDULE[threshold]
    return 1.0


def start_game_process():
    """Start the C++ game process."""
    game_path = "/home/mikolaj/work/antijoy-clone/build/anti"
    config_path = "/home/mikolaj/work/antijoy-clone/Antiyoy/config.txt"
    
    print(f"Starting game: {game_path} {config_path}")
    
    process = subprocess.Popen(
        [game_path, config_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        preexec_fn=os.setsid
    )
    
    print("Waiting for game server to initialize...")
    time.sleep(5)
    print("Game server should be ready")
    return process


def play_selfplay_game(player1_agent, player2_agent, env, game_process):
    """
    Play one self-play game with 1 action per turn.
    Returns (winner, reward1, reward2, new_game_process).
    """
    # Kill old game process if it exists
    if game_process is not None:
        try:
            os.killpg(os.getpgid(game_process.pid), signal.SIGTERM)
            time.sleep(1.5)
        except:
            pass
        
        # Close old socket connection
        if env.connected:
            env.client.close()
            env.connected = False
            time.sleep(0.5)
    
    # Start fresh game process
    game_process = start_game_process()
    
    # Reset environment
    obs = env.reset(force_new_game=False)
    time.sleep(0.2)
    
    # Add randomness variation per game (important for exploration diversity)
    np.random.seed(int(time.time() * 1000) % 2**32)
    
    agent1_experiences = []  # Track experience indices
    agent2_experiences = []
    
    done = False
    turn = 0
    max_turns = 500
    
    while not done and turn < max_turns:
        # Player 1's turn - EXACTLY 1 action
        env.current_player_id = 1
        
        valid_actions = []
        for attempt in range(3):
            valid_actions = env.get_valid_actions_from_server()
            if valid_actions:
                break
            time.sleep(0.1)
        
        if not valid_actions:
            valid_actions = [0]  # Skip if no valid actions
        
        # Select ONE action
        action1 = player1_agent.select_action(obs, training=True)
        if action1 not in valid_actions:
            action1 = np.random.choice(valid_actions)
        
        # Debug output every 10 turns
        if turn % 10 == 0:
            action_type = "Skip" if action1 == 0 else f"Action#{action1}"
            print(f"  Turn {turn}: P1 {action_type}, {len(valid_actions)} valid actions")
        
        # Execute action (turn automatically ends)
        next_obs, reward1, done, _ = env.step(action1)
        player1_agent.remember(obs, action1, reward1, next_obs, done)
        agent1_experiences.append(len(player1_agent.replay_buffer) - 1)
        
        if done:
            break
        obs = next_obs
        
        # Player 2's turn - EXACTLY 1 action
        env.current_player_id = 2
        
        valid_actions = []
        for attempt in range(3):
            valid_actions = env.get_valid_actions_from_server()
            if valid_actions:
                break
            time.sleep(0.1)
        
        if not valid_actions:
            valid_actions = [0]  # Skip if no valid actions
        
        # Select ONE action
        action2 = player2_agent.select_action(obs, training=True)
        if action2 not in valid_actions:
            action2 = np.random.choice(valid_actions)
        
        # Debug output every 10 turns
        if turn % 10 == 0:
            action_type = "Skip" if action2 == 0 else f"Action#{action2}"
            print(f"  Turn {turn}: P2 {action_type}, {len(valid_actions)} valid actions")
        
        # Execute action (turn automatically ends)
        next_obs, reward2, done, _ = env.step(action2)
        player2_agent.remember(obs, action2, reward2, next_obs, done)
        agent2_experiences.append(len(player2_agent.replay_buffer) - 1)
        
        if done:
            break
        obs = next_obs
        turn += 1
    
    # Determine winner and assign FINAL rewards to last experience
    player1_castles = sum(1 for cell in env.board_state.values()
                         if cell.get('owner_id') == 1 and 
                         cell.get('resident', '').lower() == 'castle')
    player2_castles = sum(1 for cell in env.board_state.values()
                         if cell.get('owner_id') == 2 and 
                         cell.get('resident', '').lower() == 'castle')
    
    if player1_castles == 0:
        final_reward1, final_reward2 = -100.0, +100.0
        winner = 2
    elif player2_castles == 0:
        final_reward1, final_reward2 = +100.0, -100.0
        winner = 1
    else:
        territory1 = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 1)
        territory2 = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 2)
        if territory1 > territory2:
            final_reward1, final_reward2 = +10.0, -10.0
            winner = 1
        elif territory2 > territory1:
            final_reward1, final_reward2 = -10.0, +10.0
            winner = 2
        else:
            final_reward1, final_reward2 = -20.0, -20.0
            winner = 0
    
    # Assign final game reward to LAST experience of each agent
    # ADD to existing reward instead of replacing (preserves castle destruction bonuses)
    if agent1_experiences:
        last_idx1 = agent1_experiences[-1]
        state, action, existing_reward, next_state, done = player1_agent.replay_buffer.buffer[last_idx1]
        # Add final game outcome to any existing step rewards
        total_reward = existing_reward + final_reward1
        player1_agent.replay_buffer.buffer[last_idx1] = (state, action, total_reward, next_state, done)
    
    if agent2_experiences:
        last_idx2 = agent2_experiences[-1]
        state, action, existing_reward, next_state, done = player2_agent.replay_buffer.buffer[last_idx2]
        # Add final game outcome to any existing step rewards
        total_reward = existing_reward + final_reward2
        player2_agent.replay_buffer.buffer[last_idx2] = (state, action, total_reward, next_state, done)
    
    return winner, final_reward1, final_reward2, game_process


def inject_expert_demonstrations(agent, num_demonstrations=50):
    """
    Inject expert demonstration experiences to teach the agent that:
    - Destroying enemy castles is extremely valuable (+150)
    - Losing your castle is very bad (-75)
    
    This helps agents discover aggressive strategies faster.
    """
    print(f"Injecting {num_demonstrations} expert demonstrations...")
    
    # Create a realistic dummy observation (834 dimensional)
    dummy_obs = np.zeros(834, dtype=np.float32)
    dummy_next_obs = np.zeros(834, dtype=np.float32)
    
    for _ in range(num_demonstrations):
        # Random action representing an attack move
        demo_action = np.random.randint(449, 833)  # Move action range
        
        # High reward for castle destruction
        reward = 150.0
        done = False  # Game continues after destroying one castle
        
        agent.remember(dummy_obs, demo_action, reward, dummy_next_obs, done)
    
    # Also add some negative examples (losing castles)
    for _ in range(num_demonstrations // 3):
        demo_action = np.random.randint(449, 833)
        reward = -75.0  # Lost your castle
        done = False
        
        agent.remember(dummy_obs, demo_action, reward, dummy_next_obs, done)
    
    print(f"✓ Injected {len(agent.replay_buffer)} total expert experiences")


def balance_agents(agent1, agent2, win_rate_1):
    """Balance agents by copying stronger agent's weights to weaker agent."""
    print(f"\n🔄 Balancing agents (Player 1 win rate: {win_rate_1:.2%})")
    if win_rate_1 > 0.5:
        agent2.q_network.load_state_dict(agent1.q_network.state_dict())
        agent2.target_network.load_state_dict(agent1.target_network.state_dict())
        print("   Copied Agent 1 weights to Agent 2")
    else:
        agent1.q_network.load_state_dict(agent2.q_network.state_dict())
        agent1.target_network.load_state_dict(agent2.target_network.state_dict())
        print("   Copied Agent 2 weights to Agent 1")


def main():
    print("="*70)
    print("GPU-ACCELERATED SELF-PLAY TRAINING")
    print("="*70)
    print()
    
    # Check GPU availability
    if torch.cuda.is_available():
        print(f"✓ GPU detected: {torch.cuda.get_device_name(0)}")
        print(f"  CUDA version: {torch.version.cuda}")
        device = "cuda"
    else:
        print("⚠ No GPU detected, using CPU (will be slower)")
        device = "cpu"
    
    print()
    print(f"Training configuration:")
    print(f"  Games: {NUM_GAMES}")
    print(f"  Hidden layers: {HIDDEN_SIZES}")
    print(f"  Replay buffer: {REPLAY_BUFFER_SIZE}")
    print(f"  Batch size: {BATCH_SIZE}")
    print(f"  Device: {device}")
    print()
    
    # Create checkpoint directory
    os.makedirs(CHECKPOINT_DIR, exist_ok=True)
    
    # Initialize agents with larger architecture
    print("Initializing large agents...")
    agent1 = DQNAgent(
        state_size=OBS_SIZE,
        action_size=ACTION_SIZE,
        learning_rate=LEARNING_RATE,
        gamma=GAMMA,
        epsilon_start=1.0,
        epsilon_end=0.1,
        epsilon_decay=0.995,
        hidden_sizes=HIDDEN_SIZES,
        replay_buffer_size=REPLAY_BUFFER_SIZE
    )
    
    agent2 = DQNAgent(
        state_size=OBS_SIZE,
        action_size=ACTION_SIZE,
        learning_rate=LEARNING_RATE,
        gamma=GAMMA,
        epsilon_start=1.0,
        epsilon_end=0.1,
        epsilon_decay=0.995,
        hidden_sizes=HIDDEN_SIZES,
        replay_buffer_size=REPLAY_BUFFER_SIZE
    )
    
    # Count parameters
    total_params = sum(p.numel() for p in agent1.q_network.parameters())
    print(f"✓ Agents initialized")
    print(f"  Total parameters per agent: {total_params:,}")
    print()
    
    # Inject expert demonstrations to bootstrap learning
    inject_expert_demonstrations(agent1, num_demonstrations=100)
    inject_expert_demonstrations(agent2, num_demonstrations=100)
    print()
    
    # Start with no game process
    game_process = None
    
    try:
        # Create environment
        env = AntiyoyEnv()
        
        # Training loop
        print("Starting training...")
        print()
        
        win_history = []
        
        for game_num in range(NUM_GAMES):
            print(f"\n--- Starting game {game_num + 1}/{NUM_GAMES} ---")
            
            # Update epsilon
            current_epsilon = get_epsilon_for_game(game_num)
            agent1.epsilon = current_epsilon
            agent2.epsilon = current_epsilon
            
            # Position swapping
            if (game_num // POSITION_SWAP_INTERVAL) % 2 == 0:
                player1_agent, player2_agent = agent1, agent2
                p1_name, p2_name = "Agent1", "Agent2"
            else:
                player1_agent, player2_agent = agent2, agent1
                p1_name, p2_name = "Agent2", "Agent1"
            
            # Play game
            winner, reward1, reward2, game_process = play_selfplay_game(
                player1_agent, player2_agent, env, game_process
            )
            
            # Print statistics
            winner_name = "Agent1" if winner == 1 else "Agent2" if winner == 2 else "Draw"
            print(f"Game {game_num + 1} finished:")
            print(f"  Winner: {winner_name}")
            print(f"  {p1_name} reward: {reward1:+.1f} | {p2_name} reward: {reward2:+.1f}")
            print(f"  Replay buffer: A1={len(agent1.replay_buffer)} | A2={len(agent2.replay_buffer)}")
            
            # Track winner
            if winner == 1:
                agent_winner = 1 if p1_name == "Agent1" else 2
            elif winner == 2:
                agent_winner = 2 if p2_name == "Agent2" else 1
            else:
                agent_winner = 0
            
            win_history.append(agent_winner)
            
            # Train both agents with larger batches
            if len(agent1.replay_buffer) >= BATCH_SIZE:
                for _ in range(UPDATE_FREQUENCY):
                    agent1.train_step(BATCH_SIZE)
                    agent2.train_step(BATCH_SIZE)
            
            # Update target networks
            if (game_num + 1) % 10 == 0:
                agent1.update_target_network()
                agent2.update_target_network()
            
            # Balance if needed
            if len(win_history) >= WIN_RATE_WINDOW:
                recent_wins = win_history[-WIN_RATE_WINDOW:]
                agent1_wins = sum(1 for w in recent_wins if w == 1)
                win_rate_1 = agent1_wins / WIN_RATE_WINDOW
                
                if win_rate_1 > BALANCE_THRESHOLD or win_rate_1 < (1 - BALANCE_THRESHOLD):
                    balance_agents(agent1, agent2, win_rate_1)
                    win_history = []
            
            # Progress summary
            if (game_num + 1) % 10 == 0:
                if len(win_history) >= 10:
                    recent_10 = win_history[-10:]
                    a1_wins = sum(1 for w in recent_10 if w == 1)
                    a2_wins = sum(1 for w in recent_10 if w == 2)
                    draws = sum(1 for w in recent_10 if w == 0)
                    print(f"\n{'='*60}")
                    print(f"Progress: {game_num+1}/{NUM_GAMES} games")
                    print(f"Last 10 games: A1={a1_wins} wins | A2={a2_wins} wins | Draws={draws}")
                    print(f"Exploration: ε={current_epsilon:.3f}")
                    print(f"{'='*60}\n")
           
            # Checkpoint
            if (game_num + 1) % CHECKPOINT_INTERVAL == 0:
                checkpoint_path1 = f"{CHECKPOINT_DIR}/agent1_game_{game_num+1}.pth"
                checkpoint_path2 = f"{CHECKPOINT_DIR}/agent2_game_{game_num+1}.pth"
                agent1.save(checkpoint_path1)
                agent2.save(checkpoint_path2)
                print(f"✓ Checkpoint saved at game {game_num+1}")
        
        print()
        print("="*70)
        print("TRAINING COMPLETE")
        print("="*70)
        print()
        
        # Final statistics
        if len(win_history) > 0:
            a1_total = sum(1 for w in win_history if w == 1)
            a2_total = sum(1 for w in win_history if w == 2)
            draws = sum(1 for w in win_history if w == 0)
            print(f"Final Statistics:")
            print(f"  Agent 1 wins: {a1_total}")
            print(f"  Agent 2 wins: {a2_total}")
            print(f"  Draws: {draws}")
            print()
        
        # Save final models
        agent1.save(f"{CHECKPOINT_DIR}/agent1_final.pth")
        agent2.save(f"{CHECKPOINT_DIR}/agent2_final.pth")
        print("✓ Final models saved")
        
    finally:
        print("Stopping game process...")
        if game_process is not None:
            try:
                os.killpg(os.getpgid(game_process.pid), signal.SIGTERM)
            except:
                pass
        env.close()


if __name__ == "__main__":
    main()
