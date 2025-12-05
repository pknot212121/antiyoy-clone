#!/usr/bin/env python3
"""
Long-term self-play training with balancing mechanisms.
Trains two agents over 1000+ games with position swapping and opponent balancing.
"""

import os
import sys
import time
import numpy as np
import subprocess
import signal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent import DQNAgent
from antiyoy_env import AntiyoyEnv

# Training configuration
NUM_GAMES = 1000
LEARNING_RATE = 1e-4
GAMMA = 0.99
BATCH_SIZE = 32
UPDATE_FREQUENCY = 4

# Progressive epsilon schedule
EPSILON_SCHEDULE = {
    0: 1.0,      # Games 0-200: high exploration
    200: 0.3,    # Games 200-500: medium exploration
    500: 0.15,   # Games 500-1000: low exploration
    1000: 0.1    # Games 1000+: minimal exploration
}

# Balancing configuration
POSITION_SWAP_INTERVAL = 10  # Swap positions every N games
WIN_RATE_WINDOW = 50         # Calculate win rate over last N games
BALANCE_THRESHOLD = 0.70     # Balance if win rate exceeds this

# Checkpointing
CHECKPOINT_INTERVAL = 100
CHECKPOINT_DIR = "/home/mikolaj/work/antijoy-clone/rl_bot/models/selfplay_checkpoints"

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
    time.sleep(5)  # Give game more time to start up fully
    print("Game server should be ready")
    return process


def play_selfplay_game(player1_agent, player2_agent, env, game_process):
    """
    Play one self-play game.
    Returns (winner, reward1, reward2, new_game_process) where winner is 1 or 2.
    The game process is restarted for each game.
    """
    # Kill old game process if it exists
    if game_process is not None:
        try:
            os.killpg(os.getpgid(game_process.pid), signal.SIGTERM)
            time.sleep(1.5)  # Longer delay to ensure port is released
        except:
            pass
        
        # Close old socket connection and reset environment state
        if env.connected:
            env.client.close()
            env.connected = False
            time.sleep(0.5)  # Extra delay after closing socket
    
    # Start fresh game process
    game_process = start_game_process()
    
    # Reset environment - will create fresh connection
    obs = env.reset(force_new_game=False)
    
    # Small delay to let server stabilize
    time.sleep(0.2)
    
    agent1_rewards = []
    agent2_rewards = []
    
    done = False
    turn = 0
    max_turns = 200  # Prevent infinite games
    
    while not done and turn < max_turns:
        # Player 1's turn
        env.current_player_id = 1
        
        # Get valid actions with retry
        valid_actions = []
        for attempt in range(3):
            valid_actions = env.get_valid_actions_from_server()
            if valid_actions:
                break
            time.sleep(0.1)
        
        if not valid_actions:
            valid_actions = [0]  # Fallback to end turn
        
        action1 = player1_agent.select_action(obs, training=True)
        if action1 not in valid_actions:
            action1 = np.random.choice(valid_actions)
        
        next_obs, reward1, done, _ = env.step(action1)
        player1_agent.remember(obs, action1, 0.0, next_obs, done)
        agent1_rewards.append(reward1)
        
        if done:
            break
        obs = next_obs
        
        # Player 2's turn
        env.current_player_id = 2
        
        # Get valid actions with retry
        valid_actions = []
        for attempt in range(3):
            valid_actions = env.get_valid_actions_from_server()
            if valid_actions:
                break
            time.sleep(0.1)
        
        if not valid_actions:
            valid_actions = [0]  # Fallback to end turn
        
        action2 = player2_agent.select_action(obs, training=True)
        if action2 not in valid_actions:
            action2 = np.random.choice(valid_actions)
        
        next_obs, reward2, done, _ = env.step(action2)
        player2_agent.remember(obs, action2, 0.0, next_obs, done)
        agent2_rewards.append(reward2)
        
        if done:
            break
        obs = next_obs
        turn += 1
    
    # Determine winner and assign final rewards
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
        # Draw - compare territory but give small rewards
        # Encourage decisive play, not just territory hoarding
        territory1 = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 1)
        territory2 = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 2)
        if territory1 > territory2:
            final_reward1, final_reward2 = +10.0, -10.0  # Small bonus for territory lead
            winner = 1
        elif territory2 > territory1:
            final_reward1, final_reward2 = -10.0, +10.0
            winner = 2
        else:
            final_reward1, final_reward2 = -20.0, -20.0  # Penalty for complete stalemate
            winner = 0
    
    return winner, final_reward1, final_reward2, game_process


