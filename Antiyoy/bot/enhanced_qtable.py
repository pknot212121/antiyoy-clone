"""
Enhanced Q-Table Implementation for Antijoy

Features:
- Function approximation with tile coding for continuous states
- Prioritized experience replay
- N-step returns for better credit assignment
- Adaptive learning rate
- State abstraction and generalization
- Double Q-learning to reduce overestimation
"""

import random
import json
import math
from typing import List, Tuple, Dict, Optional
from collections import deque, defaultdict


class TileCoding:
    """
    Tile coding for function approximation.
    Maps continuous state space to discrete tiles for better generalization.
    """
    
    def __init__(self, num_tilings: int = 8, tiles_per_dim: int = 8, 
                 state_dim: int = 14):
        self.num_tilings = num_tilings
        self.tiles_per_dim = tiles_per_dim
        self.state_dim = state_dim
        
        # Random offsets for each tiling
        self.offsets = []
        for _ in range(num_tilings):
            offset = [random.random() / tiles_per_dim for _ in range(state_dim)]
            self.offsets.append(offset)
    
    def get_tiles(self, state: List[float]) -> List[Tuple]:
        """Get tile indices for each tiling."""
        tiles = []
        for tiling_idx, offset in enumerate(self.offsets):
            tile = []
            for dim_idx, (value, off) in enumerate(zip(state, offset)):
                # Shift and discretize
                shifted = value + off
                tile_idx = int(shifted * self.tiles_per_dim)
                tile_idx = max(0, min(self.tiles_per_dim - 1, tile_idx))
                tile.append(tile_idx)
            tiles.append((tiling_idx, tuple(tile)))
        return tiles


class PrioritizedReplayBuffer:
    """
    Prioritized experience replay buffer.
    Samples important experiences more frequently.
    """
    
    def __init__(self, capacity: int = 50000, alpha: float = 0.6):
        self.capacity = capacity
        self.alpha = alpha  # Priority exponent
        self.buffer = []
        self.priorities = []
        self.position = 0
    
    def add(self, experience: Tuple, priority: float = 1.0):
        """Add experience with priority."""
        priority = (priority + 1e-5) ** self.alpha
        
        if len(self.buffer) < self.capacity:
            self.buffer.append(experience)
            self.priorities.append(priority)
        else:
            self.buffer[self.position] = experience
            self.priorities[self.position] = priority
        
        self.position = (self.position + 1) % self.capacity
    
    def sample(self, batch_size: int) -> List[Tuple]:
        """Sample batch with priority-based sampling."""
        if len(self.buffer) == 0:
            return []
        
        batch_size = min(batch_size, len(self.buffer))
        
        # Normalize priorities
        total = sum(self.priorities)
        probs = [p / total for p in self.priorities]
        
        # Sample indices
        indices = random.choices(range(len(self.buffer)), weights=probs, k=batch_size)
        
        return [self.buffer[i] for i in indices]
    
    def update_priority(self, index: int, priority: float):
        """Update priority of experience."""
        if 0 <= index < len(self.priorities):
            self.priorities[index] = (priority + 1e-5) ** self.alpha
    
    def __len__(self):
        return len(self.buffer)


