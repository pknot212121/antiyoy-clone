"""
RL Policy for Antiyoy AI.

Simple DQN (Deep Q-Network) implementation.
The network learns Q-values for each action given the state.

State: 14 features (normalized 0-1)
Actions: 5 (ATTACK, DEFEND, FARM, EXPAND, WAIT)
"""

import random
import json
import os
from typing import List, Tuple, Optional
from collections import deque

# Try to import numpy/torch, fall back gracefully
try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False
    print("[RL] Warning: numpy not found, using pure Python")

try:
    import torch
    import torch.nn as nn
    import torch.optim as optim
    HAS_TORCH = True
except ImportError:
    HAS_TORCH = False
    print("[RL] Warning: torch not found, using simple Q-table")


# ==================== SIMPLE Q-TABLE (No Dependencies) ====================

class QTablePolicy:
    """
    Simple Q-learning with discretized state.
    Works without numpy/torch.
    """
    
    def __init__(self, num_actions: int = 5, learning_rate: float = 0.1, 
                 discount: float = 0.95, epsilon: float = 0.3):
        self.num_actions = num_actions
        self.lr = learning_rate
        self.gamma = discount
        self.epsilon = epsilon
        self.epsilon_min = 0.05
        self.epsilon_decay = 0.995
        
        # Q-table: maps discretized state -> action values
        self.q_table = {}
        
        # Experience for training
        self.last_state = None
        self.last_action = None
    
    def discretize_state(self, state: List[float]) -> Tuple:
        """Convert continuous state to discrete bins."""
        # Use 5 bins per feature
        bins = 5
        return tuple(min(int(s * bins), bins - 1) for s in state)
    
    def get_q_values(self, state: List[float]) -> List[float]:
        """Get Q-values for all actions."""
        key = self.discretize_state(state)
        if key not in self.q_table:
            # Initialize with small random values
            self.q_table[key] = [random.uniform(-0.1, 0.1) for _ in range(self.num_actions)]
        return self.q_table[key]
    
    def choose_action(self, state: List[float]) -> int:
        """Epsilon-greedy action selection."""
        if random.random() < self.epsilon:
            return random.randint(0, self.num_actions - 1)
        
        q_values = self.get_q_values(state)
        return q_values.index(max(q_values))
    
    def update(self, state: List[float], action: int, reward: float, 
               next_state: List[float], done: bool):
        """Q-learning update."""
        q_values = self.get_q_values(state)
        next_q_values = self.get_q_values(next_state)
        
        if done:
            target = reward
        else:
            target = reward + self.gamma * max(next_q_values)
        
        # Update Q-value
        q_values[action] += self.lr * (target - q_values[action])
        
        # Decay epsilon
        self.epsilon = max(self.epsilon_min, self.epsilon * self.epsilon_decay)
    
    def save(self, filepath: str):
        """Save Q-table to JSON."""
        data = {
            'q_table': {str(k): v for k, v in self.q_table.items()},
            'epsilon': self.epsilon
        }
        with open(filepath, 'w') as f:
            json.dump(data, f)
        print(f"[RL] Saved Q-table to {filepath}")
    
    def load(self, filepath: str):
        """Load Q-table from JSON."""
        if os.path.exists(filepath):
            with open(filepath, 'r') as f:
                data = json.load(f)
            self.q_table = {eval(k): v for k, v in data['q_table'].items()}
            self.epsilon = data.get('epsilon', self.epsilon)
            print(f"[RL] Loaded Q-table from {filepath}")


# ==================== NEURAL NETWORK DQN (Requires PyTorch) ====================

