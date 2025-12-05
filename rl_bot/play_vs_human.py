#!/usr/bin/env python3
"""
Play as Human (Player 1) against trained AI (Player 2).
You control via GUI, AI responds via socket.
"""

import os
import sys
import time
import socket
import numpy as np
import subprocess
import signal
import torch

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
    print("PLAY AS HUMAN (Player 1) VS TRAINED AI (Player 2)")
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
    
    # Load AI agent for Player 2
    print(f"Loading AI agent (Player 2) from: {CHECKPOINT_PATH}")
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
    print("  👤 You: Player 1 (Blue) - GUI control")
    print("  🤖 AI:  Player 2 (Red)  - Socket control")
    print()
    print("Instructions:")
    print("  1. The game window will open")
    print("  2. You play normally using mouse clicks")
    print("  3. Click your units, make moves, end your turn")
    print("  4. AI will automatically play Player 2's turn")
    print("  5. Repeat until someone wins!")
    print()
    input("Press Enter to start the game...")
    
    # Start game
    game_process = start_game_process()
    
    try:
        # Connect to game as Player 2 (bot)
        env = AntiyoyEnv()
        obs = env.reset()
        
        print()
        print("="*70)
        print("GAME STARTED!")
        print("="*70)
        print()
        print("Make your first move in the game window (you're Player 1/Blue)")
        print()
        
        def wait_for_player2_turn(env):
            """
            In hybrid mode (LB), detect when Player 1's turn ends.
            The game may not send explicit TURN_CHANGE, so we detect  patterns.
            """
            import struct
            
            print("  Consuming Player 1's actions...")
            actions_seen = 0
            last_action_time = time.time()
            
            max_attempts = 100
            for attempt in range(max_attempts):
                try:
                    if not env.client.sock:
                        return False
                    
                    env.client.sock.settimeout(0.3)
                    
                    try:
                        # Try to receive 1 byte (the tag)
                        tag_byte = env.client.sock.recv(1, socket.MSG_PEEK)
                        if not tag_byte:
                            # No more data - check if we waited long enough
                            if actions_seen > 0 and (time.time() - last_action_time) > 1.0:
                                print(f"  Consumed {actions_seen} actions, assuming turn ended")
                                return True
                            time.sleep(0.2)
                            continue
                        
                        tag = tag_byte[0]
                        
                        # Handle different message types
                        if tag == 0x03:  # ACTION from Player 1
                            # Consume the action message
                            actual_tag = env.client.sock.recv(1)  # Consume tag
                            if actual_tag[0] != 0x03:
                                continue
                            
                            # Read action type (1 byte)
                            action_type_byte = env.client.sock.recv(1) 
                            if not action_type_byte:
                                continue
                            
                            action_type = action_type_byte[0]
                            
                            # Consume action data based on type
                            if action_type == 0:  # End turn!
                                print(f"  Player 1 ended turn (consumed {actions_seen} actions)")
                                return True
                            elif action_type == 1:  # Place unit
                                env.client.sock.recv(9)  # Resident, FromX, FromY, ToX, ToY
                                actions_seen += 1
                                last_action_time = time.time()
                            elif action_type == 2:  # Move unit  
                                env.client.sock.recv(8)  # FromX, FromY, ToX, ToY
                                actions_seen += 1
                                last_action_time = time.time()
                            
                            continue
                        
                        elif tag == 0x04:  # CONFIRMATION
                            # Consume confirmation
                            env.client.sock.recv(1)  # tag
                            env.client.sock.recv(2)  # 2 booleans
                            continue
                        
                        elif tag == 0x02:  # BOARD update
                            # Consume board
                            env.client.sock.recv(1)  # tag
                            board_size = env.board_width * env.board_height
                            env.client.sock.recv(board_size * 2)
                            continue
                        
                        elif tag == 0x05:  # TURN_CHANGE (if it exists)
                            env.client.sock.recv(1)  # tag
                            player_id_byte = env.client.sock.recv(1)
                            if player_id_byte and player_id_byte[0] == 2:
                                print(f"  Explicit turn change to Player 2")
                                return True
                            continue
                        
                        else:
                            # Unknown tag, consume 1 byte and continue
                            env.client.sock.recv(1)
                            continue
                    
                    except socket.timeout:
                        # Timeout - check if enough time passed since last action
                        if actions_seen > 0 and (time.time() - last_action_time) > 1.5:
                            print(f"  Timeout after {actions_seen} actions, assuming turn ended")
                            return True
                        continue
                    
                except Exception as e:
                    print(f"  Error: {e}")
                    time.sleep(0.2)
                    continue
                
            print(f"  Max attempts reached")
            return False
        
        done = False
        turn = 0
        
        while not done and turn < 1000:
            env.current_player_id = 2
            
            # Wait for Player 2's turn using proper protocol handling
            print(f"\n👤 Waiting for you to end your turn...")
            
            if not wait_for_player2_turn(env):
                print("Failed to detect Player 2's turn - game may be over")
                break
            
            print(f"[Turn {turn}] 🤖 AI's turn now!")
            
            # Now request valid actions for Player 2
            try:
                valid_actions = env.get_valid_actions_from_server()
            except Exception as e:
                print(f"Error getting valid actions: {e}")
                break
            
            if not valid_actions:
                print("No valid actions for AI - game may be over")
                break
            
            print(f"[Turn {turn}] 🤖 AI is playing...")
            
            # AI makes moves until it wants to end turn
            moves_made = 0
            action = 0  # Initialize to end turn by default
            
            while valid_actions and len(valid_actions) > 1 and 0 not in valid_actions:
                # AI selects best action
                action = select_best_action(ai_agent, obs, env)
                
                # Execute action
                next_obs, reward, done, info = env.step(action)
                moves_made += 1
                
                if done:
                    break
                
                # Small delay so you can see AI's moves
                time.sleep(0.2)
                
                # Check if AI wants to end turn
                if action == 0:
                    break
                
                # Get next valid actions
                try:
                    valid_actions = env.get_valid_actions_from_server()
                except:
                    # Socket error, end turn
                    break
                    
                obs = next_obs
            
            # End AI turn if not already ended
            if not done and action != 0:
                try:
                    env.step(0)
                except:
                    pass  # Ignore errors on ending turn
            
            print(f"          AI made {moves_made} moves")
            print(f"\n👤 Your turn! Play in the game window...")
            print()
            
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
            if territory1 > territory2:
                print(f"👤 YOU WIN by territory! ({territory1} vs {territory2})")
            elif territory2 > territory1:
                print(f"🤖 AI WINS by territory! ({territory2} vs {territory1})")
            else:
                print(f"DRAW - Territory: {territory1} each")
        
        print()
        
    except KeyboardInterrupt:
        print("\n\nGame interrupted by user")
    finally:
        print("Stopping game...")
        try:
            os.killpg(os.getpgid(game_process.pid), signal.SIGTERM)
        except:
            pass
        env.close()
        print("Done!")


if __name__ == "__main__":
    main()
