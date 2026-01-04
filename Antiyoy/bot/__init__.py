"""
Antiyoy Bot Module
A Python AI implementation for the Antiyoy clone game.
"""

from .game_utils import GameUtils
from .province import Province, ProvinceManager
from .ai_simple import SimpleAI
from .ai_rl_ready import RLReadyAI, RLAction, GameState
from .rl_policy import QTablePolicy, RewardCalculator, create_policy

# Try to import DQN if PyTorch is available
try:
    from .rl_policy import DQNPolicy
except ImportError:
    DQNPolicy = None

__all__ = [
    'GameUtils', 'Province', 'ProvinceManager', 
    'SimpleAI', 'RLReadyAI', 'RLAction', 'GameState',
    'QTablePolicy', 'DQNPolicy', 'RewardCalculator', 'create_policy'
]