class EnhancedQTable:
    """
    Enhanced Q-learning with advanced features.
    
    Features:
    - Tile coding for generalization
    - Prioritized replay
    - N-step returns
    - Double Q-learning
    - Adaptive learning rate
    - Eligibility traces (optional)
    """
    
    def __init__(self, num_actions: int = 5, 
                 learning_rate: float = 0.1,
                 discount: float = 0.99,
                 epsilon: float = 0.5,
                 n_step: int = 3,
                 use_double: bool = True,
                 use_tiles: bool = True):
        
        self.num_actions = num_actions
        self.lr = learning_rate
        self.gamma = discount
        self.epsilon = epsilon
        self.epsilon_min = 0.01
        self.epsilon_decay = 0.9995
        self.n_step = n_step
        self.use_double = use_double
        self.use_tiles = use_tiles
        
        # Q-tables (double Q-learning uses two)
        self.q_table_a = defaultdict(lambda: [0.0] * num_actions)
        self.q_table_b = defaultdict(lambda: [0.0] * num_actions)
        
        # Tile coding for state abstraction
        self.tile_coder = TileCoding() if use_tiles else None
        
        # N-step buffer
        self.n_step_buffer = deque(maxlen=n_step)
        
        # Prioritized replay
        self.replay_buffer = PrioritizedReplayBuffer()
        
        # Stats
        self.updates = 0
        self.state_visits = defaultdict(int)
        
        # Move tracking for statistics
        self.random_moves = 0      # Epsilon-greedy random exploration
        self.qtable_moves = 0      # Moves from existing Q-table entries
        self.new_state_moves = 0   # Moves on new states (not in Q-table yet)
        
        # Adaptive learning rate
        self.base_lr = learning_rate
    
    def state_to_key(self, state: List[float]) -> str:
        """Convert state to hashable key with tile coding or discretization."""
        if self.use_tiles and self.tile_coder:
            tiles = self.tile_coder.get_tiles(state)
            return str(tiles)
        else:
            # Simple discretization (5 bins per feature)
            bins = 5
            discrete = tuple(min(int(s * bins), bins - 1) for s in state)
            return str(discrete)
    
    def get_q_values(self, state: List[float], use_b: bool = False) -> List[float]:
        """Get Q-values for state."""
        key = self.state_to_key(state)
        if use_b:
            return self.q_table_b[key]
        else:
            return self.q_table_a[key]
    
    def get_combined_q_values(self, state: List[float]) -> List[float]:
        """Get averaged Q-values from both tables (double Q-learning)."""
        if self.use_double:
            qa = self.get_q_values(state, use_b=False)
            qb = self.get_q_values(state, use_b=True)
            return [(a + b) / 2 for a, b in zip(qa, qb)]
        else:
            return self.get_q_values(state, use_b=False)
    
    def choose_action(self, state: List[float], greedy: bool = False) -> int:
        """Epsilon-greedy action selection with optional pure greedy."""
        if not greedy and random.random() < self.epsilon:
            self.random_moves += 1
            return random.randint(0, self.num_actions - 1)
        
        # Check if state exists in Q-table
        key = self.state_to_key(state)
        state_exists = key in self.q_table_a
        
        q_values = self.get_combined_q_values(state)
        
        # Break ties randomly
        max_q = max(q_values)
        best_actions = [i for i, q in enumerate(q_values) if q == max_q]
        
        # Track whether this was from existing Q-table or new state
        if state_exists:
            self.qtable_moves += 1
        else:
            self.new_state_moves += 1
        
        return random.choice(best_actions)
    
    def update_n_step(self, state: List[float], action: int, reward: float,
                      next_state: List[float], done: bool):
        """
        N-step TD update for better credit assignment.
        """
        # Add to n-step buffer
        self.n_step_buffer.append((state, action, reward, next_state, done))
        
        # Only update when buffer is full or episode ends
        if len(self.n_step_buffer) < self.n_step and not done:
            return
        
        # Calculate n-step return
        n_step_return = 0.0
        n_step_gamma = 1.0
        
        for _, _, r, _, d in self.n_step_buffer:
            n_step_return += n_step_gamma * r
            n_step_gamma *= self.gamma
            if d:
                break
        
        # Add bootstrap value if not terminal
        if not done:
            _, _, _, last_state, _ = self.n_step_buffer[-1]
            next_q_values = self.get_combined_q_values(last_state)
            n_step_return += n_step_gamma * max(next_q_values)
        
        # Update first state in buffer
        first_state, first_action, _, _, _ = self.n_step_buffer[0]
        self._update_q_value(first_state, first_action, n_step_return)
        
        # If done, flush buffer
        if done:
            self.n_step_buffer.clear()
    
    def _update_q_value(self, state: List[float], action: int, target: float):
        """Update Q-value with adaptive learning rate."""
        key = self.state_to_key(state)
        self.state_visits[key] += 1
        
        # Adaptive learning rate (decay with visits)
        visits = self.state_visits[key]
        adaptive_lr = self.base_lr / (1 + math.log(visits))
        
        # Double Q-learning: randomly choose which table to update
        use_b = random.random() < 0.5 if self.use_double else False
        
        if use_b:
            current_q = self.q_table_b[key][action]
            self.q_table_b[key][action] += adaptive_lr * (target - current_q)
        else:
            current_q = self.q_table_a[key][action]
            self.q_table_a[key][action] += adaptive_lr * (target - current_q)
        
        self.updates += 1
        
        # Decay epsilon
        if self.updates % 10 == 0:
            self.epsilon = max(self.epsilon_min, self.epsilon * self.epsilon_decay)
    
    def update(self, state: List[float], action: int, reward: float,
               next_state: List[float], done: bool):
        """Main update function (uses n-step by default)."""
        # Store in replay buffer
        td_error = abs(reward)  # Simplified priority
        self.replay_buffer.add((state, action, reward, next_state, done), td_error)
        
        # N-step update
        self.update_n_step(state, action, reward, next_state, done)
        
        # Batch update from replay buffer
        if len(self.replay_buffer) >= 32:
            self._replay_update(batch_size=16)
    
    def _replay_update(self, batch_size: int = 32):
        """Update from prioritized replay buffer."""
        batch = self.replay_buffer.sample(batch_size)
        
        for state, action, reward, next_state, done in batch:
            if done:
                target = reward
            else:
                next_q = max(self.get_combined_q_values(next_state))
                target = reward + self.gamma * next_q
            
            self._update_q_value(state, action, target)
    
    def reset_move_counters(self):
        """Reset move tracking counters."""
        self.random_moves = 0
        self.qtable_moves = 0
        self.new_state_moves = 0
    
    def get_move_stats(self) -> Dict[str, int]:
        """Get current move statistics."""
        return {
            'random_moves': self.random_moves,
            'qtable_moves': self.qtable_moves,
            'new_state_moves': self.new_state_moves,
            'total_moves': self.random_moves + self.qtable_moves + self.new_state_moves
        }
    
    def save(self, filepath: str):
        """Save Q-tables to JSON."""
        data = {
            'q_table_a': {k: list(v) for k, v in self.q_table_a.items()},
            'q_table_b': {k: list(v) for k, v in self.q_table_b.items()} if self.use_double else {},
            'epsilon': self.epsilon,
            'updates': self.updates,
            'state_visits': dict(self.state_visits),
            'config': {
                'num_actions': self.num_actions,
                'use_double': self.use_double,
                'use_tiles': self.use_tiles,
                'n_step': self.n_step,
            }
        }
        
        with open(filepath, 'w') as f:
            json.dump(data, f)
        
        print(f"[Enhanced Q-Table] Saved {len(self.q_table_a)} states to {filepath}")
    
    def load(self, filepath: str):
        """Load Q-tables from JSON (supports both old and new formats)."""
        try:
            with open(filepath, 'r') as f:
                data = json.load(f)
            
            # Check if it's the old simple format or new enhanced format
            if 'q_table_a' in data:
                # New enhanced format
                for k, v in data['q_table_a'].items():
                    self.q_table_a[k] = v
                
                if self.use_double and 'q_table_b' in data:
                    for k, v in data['q_table_b'].items():
                        self.q_table_b[k] = v
            elif 'q_table' in data:
                # Old simple format - migrate to enhanced
                print(f"[Enhanced Q-Table] Migrating from simple Q-table format...")
                for k, v in data['q_table'].items():
                    self.q_table_a[k] = v
                    if self.use_double:
                        self.q_table_b[k] = v  # Copy to both tables
            else:
                print(f"[Enhanced Q-Table] Unknown format, starting fresh")
                return
            
            # Load metadata
            self.epsilon = data.get('epsilon', self.epsilon)
            self.updates = data.get('updates', 0)
            
            if 'state_visits' in data:
                self.state_visits = defaultdict(int, data['state_visits'])
            
            print(f"[Enhanced Q-Table] Loaded {len(self.q_table_a)} states from {filepath}")
            
        except FileNotFoundError:
            print(f"[Enhanced Q-Table] No saved model found at {filepath}, starting fresh")
        except Exception as e:
            print(f"[Enhanced Q-Table] Error loading: {e}, starting fresh")
    
    def get_stats(self) -> Dict:
        """Get training statistics."""
        return {
            'states': len(self.q_table_a),
            'updates': self.updates,
            'epsilon': self.epsilon,
            'replay_size': len(self.replay_buffer),
            'unique_states': len(self.state_visits),
        }


