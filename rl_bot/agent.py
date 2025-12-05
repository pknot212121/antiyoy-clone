"""
DQN Agent for Antiyoy game.
Implements Deep Q-Network with experience replay.
"""

import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np
from collections import deque
import logging
from typing import Tuple, List

logger = logging.getLogger(__name__)


class DQNNetwork(nn.Module):
    """
    Deep Q-Network architecture.
    Input: Flattened board state + metadata
    Output: Q-values for each action
    """
    
    def __init__(self, state_size: int, action_size: int, hidden_sizes: List[int] = None):
        super(DQNNetwork, self).__init__()
        
        if hidden_sizes is None:
            hidden_sizes = [128, 128]  # Reduced from [256, 256] for faster CPU training
        
        # Build network: state -> hidden layers -> action Q-values
        layers = [nn.Linear(state_size, hidden_sizes[0]), nn.ReLU()]
        
        for i in range(len(hidden_sizes) - 1):
            layers.append(nn.Linear(hidden_sizes[i], hidden_sizes[i+1]))
            layers.append(nn.ReLU())
        
        layers.append(nn.Linear(hidden_sizes[-1], action_size))
        
        self.network = nn.Sequential(*layers)
    
    def forward(self, state: torch.Tensor) -> torch.Tensor:
        """Forward pass: state -> Q-values."""
        return self.network(state)


class ReplayBuffer:
    """
    Experience replay buffer for DQN.
    Stores (state, action, reward, next_state, done) transitions.
    """
    
    def __init__(self, capacity: int = 5000):  # Reduced from 10000
        self.buffer = deque(maxlen=capacity)
    
    def push(self, state: np.ndarray, action: int, reward: float, 
             next_state: np.ndarray, done: bool):
        """Add transition to buffer."""
        self.buffer.append((state, action, reward, next_state, done))
    
    def sample(self, batch_size: int) -> Tuple[np.ndarray, np.ndarray, np.ndarray, 
                                                 np.ndarray, np.ndarray]:
        """Sample random batch from buffer."""
        indices = np.random.choice(len(self.buffer), batch_size, replace=False)
        
        states = np.array([self.buffer[i][0] for i in indices])
        actions = np.array([self.buffer[i][1] for i in indices])
        rewards = np.array([self.buffer[i][2] for i in indices])
        next_states = np.array([self.buffer[i][3] for i in indices])
        dones = np.array([self.buffer[i][4] for i in indices])
        
        return states, actions, rewards, next_states, dones
    
    def __len__(self):
        return len(self.buffer)
    
    def is_full(self) -> bool:
        return len(self.buffer) == self.buffer.maxlen


