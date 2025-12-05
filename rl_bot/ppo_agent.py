"""
PPO (Proximal Policy Optimization) Agent for Board Games
Implements Actor-Critic with GAE and clipped objective
"""

import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np
from collections import deque


class ActorCritic(nn.Module):
    """
    Actor-Critic network for PPO.
    Actor outputs action probabilities, Critic estimates state value.
    """
    def __init__(self, state_size, action_size, hidden_sizes=[256, 256]):
        super().__init__()
        
        self.state_size = state_size
        self.action_size = action_size
        
        # Shared backbone
        layers = []
        input_size = state_size
        for hidden_size in hidden_sizes:
            layers.extend([
                nn.Linear(input_size, hidden_size),
                nn.ReLU(),
            ])
            input_size = hidden_size
        
        self.shared = nn.Sequential(*layers)
        
        # Actor head (policy)
        self.actor = nn.Linear(input_size, action_size)
        
        # Critic head (value)
        self.critic = nn.Linear(input_size, 1)
        
    def forward(self, state):
        """Forward pass returning both policy and value."""
        features = self.shared(state)
        logits = self.actor(features)
        value = self.critic(features)
        return logits, value
    
    def get_action_and_value(self, state, action=None):
        """
        Get action distribution and value.
        If action provided, return log_prob of that action.
        """
        logits, value = self.forward(state)
        probs = torch.softmax(logits, dim=-1)
        dist = torch.distributions.Categorical(probs)
        
        if action is None:
            action = dist.sample()
        
        log_prob = dist.log_prob(action)
        entropy = dist.entropy()
        
        return action, log_prob, entropy, value


class RolloutBuffer:
    """Store trajectory data for PPO updates."""
    def __init__(self):
        self.states = []
        self.actions = []
        self.rewards = []
        self.values = []
        self.log_probs = []
        self.dones = []
        
    def add(self, state, action, reward, value, log_prob, done):
        self.states.append(state)
        self.actions.append(action)
        self.rewards.append(reward)
        self.values.append(value)
        self.log_probs.append(log_prob)
        self.dones.append(done)
    
    def clear(self):
        self.states = []
        self.actions = []
        self.rewards = []
        self.values = []
        self.log_probs = []
        self.dones = []
    
    def get(self):
        """Convert to tensors."""
        return (
            torch.FloatTensor(np.array(self.states)),
            torch.LongTensor(self.actions),
            torch.FloatTensor(self.rewards),
            torch.FloatTensor(self.values),
            torch.FloatTensor(self.log_probs),
            torch.FloatTensor(self.dones)
        )
    
    def __len__(self):
        return len(self.states)


