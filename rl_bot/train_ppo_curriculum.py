#!/usr/bin/env python3
"""
PPO Training with Curriculum Learning for Antiyoy
Stages: 5v0 → 4v1 → 3v1 to achieve 80%+ win rate
"""

import os
import sys
import time
import numpy as np
import subprocess
import signal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from ppo_agent import PPOAgent
from antiyoy_env import AntiyoyEnv

# Curriculum stages
STAGES = [
    {"name": "Easy", "agent_warriors": 5, "enemy_warriors": 0, "target_wr": 95, "games": 100},
    {"name": "Medium", "agent_warriors": 4, "enemy_warriors": 1, "target_wr": 85, "games": 200},
    {"name": "Hard", "agent_warriors": 3, "enemy_warriors": 1, "target_wr": 80, "games": 300},
]

CHECKPOINT_DIR = "/home/mikolaj/work/antijoy-clone/rl_bot/models/ppo_curriculum"
ROLLOUT_LENGTH = 20  # Collect 20 steps before update
MAX_TURN_LIMIT = 25


def start_game_process(visible=False):
    game_path = "/home/mikolaj/work/antijoy-clone/build/anti"
    config_path = "/home/mikolaj/work/antijoy-clone/Antiyoy/config_ppo.txt"
    
    if visible:
        process = subprocess.Popen([game_path, config_path], preexec_fn=os.setsid)
    else:
        process = subprocess.Popen(
            ["xvfb-run", "-a", "-s", "-screen 0 800x600x24", game_path, config_path],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, preexec_fn=os.setsid
        )
    time.sleep(3)
    return process


def filter_only_moves(valid_actions, board_width, board_height):
    """Only move actions."""
    board_size = board_width * board_height
    place_end = board_size * 7
    
    moves = [a for a in valid_actions if a > place_end]
    return moves if moves else [0]


def create_config(seed=200):
    """Create 8x8 config."""
    config_path = "/home/mikolaj/work/antijoy-clone/Antiyoy/config_ppo.txt"
    
    content = f"""8 8
{seed}
6 8
BB
-1 -1
2137
receiver 127.0.0.1
2138
"""
    
    with open(config_path, 'w') as f:
        f.write(content)


def modify_warrior_counts(agent_count, enemy_count):
    """
    Modify board.cpp to spawn specific warrior counts.
    NOTE: Requires recompilation!
    """
    # board.cpp is in project root
    board_file = "/home/mikolaj/work/antijoy-clone/board.cpp"
    
    # Read current content
    with open(board_file, 'r') as f:
        content = f.read()
    
    # Use regex to find the warrior count line (matches any current values)
    import re
    pattern = r'int targetWarriors = \(playerId == 1\) \? \d+ : \d+;'
    new_line = f'int targetWarriors = (playerId == 1) ? {agent_count} : {enemy_count};'
    
    if not re.search(pattern, content):
        print(f"WARNING: Could not find warrior count line to modify!")
        print(f"Looking for pattern: {pattern}")
        return False
    
    content = re.sub(pattern, new_line, content)
    
    with open(board_file, 'w') as f:
        f.write(content)
    
    print(f"✓ Modified warrior counts: {agent_count}v{enemy_count}")
    return True