class ImprovedRewardShaping:
    """
    Advanced reward shaping for better learning.
    """
    
    def __init__(self):
        self.prev_stats = {}
    
    def calculate_reward(self, stats: Dict, won: bool, lost: bool) -> float:
        """
        Calculate shaped reward from game statistics.
        
        Stats should include:
        - my_hexes: number of controlled hexes
        - my_income: income per turn
        - my_units: number of units
        - enemy_hexes: enemy controlled hexes
        - enemy_units: enemy units nearby
        - game_length: turns played
        """
        reward = 0.0
        
        # Territory control (progressive bonus)
        if 'my_hexes' in stats and 'my_hexes' in self.prev_stats:
            hex_gain = stats['my_hexes'] - self.prev_stats['my_hexes']
            reward += hex_gain * 2.0  # Gaining territory is valuable
        
        # Economic growth
        if 'my_income' in stats and 'my_income' in self.prev_stats:
            income_gain = stats['my_income'] - self.prev_stats['my_income']
            reward += income_gain * 1.0  # Income is crucial
        
        # Military efficiency (unit preservation)
        if 'my_units' in stats and 'my_units' in self.prev_stats:
            unit_change = stats['my_units'] - self.prev_stats['my_units']
            if unit_change < 0:  # Lost units
                reward += unit_change * 1.5  # Penalty for losing units
        
        # Enemy pressure (reward for attacking)
        if 'enemy_hexes' in stats and 'enemy_hexes' in self.prev_stats:
            enemy_loss = self.prev_stats['enemy_hexes'] - stats['enemy_hexes']
            reward += enemy_loss * 3.0  # Big bonus for taking enemy territory
        
        # Survival bonus (reward for staying alive)
        if not lost:
            reward += 0.1
        
        # Win/Loss terminal rewards
        if won:
            reward += 100.0
            # Bonus for winning quickly
            if 'game_length' in stats:
                speed_bonus = max(0, 50 - stats['game_length'])
                reward += speed_bonus * 0.5
        
        if lost:
            reward -= 100.0
        
        # Update history
        self.prev_stats = stats.copy()
        
        return reward
    
    def reset(self):
        """Reset for new episode."""
        self.prev_stats = {}
