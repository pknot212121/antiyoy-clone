"""
Antiyoy RL Bot package.
"""

from .socket_client import GameSocketClient, Resident, SocketTag
from .antiyoy_env import AntiyoyEnv
from .config import *

__version__ = "0.1.0"
__all__ = ["GameSocketClient", "Resident", "SocketTag", "AntiyoyEnv"]