def recompile_game():
    """Recompile the game after modifying warrior counts."""
    print("Recompiling game...")
    result = subprocess.run(
        ["make"],
        cwd="/home/mikolaj/work/antijoy-clone/build",
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        print(f"Compilation failed: {result.stderr}")
        return False
    
    print("✓ Recompilation successful")
    return True


def play_ppo_game(agent, env, game_process, game_num, stage_name):
    """Play one game with PPO agent."""
    # Cleanup
    if game_process is not None:
        try:
            os.killpg(os.getpgid(game_process.pid), signal.SIGTERM)
            time.sleep(1)
            try:
                os.killpg(os.getpgid(game_process.pid), signal.SIGKILL)
            except:
                pass
        except:
            pass
        if env.connected:
            env.client.close()
            env.connected = False
            time.sleep(0.3)
    
    create_config(seed=200 + game_num)  # Vary map slightly
    
    show_window = (game_num % 20 == 0)
    game_process = start_game_process(visible=show_window)
    
    obs = env.reset(force_new_game=False)
    time.sleep(0.2)
    
    done = False
    turn = 0
    total_reward = 0
    steps_in_episode = 0
    
    while not done and turn < MAX_TURN_LIMIT:
        # AGENT TURN
        env.current_player_id = 1
        valid = env.get_valid_actions_from_server()
        valid = filter_only_moves(valid, env.board_width, env.board_height)
        
        action, log_prob, value = agent.select_action(obs, valid_actions=valid)
        
        # Ensure action is valid
        if action not in valid:
            action = np.random.choice(valid)
        
        next_obs, _, done, _ = env.step(action)
        
        # Dense reward shaping
        p1_terr = sum(1 for c in env.board_state.values() if c.get('owner_id') == 1)
        reward = p1_terr * 0.5  # Small per-step reward
        total_reward += reward
        
        # Store transition
        agent.store_transition(obs, action, reward, value, log_prob, done)
        steps_in_episode += 1
        
        # PPO update every ROLLOUT_LENGTH steps
        if steps_in_episode >= ROLLOUT_LENGTH and not done:
            agent.update(next_obs)
            steps_in_episode = 0
        
        if done:
            break
        obs = next_obs
        
        # RANDOM TURN
        env.current_player_id = 2
        valid = env.get_valid_actions_from_server()
        valid = filter_only_moves(valid, env.board_width, env.board_height)
        
        random_action = np.random.choice(valid)
        next_obs, _, done, _ = env.step(random_action)
        
        if done:
            break
        obs = next_obs
        turn += 1
    
    # Final reward
    p2_castles = sum(1 for c in env.board_state.values()
                     if c.get('owner_id') == 2 and c.get('resident', '').lower() == 'castle')
    
    if p2_castles == 0:
        winner = "WIN"
        final_reward = +1000.0
    else:
        winner = "DRAW"
        final_reward = -200.0
    
    # Add final reward  to last transition
    if len(agent.buffer) > 0:
        last_reward_idx = len(agent.buffer.rewards) - 1
        agent.buffer.rewards[last_reward_idx] += final_reward
    
    total_reward += final_reward
    
    # Final update
    if len(agent.buffer) > 0:
        agent.update(next_obs)
    
    print(f"[{stage_name}] {winner:4s} (T{turn:2d}) R={total_reward:+6.0f}")
    
    return winner, game_process


def train_curriculum():
    """Train PPO with curriculum learning."""
    print("="*70)
    print("PPO CURRICULUM TRAINING")
    print("="*70)
    
    os.makedirs(CHECKPOINT_DIR, exist_ok=True)
    
    env = AntiyoyEnv()
    OBS_SIZE = env.observation_space.shape[0]
    ACTION_SIZE = env.action_space.n
    
    print(f"Obs: {OBS_SIZE}, Actions: {ACTION_SIZE}\n")
    
    # Create PPO agent
    agent = PPOAgent(
        state_size=OBS_SIZE,
        action_size=ACTION_SIZE,
        lr=3e-4,
        gamma=0.99,
        gae_lambda=0.95,
        clip_epsilon=0.2,
        epochs=4,
        minibatch_size=64,
        hidden_sizes=[256, 256]
    )
    
    print(f"PPO Agent: {sum(p.numel() for p in agent.ac_network.parameters()):,} params\n")
    
    # Train each curriculum stage
    for stage_idx, stage in enumerate(STAGES):
        print(f"\n{'='*70}")
        print(f"STAGE {stage_idx+1}: {stage['name']} - {stage['agent_warriors']}v{stage['enemy_warriors']}")
        print(f"Target: {stage['target_wr']}% win rate over {stage['games']} games")
        print(f"{'='*70}\n")
        
        # Modify C++ and recompile
        if not modify_warrior_counts(stage['agent_warriors'], stage['enemy_warriors']):
            print("Failed to modify warrior counts!")
            return
        
        if not recompile_game():
            print("Failed to recompile game!")
            return
        
        # Train this stage
        game_process = None
        wins = 0
        recent_wins = deque(maxlen=50)  # Track last 50 games
        
        for game_num in range(stage['games']):
            winner, game_process = play_ppo_game(
                agent, env, game_process, game_num, stage['name']
            )
            
            is_win = (winner == "WIN")
            wins += is_win
            recent_wins.append(is_win)
            
            # Progress update
            if (game_num + 1) % 20 == 0:
                recent_wr = sum(recent_wins) / len(recent_wins) * 100 if recent_wins else 0
                overall_wr = wins / (game_num + 1) * 100
                print(f"  Progress: {game_num+1}/{stage['games']} | Overall WR: {overall_wr:.1f}% | Recent WR: {recent_wr:.1f}%")
                
                # Check if stage complete
                if len(recent_wins) == 50 and recent_wr >= stage['target_wr']:
                    print(f"\n✅ STAGE {stage_idx+1} COMPLETE! Recent WR: {recent_wr:.1f}% >= {stage['target_wr']}%")
                    agent.save(f"{CHECKPOINT_DIR}/stage_{stage_idx+1}_complete.pth")
                    break
        
        # Final stage stats
        final_wr = wins / (game_num + 1) * 100
        print(f"\nStage {stage_idx+1} Final: {wins}/{game_num+1} = {final_wr:.1f}% win rate")
        
        if final_wr < stage['target_wr']:
            print(f"⚠️ Did not reach target {stage['target_wr']}%, but continuing...")
        
        agent.save(f"{CHECKPOINT_DIR}/stage_{stage_idx+1}_final.pth")
        
        # Cleanup
        if game_process:
            try:
                os.killpg(os.getpgid(game_process.pid), signal.SIGTERM)
            except:
                pass
    
    print(f"\n{'='*70}")
    print("CURRICULUM TRAINING COMPLETE!")
    print(f"{'='*70}")
    env.close()


if __name__ == "__main__":
    # Import deque
    from collections import deque
    train_curriculum()