class DQNAgent:
    """
    DQN agent with experience replay and target network.
    """
    
    def __init__(self, 
                 state_size: int,
                 action_size: int,
                 learning_rate: float = 1e-4,
                 gamma: float = 0.99,
                 epsilon_start: float = 1.0,
                 epsilon_end: float = 0.1,
                 epsilon_decay: float = 0.995,
                 hidden_sizes: List[int] = None,
                 replay_buffer_size: int = 5000):
        """
        Initialize DQN agent.
        
        Args:
            state_size: Dimension of state space
            action_size: Number of possible actions
            learning_rate: Learning rate for optimizer
            gamma: Discount factor for future rewards
            epsilon_start: Initial exploration rate
            epsilon_end: Minimum exploration rate
            epsilon_decay: Exploration decay rate
            hidden_sizes: List of hidden layer sizes (default: [128, 128])
            replay_buffer_size: Size of replay buffer (default: 5000)
        """
        self.state_size = state_size
        self.action_size = action_size
        self.gamma = gamma
        self.epsilon = epsilon_start
        self.epsilon_end = epsilon_end
        self.epsilon_decay = epsilon_decay
        
        # Set default hidden sizes if not provided
        if hidden_sizes is None:
            hidden_sizes = [128, 128]
        
        # Device selection - check GPU compatibility
        use_cuda = False
        if torch.cuda.is_available():
            # Check compute capability (need 7.0+ for modern PyTorch)
            major, minor = torch.cuda.get_device_capability(0)
            compute_capability = major + minor / 10
            
            if compute_capability >= 7.0:
                use_cuda = True
                logger.info(f"Using GPU with compute capability {compute_capability}")
            else:
                logger.warning(f"GPU compute capability {compute_capability} is too old (need 7.0+). Using CPU.")
        
        self.device = torch.device("cuda" if use_cuda else "cpu")
        
        # Networks
        self.q_network = DQNNetwork(state_size, action_size, hidden_sizes).to(self.device)
        self.target_network = DQNNetwork(state_size, action_size, hidden_sizes).to(self.device)
        self.target_network.load_state_dict(self.q_network.state_dict())
        self.target_network.eval()  # Target network is not trained directly
        
        # Optimizer
        self.optimizer = optim.Adam(self.q_network.parameters(), lr=learning_rate)
        self.loss_fn = nn.MSELoss()
        
        # Experience replay
        self.replay_buffer = ReplayBuffer(capacity=replay_buffer_size)
        
        # Training stats
        self.total_steps = 0
        self.episode_rewards = []
    
    def select_action(self, state: np.ndarray, training: bool = True) -> int:
        """
        Select action using ε-greedy policy.
        
        Args:
            state: Current observation
            training: If True, use exploration; if False, greedy
        
        Returns:
            Action index
        """
        if training and np.random.random() < self.epsilon:
            # Explore: random action
            return np.random.randint(self.action_size)
        else:
            # Exploit: greedy action
            state_tensor = torch.from_numpy(state).float().unsqueeze(0).to(self.device)
            with torch.no_grad():
                q_values = self.q_network(state_tensor)
            return q_values.argmax(dim=1).item()
    
    def remember(self, state: np.ndarray, action: int, reward: float,
                 next_state: np.ndarray, done: bool):
        """Store transition in replay buffer."""
        self.replay_buffer.push(state, action, reward, next_state, done)
    
    def train_step(self, batch_size: int = 32) -> float:
        """
        Train on a batch from replay buffer.
        
        Returns:
            Loss value
        """
        if len(self.replay_buffer) < batch_size:
            return 0.0
        
        # Sample batch
        states, actions, rewards, next_states, dones = self.replay_buffer.sample(batch_size)
        
        # Convert to tensors
        states = torch.from_numpy(states).float().to(self.device)
        actions = torch.from_numpy(actions).long().to(self.device)
        rewards = torch.from_numpy(rewards).float().to(self.device)
        next_states = torch.from_numpy(next_states).float().to(self.device)
        dones = torch.from_numpy(dones).float().to(self.device)
        
        # Compute Q-values
        q_values = self.q_network(states).gather(1, actions.unsqueeze(1)).squeeze(1)
        
        # Compute target Q-values using target network
        with torch.no_grad():
            max_next_q_values = self.target_network(next_states).max(dim=1)[0]
            target_q_values = rewards + (1 - dones) * self.gamma * max_next_q_values
        
        # Compute loss and backprop
        loss = self.loss_fn(q_values, target_q_values)
        self.optimizer.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(self.q_network.parameters(), 1.0)
        self.optimizer.step()
        
        return loss.item()
    
    def update_target_network(self):
        """Update target network with current network weights."""
        self.target_network.load_state_dict(self.q_network.state_dict())
    
    def decay_epsilon(self):
        """Decay exploration rate."""
        self.epsilon = max(self.epsilon_end, self.epsilon * self.epsilon_decay)
    
    def save(self, filepath: str):
        """Save model weights."""
        torch.save(self.q_network.state_dict(), filepath)
        logger.info(f"Model saved to {filepath}")
    
    def load(self, filepath: str):
        """Load model weights."""
        self.q_network.load_state_dict(torch.load(filepath, map_location=self.device))
        self.target_network.load_state_dict(self.q_network.state_dict())
        logger.info(f"Model loaded from {filepath}")


# Example usage:
if __name__ == "__main__":
    # Initialize agent
    agent = DQNAgent(
        state_size=850,  # 400 hexes * 2 + 50 metadata
        action_size=68,  # Example action space
        learning_rate=1e-4,
        gamma=0.99,
        device="cuda" if torch.cuda.is_available() else "cpu"
    )
    
    print(f"Agent initialized. Device: {agent.device}")
    print(f"Q-Network: {agent.q_network}")
    
    # Dummy training loop (for testing)
    for episode in range(5):
        state = np.random.randn(850).astype(np.float32)
        total_reward = 0
        
        for step in range(10):
            action = agent.select_action(state, training=True)
            next_state = np.random.randn(850).astype(np.float32)
            reward = np.random.randn()
            done = (step == 9)
            
            agent.remember(state, action, reward, next_state, done)
            
            loss = agent.train_step(batch_size=4)
            total_reward += reward
            state = next_state
        
        agent.decay_epsilon()
        agent.update_target_network()
        
        print(f"Episode {episode+1} | Reward: {total_reward:.2f} | ε: {agent.epsilon:.3f}")
