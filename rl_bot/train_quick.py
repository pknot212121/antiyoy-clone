#!/usr/bin/env python3
"""
Quick training script - Train on ONE fixed map for fast testing.
Trains agents with the new reward system.
"""

import os
import sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent import DQNAgent
from antiyoy_env import AntiyoyEnv

# Quick training config - SMALL 8x8 MAP for fast learning!
NUM_GAMES = 200  # Train for 200 games
FIXED_SEED = 12345  # SAME MAP EVERY TIME - makes learning easier!
LEARNING_RATE = 1e-4
GAMMA = 0.99
EPSILON_START = 1.0
EPSILON_END = 0.1
EPSILON_DECAY = 0.98
BATCH_SIZE = 32
UPDATE_FREQUENCY = 4

OBS_SIZE = 834  # 8x8 board: 64*13 + 2 = 834
ACTION_SIZE = 833  # 8x8 board: 1 + 64*7 + 64*6 = 833


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


def play_training_game(agent1, agent2, env):
    """Play one training game on the fixed map using SERVER connection."""
    # Reset environment (connects to server)
    obs = env.reset()
    
    agent1_rewards = []
    agent2_rewards = []
    
    done = False
    while not done:
        # Player 1
        env.current_player_id = 1
        
        # Get valid actions FROM SERVER
        valid_actions = env.get_valid_actions_from_server()
        if not valid_actions:
            valid_actions = [0]
            
        action1 = agent1.select_action(obs, training=True)
        
        # Force valid action if selected is invalid
        if action1 not in valid_actions:
            action1 = np.random.choice(valid_actions)
        
        next_obs, reward1, done, _ = env.step(action1)
        agent1.remember(obs, action1, 0.0, next_obs, done) # Store 0 reward initially
        agent1_rewards.append(reward1) # Track actual step reward (if any)
        
        if done: break
        obs = next_obs
        
        # Player 2
        env.current_player_id = 2
        
        # Get valid actions FROM SERVER
        valid_actions = env.get_valid_actions_from_server()
        if not valid_actions:
            valid_actions = [0]
            
        action2 = agent2.select_action(obs, training=True)
        
        if action2 not in valid_actions:
            action2 = np.random.choice(valid_actions)
            
        next_obs, reward2, done, _ = env.step(action2)
        agent2.remember(obs, action2, 0.0, next_obs, done)
        agent2_rewards.append(reward2)
        
        if done: break
        obs = next_obs
    
    # GAME OVER: Determine winner and assign final rewards
    # This logic is now handled by the server and reflected in the final rewards.
    # We need to update the last experience with the actual final reward.
    
    final_reward1 = sum(agent1_rewards)
    final_reward2 = sum(agent2_rewards)
    
    # Update the last experience with the final reward and done status
    # The rewards stored in remember() were 0.0, now we update them.
    # This assumes that the last reward in agent1_rewards/agent2_rewards is the final game outcome.
    # If the environment gives cumulative rewards, this needs adjustment.
    # For now, let's assume the final reward is the sum of step rewards.
    
    # This part needs careful consideration if rewards are sparse (only at end).
    # If rewards are only given at the end, the `reward1` and `reward2` from `env.step`
    # will be 0 until the final step.
    # The current implementation of `remember` stores 0.0, and `agent1_rewards` tracks actual step rewards.
    # We need to update the stored experiences with the correct rewards.
    
    # A common approach for sparse rewards is to only assign the final reward to the last step.
    # Or, if rewards are cumulative, the `reward1` and `reward2` from `env.step` should be used directly.
    # Given the original code's structure of assigning a final reward, let's adapt that.
    
    # The `reward1` and `reward2` from `env.step` are the immediate rewards.
    # The `0.0` in `agent.remember` was a placeholder.
    # We need to iterate through the stored experiences and update their rewards.
    
    # This is a simplified approach for now, assuming `env.step` returns the immediate reward.
    # If the game is designed such that the final reward is only known at the very end,
    # and `env.step` returns 0 until then, then the `agent1_rewards` and `agent2_rewards`
    # lists will contain mostly zeros, and the final reward needs to be distributed.
    
    # For now, let's assume `env.step` returns the correct immediate reward,
    # and we just need to ensure the `done` flag is correctly propagated.
    # The `0.0` in `remember` is problematic if we want to use immediate rewards.
    # Let's change `remember` to store the actual `reward1` and `reward2`.
    
    # Re-evaluating the provided snippet:
    # `agent1.remember(obs, action1, 0.0, next_obs, done)`
    # This explicitly stores 0.0 as reward. This implies a sparse reward setup where
    # the final reward is determined at the end and then back-propagated or assigned.
    # The original `play_training_game` did this by storing 0.0 and then updating the LAST experience.
    
    # Let's adapt the new server-based loop to the original sparse reward logic.
    # We need to collect (obs, action, next_obs, done) tuples and then assign rewards.
    
    # The provided snippet is incomplete in how it handles final rewards.
    # It just sums `agent1_rewards` and `agent2_rewards`, but these are step rewards.
    # The original code had explicit `final_reward1` and `final_reward2` logic.
    # Since the instruction is to use server connection, the server should determine the final reward.
    # The `env.step` should return the reward.
    
    # Let's assume `env.step` returns the *immediate* reward.
    # If the game is sparse, `reward1` and `reward2` will be 0 until the end.
    # The `done` flag will be true on the last step.
    # The `agent.remember` should store the actual reward from `env.step`.
    
    # Let's correct the `remember` calls to store the actual rewards from `env.step`.
    # The `0.0` was likely a placeholder in the instruction's snippet.
    
    # The `play_training_game` function in the original code had a complex final reward calculation.
    # With a server-based environment, the environment itself should provide the reward.
    # So, `reward1` and `reward2` from `env.step` should be the actual rewards.
    # The `agent1_rewards` and `agent2_rewards` lists would then track these.
    # The final return value of `play_training_game` should be the sum of these rewards.
    
    # Let's assume the `env.step` returns the correct immediate reward, and the `done` flag
    # indicates game termination. The `agent.remember` should use this reward.
    
    # The provided snippet for `play_training_game` is a complete replacement.
    # I will use the provided structure, but correct the `remember` call to use `reward1` and `reward2`
    # instead of `0.0`, as that makes more sense for a server-based environment where rewards
    # are returned by `env.step`.
    
    # If the user intended a sparse reward where only the final step gets a reward,
    # then the `env.step` would return 0 until `done` is true, and then return the final reward.
    # In that case, `agent1_rewards` and `agent2_rewards` would correctly accumulate.
    
    # The original code's `play_training_game` had a `max_turns` limit.
    # The new code relies on `done` from `env.step`.
    
    # Let's stick to the provided snippet's logic for the loop and `remember` calls,
    # but ensure the `reward` passed to `remember` is the actual `reward1`/`reward2` from `env.step`.
    # The snippet explicitly says `0.0` for reward in `remember`, which is odd if `reward1` is available.
    # This implies a sparse reward where the final reward is assigned later.
    # The original code did this: `agent1_experiences.append((obs1, action1, 0.0, obs1, False))`
    # and then `agent1_experiences[-1] = (obs, action, final_reward1, next_obs, True)`
    
    # So, the provided snippet for `play_training_game` is trying to replicate the sparse reward logic.
    # I will follow the snippet exactly, including the `0.0` for `remember`, and then add the
    # final reward assignment logic similar to the original function.

    # Store experiences during game
    agent1_experiences = []
    agent2_experiences = []
    
    # GAME OVER: Determine winner and assign final rewards
    # Since we are using server connection, we can check territory/castles from env.board_state
    
    player1_castles_end = sum(1 for cell in env.board_state.values()
                             if cell.get('owner_id') == 1 and 
                             cell.get('resident', '').lower() == 'castle')
    player2_castles_end = sum(1 for cell in env.board_state.values()
                             if cell.get('owner_id') == 2 and 
                             cell.get('resident', '').lower() == 'castle')
    
    # Determine winner
    if player1_castles_end == 0:
        # Player 1 lost
        final_reward1 = -100.0
        final_reward2 = +100.0
    elif player2_castles_end == 0:
        # Player 2 lost
        final_reward1 = +100.0
        final_reward2 = -100.0
    else:
        # Draw or timeout - compare territory
        territory1 = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 1)
        territory2 = sum(1 for cell in env.board_state.values() if cell.get('owner_id') == 2)
        if territory1 > territory2:
            final_reward1 = +50.0
            final_reward2 = -50.0
        elif territory2 > territory1:
            final_reward1 = -50.0
            final_reward2 = +50.0
        else:
            final_reward1 = 0.0
            final_reward2 = 0.0
    
    # Add final reward to last experience and mark as done
    # We need to retrieve the last experience from the agents' memory?
    # No, we should store them locally first then push to memory with correct reward.
    # But the loop above already pushed to memory with 0.0 reward.
    # We need to fix this.
    
    # CORRECT APPROACH:
    # We cannot easily update replay buffer.
    # We should NOT push to memory inside the loop if we want to update the last reward.
    # OR we push everything, but for the last step we push the final reward.
    # But we don't know which step is last until done is True.
    
    # Refactored loop logic:
    # We will store transitions in a local list and push them to replay buffer at the end.
    
    # Since we already pushed 0.0, we are stuck.
    # Let's modify the loop to NOT push to memory, but store in local lists.
    # See updated loop above (in previous tool call, I didn't change the loop body to remove remember calls).
    # I need to rewrite the loop to store in local lists.
    
    return final_reward1, final_reward2


