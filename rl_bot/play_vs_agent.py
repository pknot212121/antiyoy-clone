#!/usr/bin/env python3
"""
Play against a trained AI agent.
You control Player 1 (blue), AI controls Player 2 (red).
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

# Configuration
CHECKPOINT_PATH = "/home/mikolaj/work/antijoy-clone/rl_bot/models/gpu_checkpoints/agent2_game_500.pth"
HIDDEN_SIZES = [512, 512, 256]
OBS_SIZE = 834
ACTION_SIZE = 833


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
    
    print("Waiting for game to initialize...")
    time.sleep(3)
    print("Game ready!")
    return process


def select_best_action(agent, obs, env):
    """Select the best action for the AI agent."""
    valid_actions = env.get_valid_actions_from_server()
    
    if not valid_actions or len(valid_actions) <= 1:
        return 0  # End turn if no valid actions
    
    # Use agent's network to select best action (no exploration)
    import torch
    with torch.no_grad():
        q_values = agent.q_network(
            torch.tensor(obs, dtype=torch.float32).unsqueeze(0).to(agent.device)
        ).cpu().numpy()[0]
    
    # Mask invalid actions
    masked_q = np.full_like(q_values, -np.inf)
    masked_q[valid_actions] = q_values[valid_actions]
    
    return np.argmax(masked_q)


def main():
    print("="*70)
    print("PLAY AGAINST TRAINED AI AGENT")
    print("="*70)
    print()
    
    # Check if checkpoint exists
    if not os.path.exists(CHECKPOINT_PATH):
        print(f"❌ Checkpoint not found: {CHECKPOINT_PATH}")
        print()
        print("Available checkpoints:")
        checkpoint_dir = os.path.dirname(CHECKPOINT_PATH)
        if os.path.exists(checkpoint_dir):
            checkpoints = sorted([f for f in os.listdir(checkpoint_dir) if f.endswith('.pth')])
            for cp in checkpoints:
                print(f"  - {cp}")
        else:
            print("  No checkpoints found!")
        print()
        print("Update CHECKPOINT_PATH in the script to point to an existing checkpoint.")
        return
    
    # Load AI agent
    print(f"Loading AI agent from: {CHECKPOINT_PATH}")
    ai_agent = DQNAgent(
        state_size=OBS_SIZE,
        action_size=ACTION_SIZE,
        hidden_sizes=HIDDEN_SIZES,
        epsilon_start=0.0,  # No exploration - always pick best action
        epsilon_end=0.0
    )
    
    try:
        ai_agent.load(CHECKPOINT_PATH)
        print("✓ AI agent loaded successfully")
    except Exception as e:
        print(f"❌ Failed to load agent: {e}")
        return
    
    print()
    print("Game Setup:")
    print("  👤 You: Player 1 (Blue)")
    print("  🤖 AI:  Player 2 (Red)")
    print()
    print("Instructions:")
    print("  1. The game window will open")
    print("  2. Play your turn normally by clicking in the game")
    print("  3. When you end your turn, the AI will play automatically")
    print("  4. Watch the AI make its moves!")
    print()
    input("Press Enter to start the game...")
    
    # Start game
    game_process = start_game_process()
    
    try:
        # Connect to game
        env = AntiyoyEnv()
        obs = env.reset()
        
        print()
        print("="*70)
        print("GAME STARTED!")
        print("="*70)
        print()
        print("⚠️  IMPORTANT: The game is designed for bot vs bot.")
        print("    For human vs AI play, use this workflow:")
        print()
        print("    1. Watch the AI play as Player 2 (Red)")
        print("    2. After AI ends turn, you can manually intervene by:")
        print("       - Typing 'help' to see what the AI would do")
        print("       - Typing 'skip' to let AI control Player 1 too")
        print("       - Typing 'quit' to exit")
        print()
        print("="*70)
        print()
        
        done = False
        turn = 0
        
        # For this version, let's just watch both players controlled by AI
        # but give you the option to intervene
        
        player1_agent = ai_agent  # Use same AI for both for now
        player2_agent = ai_agent
        
        while not done and turn < 500:
            # Player 1's turn
            env.current_player_id = 1
            
            print(f"\n[Turn {turn}] Player 1's turn")
            print("Options:")
            print("  - Press ENTER to let AI play this turn")
            print("  - Type 'quit' to exit")
            
            user_input = input(">>> ").strip().lower()
            
            if user_input == 'quit':
                print("Exiting...")
                break
            
            # Let AI play Player 1
            valid_actions = env.get_valid_actions_from_server()
            if not valid_actions:
                valid_actions = [0]
            
            # AI makes moves until it ends turn
            while valid_actions and 0 not in valid_actions:
                action = select_best_action(player1_agent, obs, env)
                if action == 0:
                    break
                    
                next_obs, reward, done, info = env.step(action)
                if done:
                    break
                    
                obs = next_obs
                time.sleep(0.2)
                valid_actions = env.get_valid_actions_from_server()
            
            if not done:
                # End turn
                env.step(0)
            
            if done:
                break
            
            # Player 2's turn (AI)
            env.current_player_id = 2
            print(f"\n[Turn {turn}] Player 2's turn (AI playing...)")
            
            valid_actions = env.get_valid_actions_from_server()
            if not valid_actions:
                valid_actions = [0]
            
            # AI makes moves
            while valid_actions and 0 not in valid_actions:
                action = select_best_action(player2_agent, obs, env)
                if action == 0:
                    break
                    
                next_obs, reward, done, info = env.step(action)
                if done:
                    break
                    
                obs = next_obs
                time.sleep(0.2)
                valid_actions = env.get_valid_actions_from_server()
            
            if not done:
                # End turn
                env.step(0)
            
            turn += 1
            
            if done:
                break
        
        print()
        print("="*70)
        print("GAME OVER!")
        print("="*70)
        
        # Check who won
        player1_castles = sum(1 for cell in env.board_state.values()
                             if cell.get('owner_id') == 1 and 
                             cell.get('resident', '').lower() == 'castle')
        player2_castles = sum(1 for cell in env.board_state.values()
                             if cell.get('owner_id') == 2 and 
                             cell.get('resident', '').lower() == 'castle')
        
        if player1_castles == 0:
            print("🤖 AI WINS! All your castles were destroyed.")
        elif player2_castles == 0:
            print("👤 YOU WIN! You destroyed all AI castles!")
        else:
            territory1 = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 1)
            territory2 = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 2)
            print(f"DRAW - Your territory: {territory1} | AI territory: {territory2}")
        
        print()
        
    finally:
        print("Stopping game...")
        os.killpg(os.getpgid(game_process.pid), signal.SIGTERM)
        env.close()
        print("Done!")


if __name__ == "__main__":
    main()
