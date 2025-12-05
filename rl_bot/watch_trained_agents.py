#!/usr/bin/env python3
"""
Watch trained agents play against each other in the real game window.
Load trained models and play demo games with visualization.
"""

import os
import sys
import time
import subprocess
import signal
import numpy as np
import logging

# logging.basicConfig(
#     level=logging.DEBUG,
#     format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
# )

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent import DQNAgent
from antiyoy_env import AntiyoyEnv

# MUST MATCH train_quick.py and antiyoy_env.py!
OBS_SIZE = 834  # 8x8 board
ACTION_SIZE = 833  # 8x8 board

game_process = None


def start_game():
    """Start the Antiyoy game executable."""
    game_path = "/home/mikolaj/work/antijoy-clone/build/anti"
    config_path = "/home/mikolaj/work/antijoy-clone/Antiyoy/config.txt"
    work_dir = "/home/mikolaj/work/antijoy-clone"
    
    if not os.path.exists(game_path):
        print(f"ERROR: Game executable not found at {game_path}")
        return None
    
    if not os.path.exists(config_path):
        print(f"ERROR: Config file not found at {config_path}")
        return None
    
    print(f"Starting Antiyoy game with visual window...")
    
    process = subprocess.Popen(
        [game_path, config_path],
        cwd=work_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        preexec_fn=os.setsid
    )
    
    print("Waiting for game to initialize...")
    time.sleep(3)
    
    if process.poll() is not None:
        print("ERROR: Game failed to start")
        return None
    
    print("✓ Game started - you should see the game window now!")
    return process


def stop_game(process):
    """Stop the game process."""
    if process:
        print("\nStopping game...")
        os.killpg(os.getpgid(process.pid), signal.SIGTERM)
        time.sleep(1)


def select_best_action(agent, obs, env):
    """Select best action (no exploration) from valid actions."""
    valid_actions = env.get_valid_actions_from_server()
    
    if len(valid_actions) <= 1:
        return 0
    
    import torch
    with torch.no_grad():
        q_values = agent.q_network(
            torch.tensor(obs, dtype=torch.float32).unsqueeze(0).to(agent.device)
        ).cpu().numpy()[0]
    

    
    # Mask invalid actions
    masked_q = np.full_like(q_values, -np.inf)
    masked_q[valid_actions] = q_values[valid_actions]
    
    return np.argmax(masked_q)


def watch_agents_play(env, agent1, agent2, num_games=3):
    """Watch agents play against each other."""
    print(f"\nPlaying {num_games} demo games...\n")
    
    for game in range(num_games):
        print(f"\n{'='*70}")
        print(f"DEMO GAME {game+1}")
        print(f"{'='*70}\n")
        
        env.reset()
        
        print("Board initialized:")
        print(f"  Player 1 territory: {env._count_territory(1)} hexes")
        print(f"  Player 2 territory: {env._count_territory(2)} hexes")
        print(f"  Player 1 money: ${env.player_money.get(1, 0)}")
        print(f"  Player 2 money: ${env.player_money.get(2, 0)}")
        print("\nWatch the game window on your screen!")
        print("Agents will make moves every 0.5 seconds.\n")
        
        done = False
        turn = 0
        max_turns = 100
        
        while turn < max_turns:
            # Player 1
            env.current_player_id = 1
            
            # SYNC BOARD BEFORE MOVE
            if env.connected:
                try:
                    import socket as sock_module
                    env.client.sock.settimeout(0.1)
                    while True:
                        tag_byte = env.client.sock.recv(1, sock_module.MSG_PEEK)
                        if tag_byte and tag_byte[0] == 2:  # BOARD tag
                            updated_board = env.client.receive_board_state()
                            if updated_board:
                                env.board_state = updated_board
                                # print("✓ Board synced")
                        else:
                            break
                except:
                    pass
            
            state = env._encode_observation()
            action = select_best_action(agent1, state, env)
            
            # Print action
            action_type, x, y, param = env._decode_action(action)
            print(f"[Turn {turn:2d}] Player 1 ({action:3d}): {action_type:10s} at ({x:2d},{y:2d})")
            
            next_state, reward, done, info = env.step(action)
            
            if done:
                print(f"Game Over! Reward: {reward}")
                break
                
            time.sleep(0.5)
            
            # Player 2
            env.current_player_id = 2
            
            # SYNC BOARD BEFORE MOVE
            if env.connected:
                try:
                    import socket as sock_module
                    env.client.sock.settimeout(0.1)
                    while True:
                        tag_byte = env.client.sock.recv(1, sock_module.MSG_PEEK)
                        if tag_byte and tag_byte[0] == 2:
                            updated_board = env.client.receive_board_state()
                            if updated_board:
                                env.board_state = updated_board
                        else:
                            break
                except:
                    pass

            state = env._encode_observation()
            action = select_best_action(agent2, state, env)
            
            # Print action
            action_type, x, y, param = env._decode_action(action)
            print(f"[Turn {turn:2d}] Player 2 ({action:3d}): {action_type:10s} at ({x:2d},{y:2d})")
            
            next_state, reward, done, info = env.step(action)
            
            if done:
                print(f"Game Over! Reward: {reward}")
                break
                
            time.sleep(0.5)
            turn += 1
        
        print(f"\nGame {game + 1} finished after {turn} turns")
        if game < num_games - 1:
            print("\nStarting next game in 3 seconds...")
            time.sleep(3)


def main():
    """Watch trained agents play demo games."""
    global game_process
    
    print("="*70)
    print("WATCH TRAINED AGENTS PLAY")
    print("="*70)
    print()
    
    # Load trained models
    model1_path = "/home/mikolaj/work/antijoy-clone/rl_bot/models/gpu_checkpoints/agent1_game_500.pth"
    model2_path = "/home/mikolaj/work/antijoy-clone/rl_bot/models/gpu_checkpoints/agent2_game_500.pth"
    
    if not os.path.exists(model1_path) or not os.path.exists(model2_path):
        print("ERROR: Trained models not found!")
        print("Please train agents first:")
        print("  python3 train_1000_games.py")
        print()
        print("Or use checkpoint models:")
        print(f"  Agent 1: {model1_path}")
        print(f"  Agent 2: {model2_path}")
        return
    
    print("Loading trained agents...")
    agent1 = DQNAgent(state_size=OBS_SIZE, action_size=ACTION_SIZE)
    agent1.load(model1_path)
    agent1.epsilon = 0.0  # No exploration for demo
    
    agent2 = DQNAgent(state_size=OBS_SIZE, action_size=ACTION_SIZE)
    agent2.load(model2_path)
    agent2.epsilon = 0.0
    
    print("✓ Agents loaded")
    print()
    
    # Start game
    game_process = start_game()
    if not game_process:
        return
    
    try:
        # Connect to game
        print("\nConnecting to game...")
        env = AntiyoyEnv(host="127.0.0.1", port=2137)
        print("✓ Connected")
        print()
        
        # Play 3 demo games
        watch_agents_play(env, agent1, agent2, num_games=3)
        
        print("\n" + "="*70)
        print("DEMO COMPLETE")
        print("="*70)
        print("\nYou've watched your trained RL agents play!")
        
        env.close()
        
    except KeyboardInterrupt:
        print("\n\nDemo interrupted by user")
    except Exception as e:
        print(f"\n\nError during demo: {e}")
        import traceback
        traceback.print_exc()
    finally:
        stop_game(game_process)


if __name__ == "__main__":
    main()