if HAS_TORCH:
    class DQN(nn.Module):
        """Deep Q-Network."""
        
        def __init__(self, state_size: int = 14, action_size: int = 5, hidden: int = 64):
            super(DQN, self).__init__()
            self.fc1 = nn.Linear(state_size, hidden)
            self.fc2 = nn.Linear(hidden, hidden)
            self.fc3 = nn.Linear(hidden, action_size)
        
        def forward(self, x):
            x = torch.relu(self.fc1(x))
            x = torch.relu(self.fc2(x))
            return self.fc3(x)
    
    
    class DQNPolicy:
        """DQN with experience replay."""
        
        def __init__(self, state_size: int = 14, action_size: int = 5,
                     learning_rate: float = 0.001, discount: float = 0.95,
                     epsilon: float = 0.3, buffer_size: int = 10000, batch_size: int = 64):
            self.state_size = state_size
            self.action_size = action_size
            self.gamma = discount
            self.epsilon = epsilon
            self.epsilon_min = 0.05
            self.epsilon_decay = 0.995
            self.batch_size = batch_size
            
            # Networks
            self.policy_net = DQN(state_size, action_size)
            self.target_net = DQN(state_size, action_size)
            self.target_net.load_state_dict(self.policy_net.state_dict())
            
            self.optimizer = optim.Adam(self.policy_net.parameters(), lr=learning_rate)
            
            # Experience replay
            self.memory = deque(maxlen=buffer_size)
            self.update_target_every = 100
            self.steps = 0
        
        def choose_action(self, state: List[float]) -> int:
            """Epsilon-greedy action selection."""
            if random.random() < self.epsilon:
                return random.randint(0, self.action_size - 1)
            
            with torch.no_grad():
                state_tensor = torch.FloatTensor(state).unsqueeze(0)
                q_values = self.policy_net(state_tensor)
                return q_values.argmax().item()
        
        def remember(self, state, action, reward, next_state, done):
            """Store experience in replay buffer."""
            self.memory.append((state, action, reward, next_state, done))
        
        def replay(self):
            """Train on a batch of experiences."""
            if len(self.memory) < self.batch_size:
                return
            
            batch = random.sample(self.memory, self.batch_size)
            states, actions, rewards, next_states, dones = zip(*batch)
            
            states = torch.FloatTensor(states)
            actions = torch.LongTensor(actions).unsqueeze(1)
            rewards = torch.FloatTensor(rewards)
            next_states = torch.FloatTensor(next_states)
            dones = torch.FloatTensor(dones)
            
            # Current Q values
            current_q = self.policy_net(states).gather(1, actions).squeeze()
            
            # Target Q values
            with torch.no_grad():
                next_q = self.target_net(next_states).max(1)[0]
                target_q = rewards + (1 - dones) * self.gamma * next_q
            
            # Loss and backprop
            loss = nn.MSELoss()(current_q, target_q)
            self.optimizer.zero_grad()
            loss.backward()
            self.optimizer.step()
            
            # Update target network
            self.steps += 1
            if self.steps % self.update_target_every == 0:
                self.target_net.load_state_dict(self.policy_net.state_dict())
            
            # Decay epsilon
            self.epsilon = max(self.epsilon_min, self.epsilon * self.epsilon_decay)
        
        def update(self, state, action, reward, next_state, done):
            """Store experience and train."""
            self.remember(state, action, reward, next_state, done)
            self.replay()
        
        def save(self, filepath: str):
            """Save model weights."""
            torch.save({
                'policy_net': self.policy_net.state_dict(),
                'target_net': self.target_net.state_dict(),
                'epsilon': self.epsilon,
                'steps': self.steps
            }, filepath)
            print(f"[RL] Saved DQN to {filepath}")
        
        def load(self, filepath: str):
            """Load model weights."""
            if os.path.exists(filepath):
                checkpoint = torch.load(filepath)
                self.policy_net.load_state_dict(checkpoint['policy_net'])
                self.target_net.load_state_dict(checkpoint['target_net'])
                self.epsilon = checkpoint.get('epsilon', self.epsilon)
                self.steps = checkpoint.get('steps', 0)
                print(f"[RL] Loaded DQN from {filepath}")


# ==================== FACTORY ====================

def create_policy(use_dqn: bool = True):
    """Create the best available policy."""
    if use_dqn and HAS_TORCH:
        print("[RL] Using DQN policy (PyTorch)")
        return DQNPolicy()
    else:
        print("[RL] Using Q-table policy (pure Python)")
        return QTablePolicy()


# ==================== REWARD CALCULATOR ====================

class RewardCalculator:
    """
    Calculate reward signal for RL training.
    Called after each turn to compute reward.
    """
    
    def __init__(self):
        self.prev_hexes = 0
        self.prev_income = 0
        self.prev_enemy_hexes = 0
        self.prev_units = 0
    
    def calculate(self, my_hexes: int, income: int, enemy_hexes: int, 
                  my_units: int, won: bool, lost: bool) -> float:
        """
        Calculate reward based on game changes.
        
        Positive rewards:
        - Gaining territory
        - Increasing income
        - Killing enemy hexes
        - Winning
        
        Negative rewards:
        - Losing territory
        - Decreasing income
        - Losing units
        - Losing game
        """
        reward = 0.0
        
        # Territory change
        hex_delta = my_hexes - self.prev_hexes
        reward += hex_delta * 1.0
        
        # Income change
        income_delta = income - self.prev_income
        reward += income_delta * 0.5
        
        # Enemy territory lost (we attacked)
        enemy_lost = self.prev_enemy_hexes - enemy_hexes
        reward += enemy_lost * 0.5
        
        # Unit losses
        unit_delta = my_units - self.prev_units
        if unit_delta < 0:  # Lost units
            reward += unit_delta * 2.0  # Penalty for losing units
        
        # Win/Lose
        if won:
            reward += 100.0
        if lost:
            reward -= 100.0
        
        # Update prev values
        self.prev_hexes = my_hexes
        self.prev_income = income
        self.prev_enemy_hexes = enemy_hexes
        self.prev_units = my_units
        
        return reward
    
    def reset(self):
        """Reset for new game."""
        self.prev_hexes = 0
        self.prev_income = 0
        self.prev_enemy_hexes = 0
        self.prev_units = 0