class PPOAgent:
    """
    PPO Agent with clipped objective and GAE.
    """
    def __init__(
        self,
        state_size,
        action_size,
        lr=3e-4,
        gamma=0.99,
        gae_lambda=0.95,
        clip_epsilon=0.2,
        epochs=4,
        minibatch_size=64,
        hidden_sizes=[256, 256]
    ):
        self.gamma = gamma
        self.gae_lambda = gae_lambda  
        self.clip_epsilon = clip_epsilon
        self.epochs = epochs
        self.minibatch_size = minibatch_size
        
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        
        # Actor-Critic network
        self.ac_network = ActorCritic(state_size, action_size, hidden_sizes).to(self.device)
        self.optimizer = optim.Adam(self.ac_network.parameters(), lr=lr)
        
        # Rollout buffer
        self.buffer = RolloutBuffer()
        
        # Stats
        self.policy_losses = []
        self.value_losses = []
        self.entropies = []
        
    def select_action(self, state, valid_actions=None):
        """
        Select action using current policy.
        Args:
            state: Current state observation
            valid_actions: List of valid action indices (optional)
        Returns:
            action, log_prob, value
        """
        state_tensor = torch.FloatTensor(state).unsqueeze(0).to(self.device)
        
        with torch.no_grad():
            logits, value = self.ac_network(state_tensor)
            
            # Mask invalid actions if provided
            if valid_actions is not None:
                mask = torch.ones(logits.shape[-1], dtype=torch.bool)
                mask[valid_actions] = False
                logits[:, mask] = -1e10  # Large negative number
            
            probs = torch.softmax(logits, dim=-1)
            dist = torch.distributions.Categorical(probs)
            action = dist.sample()
            log_prob = dist.log_prob(action)
        
        return action.item(), log_prob.item(), value.item()
    
    def store_transition(self, state, action, reward, value, log_prob, done):
        """Store transition in rollout buffer."""
        self.buffer.add(state, action, reward, value, log_prob, done)
    
    def compute_gae(self, rewards, values, dones, next_value):
        """
        Compute Generalized Advantage Estimation.
        """
        advantages = []
        gae = 0
        
        # Append next value for bootstrap
        values_list = values.tolist() + [next_value]
        
        # Compute advantages backwards
        for t in reversed(range(len(rewards))):
            if dones[t]:
                delta = rewards[t] - values_list[t]
                gae = delta
            else:
                delta = rewards[t] + self.gamma * values_list[t + 1] - values_list[t]
                gae = delta + self.gamma * self.gae_lambda * gae
            
            advantages.insert(0, gae)
        
        # Returns = advantages + values
        returns = [adv + val for adv, val in zip(advantages, values.tolist())]
        
        return torch.FloatTensor(advantages), torch.FloatTensor(returns)
    
    def update(self, next_state):
        """
        PPO update with multiple epochs over collected data.
        """
        # Need at least 2 samples for stable updates
        if len(self.buffer) < 2:
            self.buffer.clear()
            return
        
        # Get data from buffer
        states, actions, rewards, old_values, old_log_probs, dones = self.buffer.get()
        
        states = states.to(self.device)
        actions = actions.to(self.device)
        old_log_probs = old_log_probs.to(self.device)
        
        # Compute next value for GAE
        next_state_tensor = torch.FloatTensor(next_state).unsqueeze(0).to(self.device)
        with torch.no_grad():
            _, next_value = self.ac_network(next_state_tensor)
            next_value = next_value.squeeze().item()
        
        # Compute advantages and returns
        advantages, returns = self.compute_gae(rewards, old_values, dones, next_value)
        advantages = advantages.to(self.device)
        returns = returns.to(self.device)
        
        # Normalize advantages (with safe std for small batches)
        if len(advantages) > 1:
            adv_std = advantages.std()
            if adv_std > 1e-8:
                advantages = (advantages - advantages.mean()) / (adv_std + 1e-8)
            else:
                advantages = advantages - advantages.mean()
        
        # PPO update for multiple epochs
        for epoch in range(self.epochs):
            # Shuffle data
            indices = torch.randperm(len(states))
            
            # Minibatch updates
            for start in range(0, len(states), self.minibatch_size):
                end = min(start + self.minibatch_size, len(states))
                mb_indices = indices[start:end]
                
                mb_states = states[mb_indices]
                mb_actions = actions[mb_indices]
                mb_old_log_probs = old_log_probs[mb_indices]
                mb_advantages = advantages[mb_indices]
                mb_returns = returns[mb_indices]
                
                # Forward pass
                logits, values = self.ac_network(mb_states)
                
                # Check for NaN in logits
                if torch.isnan(logits).any():
                    continue
                
                probs = torch.softmax(logits, dim=-1)
                
                # Check for NaN in probs
                if torch.isnan(probs).any() or (probs <= 0).any():
                    continue
                
                dist = torch.distributions.Categorical(probs)
                
                new_log_probs = dist.log_prob(mb_actions)
                entropy = dist.entropy().mean()
                
                # Policy loss (PPO clipped objective)
                ratio = torch.exp(new_log_probs - mb_old_log_probs)
                surr1 = ratio * mb_advantages
                surr2 = torch.clamp(ratio, 1 - self.clip_epsilon, 1 + self.clip_epsilon) * mb_advantages
                policy_loss = -torch.min(surr1, surr2).mean()
                
                # Value loss (ensure shapes match)
                values_flat = values.view(-1)
                value_loss = nn.MSELoss()(values_flat, mb_returns)
                
                # Total loss
                loss = policy_loss + 0.5 * value_loss - 0.01 * entropy
                
                # Check for NaN loss
                if torch.isnan(loss):
                    continue
                
                # Backprop
                self.optimizer.zero_grad()
                loss.backward()
                nn.utils.clip_grad_norm_(self.ac_network.parameters(), 0.5)
                self.optimizer.step()
                
                # Track stats
                self.policy_losses.append(policy_loss.item())
                self.value_losses.append(value_loss.item())
                self.entropies.append(entropy.item())
        
        # Clear buffer
        self.buffer.clear()
    
    def save(self, path):
        """Save model."""
        torch.save({
            'ac_network': self.ac_network.state_dict(),
            'optimizer': self.optimizer.state_dict(),
        }, path)
    
    def load(self, path):
        """Load model."""
        checkpoint = torch.load(path, map_location=self.device)
        self.ac_network.load_state_dict(checkpoint['ac_network'])
        self.optimizer.load_state_dict(checkpoint['optimizer'])