def balance_agents(agent1, agent2, win_rate_1):
    """Balance agents by copying stronger agent's weights to weaker agent."""
    print(f"\n🔄 Balancing agents (Player 1 win rate: {win_rate_1:.2%})")
    if win_rate_1 > 0.5:
        # Agent 1 is stronger, copy to Agent 2
        agent2.q_network.load_state_dict(agent1.q_network.state_dict())
        agent2.target_network.load_state_dict(agent1.target_network.state_dict())
        print("   Copied Agent 1 weights to Agent 2")
    else:
        # Agent 2 is stronger, copy to Agent 1
        agent1.q_network.load_state_dict(agent2.q_network.state_dict())
        agent1.target_network.load_state_dict(agent2.target_network.state_dict())
        print("   Copied Agent 2 weights to Agent 1")


def main():
    print("="*70)
    print("LONG-TERM SELF-PLAY TRAINING")
    print("="*70)
    print()
    print(f"Training for {NUM_GAMES} games with balanced self-play")
    print(f"Position swapping every {POSITION_SWAP_INTERVAL} games")
    print(f"Balancing if win rate exceeds {BALANCE_THRESHOLD:.0%}")
    print()
    
    # Create checkpoint directory
    os.makedirs(CHECKPOINT_DIR, exist_ok=True)
    
    # Initialize agents
    print("Initializing agents...")
    agent1 = DQNAgent(
        state_size=OBS_SIZE,
        action_size=ACTION_SIZE,
        learning_rate=LEARNING_RATE,
        gamma=GAMMA,
        epsilon_start=1.0,
        epsilon_end=0.1,
        epsilon_decay=0.995
    )
    
    agent2 = DQNAgent(
        state_size=OBS_SIZE,
        action_size=ACTION_SIZE,
        learning_rate=LEARNING_RATE,
        gamma=GAMMA,
        epsilon_start=1.0,
        epsilon_end=0.1,
        epsilon_decay=0.995
    )
    print("✓ Agents initialized")
    print()
    
    # Start with no game process (will be created in first game)
    game_process = None
    
    try:
        # Create environment
        env = AntiyoyEnv()
        
        # Training loop
        print("Starting training...")
        print()
        
        win_history = []  # Track winners for balance calculation
        
        for game_num in range(NUM_GAMES):
            print(f"\n--- Starting game {game_num + 1}/{NUM_GAMES} ---")
            
            # Update epsilon based on schedule
            current_epsilon = get_epsilon_for_game(game_num)
            agent1.epsilon = current_epsilon
            agent2.epsilon = current_epsilon
            
            # Position swapping: alternate which agent plays as Player 1
            if (game_num // POSITION_SWAP_INTERVAL) % 2 == 0:
                player1_agent, player2_agent = agent1, agent2
                p1_name, p2_name = "Agent1", "Agent2"
            else:
                player1_agent, player2_agent = agent2, agent1
                p1_name, p2_name = "Agent2", "Agent1"
            
            # Play game and get new game process
            winner, reward1, reward2, game_process = play_selfplay_game(
                player1_agent, player2_agent, env, game_process
            )
            
            # Print game statistics
            winner_name = "Agent1" if winner == 1 else "Agent2" if winner == 2 else "Draw"
            print(f"Game {game_num + 1} finished:")
            print(f"  Winner: {winner_name}")
            print(f"  {p1_name} reward: {reward1:+.1f} | {p2_name} reward: {reward2:+.1f}")
            print(f"  Replay buffer: A1={len(agent1.replay_buffer)} | A2={len(agent2.replay_buffer)}")
            
            # Track winner (convert to agent ID)
            if winner == 1:
                agent_winner = 1 if p1_name == "Agent1" else 2
            elif winner == 2:
                agent_winner = 2 if p2_name == "Agent2" else 1
            else:
                agent_winner = 0
            
            win_history.append(agent_winner)
            
            # Train both agents
            if len(agent1.replay_buffer) >= BATCH_SIZE:
                for _ in range(UPDATE_FREQUENCY):
                    agent1.train_step(BATCH_SIZE)
                    agent2.train_step(BATCH_SIZE)
            
            # Update target networks periodically
            if (game_num + 1) % 10 == 0:
                agent1.update_target_network()
                agent2.update_target_network()
            
            # Calculate win rate and balance if needed
            if len(win_history) >= WIN_RATE_WINDOW:
                recent_wins = win_history[-WIN_RATE_WINDOW:]
                agent1_wins = sum(1 for w in recent_wins if w == 1)
                win_rate_1 = agent1_wins / WIN_RATE_WINDOW
                
                if win_rate_1 > BALANCE_THRESHOLD or win_rate_1 < (1 - BALANCE_THRESHOLD):
                    balance_agents(agent1, agent2, win_rate_1)
                    win_history = []  # Reset history after balancing
            
            # Print progress summary
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