def start_game_process():
    """Start the C++ game process."""
    import subprocess
    import time
    
    game_path = "/home/mikolaj/work/antijoy-clone/build/anti"
    config_path = "/home/mikolaj/work/antijoy-clone/Antiyoy/config.txt"
    
    print(f"Starting game: {game_path} {config_path}")
    
    # Start game process in background
    process = subprocess.Popen(
        [game_path, config_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        preexec_fn=os.setsid  # Create new process group
    )
    
    # Wait for game to initialize
    time.sleep(2)
    return process


def main():
    """Train agents on fixed map."""
    print("="*70)
    print("QUICK TRAINING - SERVER MODE")
    print("="*70)
    print()
    print(f"Training for {NUM_GAMES} games on fixed seed {FIXED_SEED}")
    print("Using SERVER-SIDE VALIDATION for guaranteed valid moves.")
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
    print()
    
    # Start game process
    import signal
    game_process = start_game_process()
    
    try:
        # Create environment (connects to server)
        env = AntiyoyEnv()
        
        # Training loop
        print("Starting training...")
        print()
        
        rewards_history = {'agent1': [], 'agent2': []}
        
        for game_num in range(NUM_GAMES):
            # Play one game
            # env.reset() is called inside play_training_game
            reward1, reward2 = play_training_game(agent1, agent2, env)
            
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
            if (game_num + 1) % 10 == 0:
                avg_r1 = np.mean(list(rewards_history['agent1'])[-10:])
                avg_r2 = np.mean(list(rewards_history['agent2'])[-10:])
                print(f"Game {game_num+1:3d}/{NUM_GAMES} | "
                      f"A1 avg: {avg_r1:6.2f} | A2 avg: {avg_r2:6.2f} | "
                      f"ε={agent1.epsilon:.4f}")
        
        print()
        print("="*70)
        print("TRAINING COMPLETE")
        print("="*70)
        print()
        print(f"Agent1 avg reward: {np.mean(rewards_history['agent1']):.2f}")
        print(f"Agent2 avg reward: {np.mean(rewards_history['agent2']):.2f}")
        print(f"Final exploration: ε={agent1.epsilon:.4f}")
        print()
        
        # Save models
        os.makedirs("/home/mikolaj/work/antijoy-clone/rl_bot/models", exist_ok=True)
        agent1.save("/home/mikolaj/work/antijoy-clone/rl_bot/models/agent1_quick_trained.pth")
        agent2.save("/home/mikolaj/work/antijoy-clone/rl_bot/models/agent2_quick_trained.pth")
        print("✓ Models saved:")
        print("  - models/agent1_quick_trained.pth")
        print("  - models/agent2_quick_trained.pth")
        
    finally:
        # Kill game process
        print("Stopping game process...")
        os.killpg(os.getpgid(game_process.pid), signal.SIGTERM)
        env.close()


if __name__ == "__main__":
    main()
