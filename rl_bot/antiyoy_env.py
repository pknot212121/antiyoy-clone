"""
OpenAI Gym environment wrapper for Antiyoy game.
Provides standard gym interface (reset, step, render) for RL training.
"""

import gym
from gym import spaces
import os
import sys
import numpy as np
import logging
import time
from typing import Dict, List, Tuple, Optional

from socket_client import GameSocketClient, Resident
from game_rules import GameRules

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class AntiyoyEnv(gym.Env):
    """
    Gym environment for Antiyoy.
    
    Observation: Flattened board state + resource info
    Action: Discrete (place unit, move unit, end turn)
    Reward: Territory control, income, elimination bonuses
    """
    
    metadata = {'render.modes': ['human', 'none']}
    
    def __init__(self, host='127.0.0.1', port=2137):
        super().__init__()
        self.host = host
        self.port = port
        self.client = GameSocketClient(host, port)  # Initialize client
        self.connected = False
        
        # Board configuration - MUST MATCH config.txt!
        self.board_width = 8   # 8x8 board for curriculum training
        self.board_height = 8
        self.board_size = self.board_width * self.board_height
        
        # Game state
        self.board_state = {}
        self.current_player_id = 1
        self.prev_board_state = {}
        self.prev_territory = 0
        self.prev_income = 0
        self.step_count = 0
        self.episode_reward = 0.0
        
        # Player money tracking (starts at 100 per castle like in C++)
        self.player_money = {}
        
        # Action space: discrete
        # Simplified: treat as multi-discrete or use action mapping
        # Observation space: flat vector of board + resources
        # board: width*height*13 features per hex (owner, resident, power, capturable, adjacency)
        # + global: 10 features (money, income, territory, castles, enemy_castles, player_id, step, reserved)
        # Observation space (must match _encode_observation output size)
        # For 8x8: 8*8=64 hexes * 13 features + 10 global = 842
        obs_size = self.board_width * self.board_height * 13 + 10
        self.observation_space = spaces.Box(
            low=0, high=255,
            shape=(obs_size,),
            dtype=np.uint8
        )
        
        # Action space:
        # - 1 END_TURN action (0)
        # - 64*7 = 448 PLACE actions (1-448): 7 unit types per hex
        # - 64*6 = 384 MOVE actions (449-832): 6 directions per hex
        # Total: 833 actions for 8x8 board
        self.action_size = 1 + (self.board_width * self.board_height * 7) + \
                          (self.board_width * self.board_height * 6)
        self.action_space = spaces.Discrete(self.action_size)
        
        self.connected = False
        self.game_over = False
        
        # Cache for valid action sources (target, resident) -> (source_x, source_y)
        # Populated by get_valid_actions_from_server()
        self.valid_action_sources = {}
        
    def _calculate_num_actions(self) -> int:
        """Estimate total number of possible actions."""
        # This method is now deprecated as action_size is calculated directly in __init__
        # and the action space is fixed for an 8x8 board.
        # 1 end_turn + 7*board_size place actions + board_size*6 move actions
        # This is an upper bound; not all actions are valid at each step
        return 1 + (7 * self.board_size) + (self.board_size * 6)
    
    def _generate_board(self, seed: int) -> dict:
        """
        Generate initial board state using the same seed as the game server.
        This is a simplified version - in reality, the game's algorithm is more complex.
        For now, we generate a randomized board with player territories.
        """
        np.random.seed(seed)
        board_state = {}
        
        # Generate board with dimensions from self.board_width x self.board_height
        for y in range(self.board_height):
            for x in range(self.board_width):
                # Assign territories to players (simplified)
                owner_id = (x + y) % 2 + 1  # Alternate between players 1 and 2
                # Most hexes start as EMPTY
                resident = Resident.EMPTY.value
                
                # Randomly place some initial structures
                r = np.random.random()
                if r < 0.05:
                    resident = Resident.CASTLE.value
                elif r < 0.10:
                    resident = Resident.WARRIOR1.value
                elif r < 0.15:
                    resident = Resident.FARM.value
                elif r < 0.20:
                    resident = Resident.PALM_TREE.value
                
                board_state[(x, y)] = {
                    "owner_id": owner_id,
                    "resident": Resident(resident).name.lower()
                }
        
        return board_state
    
    def reset(self, force_new_game=False):
        """Reset the environment to initial state.
        
        Args:
            force_new_game: If True, disconnect and reconnect to force server to create new game
        """
        logger.info("Resetting environment...")
        
        # Force new game by disconnecting if already connected
        if force_new_game and self.connected:
            logger.info("Forcing new game - disconnecting from server...")
            self.client.close()
            self.connected = False
            logger.info("Disconnected. Waiting for server cleanup...")
            time.sleep(0.2)  # Give server time to clean up
            logger.info("Ready to reconnect")
        
        # Connect if not already
        if not self.connected:
            logger.info("Attempting to connect to game server...")
            if not self.client.connect():
                raise RuntimeError("Failed to connect to game server")
            logger.info("Socket connected, starting handshake...")
            
            # Handshake: Game sends magic numbers first, we receive then respond
            logger.info("Waiting for magic numbers from game...")
            if not self.client.receive_magic_numbers():
                raise RuntimeError("Failed to receive magic numbers from game")
            
            logger.info("Sending magic numbers back to game...")
            if not self.client.send_magic_numbers():
                raise RuntimeError("Failed to send magic numbers")
            
            # Get game config
            logger.info("Receiving game config...")
            if not self.client.receive_game_config(expect_tag=True):
                raise RuntimeError("Failed to receive game config")
            
            logger.info(f"Game config received: {self.client.game_config}")
            self.board_width = self.client.game_config['width']
            self.board_height = self.client.game_config['height']
            self.board_size = self.board_width * self.board_height
            self.connected = True
            
            # Generate board locally using the same seed as the server
            # This ensures both agents have the same board state
            seed = self.client.game_config['seed']
            logger.info(f"Generating board locally with seed {seed}...")
            self.board_state = self._generate_board(seed)
            logger.info(f"Generated board with {len(self.board_state)} hexagons")
        
        # Setup initial state
        if not self.board_state:
            raise RuntimeError("Board state is empty")
        
        # Initialize player money (each castle starts with 100 money)
        self.player_money = {}
        for player_id in range(1, 10):  # Support up to 10 players
            # Count castles for this player
            castle_count = sum(1 for cell in self.board_state.values()
                             if cell.get('owner_id') == player_id and 
                             cell.get('resident', '').lower() == 'castle')
            if castle_count > 0:
                self.player_money[player_id] = 100 * castle_count
        
        self.prev_board_state = self.board_state.copy()
        self.current_player_id = 1
        self.step_count = 0
        self.episode_reward = 0.0
        self.game_over = False
        self.prev_territory = self._count_territory(self.current_player_id)
        self.prev_income = self._calculate_income(self.current_player_id)
        
        obs = self._encode_observation()
        logger.info("Environment reset complete")
        return obs

    def reset_local(self, seed: int = None):
        """
        Reset environment for LOCAL training (no server connection).
        Initializes board and money.
        """
        logger.info(f"Resetting local environment with seed {seed}...")
        
        # Generate board locally
        self.board_state = self._generate_board(seed)
        
        # Initialize player money
        self.player_money = {}
        for player_id in range(1, 10):
            castle_count = sum(1 for cell in self.board_state.values()
                             if cell.get('owner_id') == player_id and 
                             cell.get('resident', '').lower() == 'castle')
            if castle_count > 0:
                self.player_money[player_id] = 100 * castle_count
            else:
                self.player_money[player_id] = 10  # Default starting money if no castle
        
        self.current_player_id = 1
        self.step_count = 0
        self.episode_reward = 0.0
        self.game_over = False
        self.prev_territory = self._count_territory(self.current_player_id)
        self.prev_income = self._calculate_income(self.current_player_id)
        
        return self._encode_observation()
    
    def _decode_action(self, action: int) -> Tuple[str, int, int, any]:
        """
        Decode action index to (action_type, x, y, param).
        
        Action encoding (dynamic based on board size):
        - 0: End turn
        - 1 to (board_size*7): Place unit actions  
          - Indices: 1 + (y*width + x)*7 + unit_type
          - unit_type: 0-6 (WARRIOR1-4, FARM, TOWER, STRONG_TOWER)
          - Returns: ("place", x, y, resident_type)
        - (board_size*7+1) to end: Move warrior actions
          - Indices: (board_size*7+1) + (y*width + x)*6 + direction
          - Returns: ("move", x, y, (target_x, target_y))
        """
        if action == 0:
            return ("end_turn", 0, 0, None)
        
        # Calculate thresholds based on actual board size
        num_place_actions = self.board_size * 7
        place_actions_end = num_place_actions
        
        # Place actions: 1 to place_actions_end
        if 1 <= action <= place_actions_end:
            action_idx = action - 1
            hex_idx = action_idx // 7
            unit_type = action_idx % 7
            
            y = hex_idx // self.board_width
            x = hex_idx % self.board_width
            
            # Map unit type to resident enum (0-6 are warrior/unit types)
            # WARRIOR1-4 (2-5), FARM (10), TOWER (12), STRONG_TOWER (13)
            unit_map = [2, 3, 4, 5, 10, 12, 13]
            resident = unit_map[min(unit_type, 6)]
            
            return ("place", x, y, resident)
        
        # Move actions: place_actions_end+1 to end
        else:
            action_idx = action - place_actions_end - 1
            hex_idx = action_idx // 6
            direction = action_idx % 6
            
            y = hex_idx // self.board_width
            x = hex_idx % self.board_width
            
            # Hex neighbor offsets (axial coordinates)
            # Direction: 0-5 represent 6 neighbors in hexagonal grid
            neighbors = [
                (x + 1, y),      # E
                (x + 1, y - 1),  # NE
                (x, y - 1),      # NW
                (x - 1, y),      # W
                (x - 1, y + 1),  # SW
                (x, y + 1),      # SE
            ]
            
            target = neighbors[direction % 6]
            # Clamp to board
            target = (
                max(0, min(target[0], self.board_width - 1)),
                max(0, min(target[1], self.board_height - 1))
            )
            
            return ("move", x, y, target)
    
    def step(self, action: int):
        """
        Execute action and return observation, reward, done, info.
        
        NOTE: Real board state reception is disabled to avoid blocking.
        Local board state is updated locally for now.
        """
        if not self.connected:
            raise RuntimeError("Environment not connected. Call reset() first.")
        
        info = {}
        reward = 0.0  # Default: NO reward during game
        success = False
        done = False
        
        # Track castles for sparse rewards and game-end detection
        player_castles_before = sum(1 for cell in self.board_state.values()
                                   if cell.get('owner_id') == self.current_player_id and 
                                   cell.get('resident', '').lower() == 'castle')
        enemy_castles_before = sum(1 for cell in self.board_state.values()
                                  if cell.get('owner_id') != self.current_player_id and
                                  cell.get('owner_id') != 0 and
                                  cell.get('resident', '').lower() == 'castle')
        
        # Validate action
        valid_actions = self.get_valid_actions()
        if action not in valid_actions:
            logger.warning(f"Invalid action {action} attempted by player {self.current_player_id}")
            # Small penalty for invalid actions
            return self._encode_observation(), -0.1, False, {'invalid_action': True}
        
        # Decode and execute action
        action_type, x, y, param = self._decode_action(action)
        
        if action == 0:
            # End turn
            success = self.client.send_action_end_turn()
            if not success:
                logger.error("Failed to send end turn action")
        else:
            # Place or move action
            # Place or move action
            if action_type == "place":
                resident_type = param
                target_x, target_y = x, y
                
                # Lookup cached source castle from get_valid_actions_from_server()
                cache_key = (target_x, target_y, resident_type)
                if cache_key in self.valid_action_sources:
                    source_x, source_y = self.valid_action_sources[cache_key]
                    success = self.client.send_action_place(
                        resident_type=resident_type,
                        from_x=source_x, from_y=source_y,
                        to_x=target_x, to_y=target_y
                    )
                    if not success:
                        logger.error(f"Failed to send place action: resident={resident_type} ({source_x},{source_y})->({target_x},{target_y})")
                else:
                    logger.error(f"No cached source castle for place action at ({target_x},{target_y}) with resident {resident_type}")

                    
            elif action_type == "move":
                to_x, to_y = param
                success = self.client.send_action_move(
                    from_x=x, from_y=y,
                    to_x=to_x, to_y=to_y
                )
                if not success:
                    logger.error(f"Failed to send move action: ({x},{y})->({to_x},{to_y})")
            else:
                # Unknown action, end turn as fallback
                success = self.client.send_action_end_turn()
        
        # Try to receive updated board state
        try:
            import socket as sock_module
            old_timeout = self.client.sock.gettimeout()
            self.client.sock.settimeout(0.5)
            
            tag_byte = self.client.sock.recv(1, sock_module.MSG_PEEK)
            if tag_byte and tag_byte[0] == 2:  # BOARD tag
                updated_board = self.client.receive_board_state()
                if updated_board:
                    self.board_state = updated_board
                    logger.debug(f"Board state synced from game")
            
            self.client.sock.settimeout(old_timeout)
        except sock_module.timeout:
            logger.debug("No board state received (timeout), using local state")
        except Exception as e:
            logger.debug(f"Board state reception skipped: {e}")
        
        # OUTCOME-BASED REWARDS: Check game state changes
        player_castles_after = sum(1 for cell in self.board_state.values()
                                  if cell.get('owner_id') == self.current_player_id and 
                                  cell.get('resident', '').lower() == 'castle')
        enemy_castles_after = sum(1 for cell in self.board_state.values()
                                 if cell.get('owner_id') != self.current_player_id and
                                 cell.get('owner_id') != 0 and
                                 cell.get('resident', '').lower() == 'castle')
        
        # SPARSE REWARD: Castle capture/loss (game-changing events)
        # Heavily favor castle destruction to encourage aggressive play
        if player_castles_after < player_castles_before:
            reward -= 75.0  # Lost our castle - very bad!
        if enemy_castles_after < enemy_castles_before:
            reward += 150.0  # Destroyed enemy castle - EXTREMELY good!
        
        # GAME END DETECTION: No castles = lose, no enemy castles = win
        if player_castles_after == 0:
            done = True
            reward -= 100.0  # LOST THE GAME
        elif enemy_castles_after == 0:
            done = True
            reward += 200.0  # WON THE GAME - huge reward!
        
        # Update local state to track money/income accurately
        # This ensures observation stays in sync with game state
        if action == 0:  # End turn updates income
            income = self._calculate_income(self.current_player_id)
            if self.current_player_id in self.player_money:
                self.player_money[self.current_player_id] += income
            logger.debug(f"End turn: Player {self.current_player_id} earned {income}, total: {self.player_money.get(self.current_player_id, 0)}")
        
        # Update metadata
        self.prev_territory = self._count_territory(self.current_player_id)
        self.prev_income = self._calculate_income(self.current_player_id)
        
        self.episode_reward += reward
        self.step_count += 1
        
        # Also check max turns to end episode (reduced for 6x6 board)
        if self.step_count >= 300:
            done = True
        
        info = {
            'step': self.step_count,
            'episode_reward': self.episode_reward,
            'current_player': self.current_player_id,
            'territory': self._count_territory(self.current_player_id),
            'action_success': success,
            'action_type': action_type,
            'player_castles': player_castles_after,
            'enemy_castles': enemy_castles_after,
            'money': self.player_money.get(self.current_player_id, 0),
            'income': self._calculate_income(self.current_player_id)
        }
        
        obs = self._encode_observation()
        return obs, reward, done, info
    
    def render(self, mode='human'):
        """Render board state."""
        if mode == 'human':
            logger.info(f"=== Board State (Step {self.step_count}) ===")
            for y in range(self.board_height):
                row = []
                for x in range(self.board_width):
                    cell = self.board_state.get((x, y), {})
                    owner = cell.get('owner_id', 0)
                    resident = cell.get('resident', 'unknown')
                    row.append(f"{owner}:{resident[0]}")
                logger.info(" ".join(row))
    
    def close(self):
        """Cleanup."""
        if self.connected:
            self.client.disconnect()
            self.connected = False
    
    # Helper methods
    
    def _encode_observation(self) -> np.ndarray:
        """Convert board state to observation vector with complete game state."""
        obs = np.zeros(self.observation_space.shape[0], dtype=np.uint8)
        
        # Encode board (13 bytes per hex)
        for y in range(self.board_height):
            for x in range(self.board_width):
                idx = (y * self.board_width + x) * 13
                cell = self.board_state.get((x, y), {})
                
                # Basic hex info (3 bytes)
                obs[idx] = cell.get('owner_id', 0)
                obs[idx + 1] = self._resident_to_int(cell.get('resident', 'water'))
                obs[idx + 2] = self._get_hex_power(x, y)
                
                # Adjacency info: count friendly/enemy neighbors (10 bytes)
                # [3]: friendly warriors adjacent
                # [4]: enemy warriors adjacent  
                # [5]: friendly buildings adjacent
                # [6]: empty owned hexes adjacent
                # [7-12]: reserved for future features
                friendly_warriors = 0
                enemy_warriors = 0
                friendly_buildings = 0
                empty_owned = 0
                
                # Get neighbors (simplified - assume even column for now)
                neighbors = [
                    (x, y-1), (x-1, y-1), (x-1, y),
                    (x, y+1), (x+1, y), (x+1, y-1)
                ]
                
                for nx, ny in neighbors:
                    if 0 <= nx < self.board_width and 0 <= ny < self.board_height:
                        ncell = self.board_state.get((nx, ny), {})
                        nowner = ncell.get('owner_id', 0)
                        nresident = self._resident_to_int(ncell.get('resident', 'water'))
                        
                        if nowner == self.current_player_id:
                            if 2 <= nresident <= 9:  # Warriors
                                friendly_warriors += 1
                            elif 10 <= nresident <= 13:  # Buildings
                                friendly_buildings += 1
                            elif nresident == 1:  # Empty
                                empty_owned += 1
                        elif nowner != 0:
                            if 2 <= nresident <= 9:  # Enemy warriors
                                enemy_warriors += 1
                
                obs[idx + 3] = min(friendly_warriors, 255)
                obs[idx + 4] = min(enemy_warriors, 255)
                obs[idx + 5] = min(friendly_buildings, 255)
                obs[idx + 6] = min(empty_owned, 255)
        
        # Encode global game state (last 10 bytes)
        player_id = self.current_player_id
        obs[-10] = min(max(self.player_money.get(player_id, 0), 0) // 4, 255)  # Scale money (0-1020), clamp negatives
        obs[-9] = min(self._calculate_income(player_id) + 128, 255)  # Offset income (-128 to 127)
        obs[-8] = min(self._count_territory(player_id), 255)
        obs[-7] = min(self._count_castles(player_id), 255)
        obs[-6] = min(self._count_enemy_castles(player_id), 255)
        obs[-5] = player_id
        obs[-4] = min(self.step_count, 255)
        # obs[-3] to obs[-1] reserved for future use
        
        return obs
    
    def _resident_to_int(self, resident_name: str) -> int:
        """Convert resident string to enum int."""
        resident_map = {
            'water': Resident.WATER,
            'empty': Resident.EMPTY,
            'warrior1': Resident.WARRIOR1,
            'warrior2': Resident.WARRIOR2,
            'warrior3': Resident.WARRIOR3,
            'warrior4': Resident.WARRIOR4,
            'warrior1_moved': Resident.WARRIOR1_MOVED,
            'warrior2_moved': Resident.WARRIOR2_MOVED,
            'warrior3_moved': Resident.WARRIOR3_MOVED,
            'warrior4_moved': Resident.WARRIOR4_MOVED,
            'farm': Resident.FARM,
            'castle': Resident.CASTLE,
            'tower': Resident.TOWER,
            'strong_tower': Resident.STRONG_TOWER,
            'palm_tree': Resident.PALM_TREE,
            'pine_tree': Resident.PINE_TREE,
            'gravestone': Resident.GRAVESTONE,
        }
        return resident_map.get(resident_name, Resident.EMPTY)
    
    def _apply_action_locally(self, action: int):
        """
        Simulate action effects on local board state (for training).
        Updates self.board_state and self.player_money.
        """
        action_type, x, y, param = self._decode_action(action)
        
        if action == 0:
            # End turn: Update money with income
            income = self._calculate_income(self.current_player_id)
            self.player_money[self.current_player_id] += income
            return
            
        if action_type == "place":
            resident_type = param
            # Deduct money
            price = self._get_unit_price(resident_type)
            self.player_money[self.current_player_id] -= price
            
            # Update board
            self.board_state[(x, y)] = {
                'owner_id': self.current_player_id,
                'resident': Resident(resident_type).name.lower()
            }
            
        elif action_type == "move":
            to_x, to_y = param
            
            # Get moving unit
            from_cell = self.board_state.get((x, y))
            unit_type = self._resident_to_int(from_cell['resident'])
            
            # Remove from old spot (leave empty or palm tree if it was there? Simplified: leave empty)
            self.board_state[(x, y)] = {
                'owner_id': self.current_player_id,
                'resident': 'empty'
            }
            
            # Place in new spot (capture)
            self.board_state[(to_x, to_y)] = {
                'owner_id': self.current_player_id,
                'resident': Resident(unit_type).name.lower()
            }
            
            # Handle merging/protection logic? 
            # For simplified training, we assume valid moves just overwrite.
            
    def _get_unit_price(self, resident_type: int) -> int:
        """Get price for unit type."""
        if resident_type in [Resident.WARRIOR1.value]: return 10
        if resident_type in [Resident.WARRIOR2.value]: return 20
        if resident_type in [Resident.WARRIOR3.value]: return 30
        if resident_type in [Resident.WARRIOR4.value]: return 40
        if resident_type == Resident.FARM.value: return 12
        if resident_type == Resident.TOWER.value: return 15
        if resident_type == Resident.STRONG_TOWER.value: return 35
        return 0

    def _count_territory(self, player_id: int) -> int:
        """Count hexagons controlled by player."""
        count = 0
        for cell in self.board_state.values():
            if cell.get('owner_id') == player_id:
                count += 1
        return count
    
    
    def _get_direction(self, x: int, y: int, tx: int, ty: int) -> Optional[int]:
        """Calculate direction index (0-5) from (x,y) to (tx,ty)."""
        neighbors = [
            (x + 1, y),      # 0: E
            (x + 1, y - 1),  # 1: NE
            (x, y - 1),      # 2: NW
            (x - 1, y),      # 3: W
            (x - 1, y + 1),  # 4: SW
            (x, y + 1),      # 5: SE
        ]
        try:
            return neighbors.index((tx, ty))
        except ValueError:
            return None  # Target is not a direct neighbor
    
    def _calculate_income(self, player_id: int) -> int:
        """Estimate income for player based on buildings."""
        income_map = {
            'farm': 4,
            'tower': -1,
            'strong_tower': -6,
            'warrior1': -2,
            'warrior2': -6,
            'warrior3': -18,
            'warrior4': -38,
        }
        
        income = 0
        for cell in self.board_state.values():
            if cell.get('owner_id') == player_id:
                resident = cell.get('resident', 'empty')
                income += income_map.get(resident, 1)  # 1 base per hex
        
        return income
    
    def _count_castles(self, player_id: int) -> int:
        """Count castles owned by player."""
        return sum(1 for cell in self.board_state.values()
                   if cell.get('owner_id') == player_id and 
                   cell.get('resident', '').lower() == 'castle')
    
    def _count_enemy_castles(self, player_id: int) -> int:
        """Count total enemy castles."""
        return sum(1 for cell in self.board_state.values()
                   if cell.get('owner_id') != player_id and
                   cell.get('owner_id') != 0 and
                   cell.get('resident', '').lower() == 'castle')
    
    def _get_hex_power(self, x: int, y: int) -> int:
        """Get power level of resident at hex (0-4)."""
        cell = self.board_state.get((x, y), {})
        resident_str = cell.get('resident', 'water')
        resident = self._resident_to_int(resident_str)
        
        # Import power function from game_rules
        from game_rules import power
        return max(0, power(resident))

    
    def _compute_reward(self) -> float:
        """Compute reward for current step."""
        reward = 0.0
        player_id = 1  # Assume agent is player 1
        
        # Territory reward
        curr_territory = self._count_territory(player_id)
        territory_gain = curr_territory - self.prev_territory
        reward += territory_gain * 10.0
        self.prev_territory = curr_territory
        
        # Income reward
        curr_income = self._calculate_income(player_id)
        income_gain = curr_income - self.prev_income
        reward += income_gain * 0.1
        self.prev_income = curr_income
        
        # Time penalty - encourage faster victories!
        # Each turn costs a small amount to prevent stalling
        reward -= 0.05  # Increased from 0.01 to push for aggression
        
        return reward
    
    def get_valid_actions(self) -> List[int]:
        """
        Get list of valid actions for current state using GameRules validator.
        Valid actions include:
        - END_TURN (always valid)
        - PLACE: Units that can be afforded and placed per game rules
        - MOVE: Warriors that can move to valid destinations per game rules
        """
        valid = [0]  # End turn always valid
        
        # If connected to server, use server's valid actions for perfect sync
        if self.connected:
            return self.get_valid_actions_from_server()
            
        player_id = self.current_player_id
        
        # Create rules validator
        rules = GameRules(
            self.board_state,
            self.board_width,
            self.board_height,
            self.player_money
        )
        
        # Unit types that can be placed (mapped to action indices)
        # 0-6 map to: WARRIOR1, WARRIOR2, WARRIOR3, WARRIOR4, FARM, TOWER, STRONG_TOWER
        unit_map = [
            Resident.WARRIOR1.value,
            Resident.WARRIOR2.value,
            Resident.WARRIOR3.value,
            Resident.WARRIOR4.value,
            Resident.FARM.value,
            Resident.TOWER.value,
            Resident.STRONG_TOWER.value
        ]
        
        # Add valid place actions using game rules
        for unit_idx, resident_type in enumerate(unit_map):
            # Check if player can afford this unit type
            if not rules.can_afford(player_id, resident_type):
                continue
            
            # Get valid placement locations from game rules
            valid_hexes = rules.get_valid_placements(player_id, resident_type)
            
            for (x, y) in valid_hexes:
                hex_idx = y * self.board_width + x
                action_idx = 1 + hex_idx * 7 + unit_idx
                # No need to check upper bound - it's calculated correctly
                valid.append(action_idx)
        
        # Add valid move actions using game rules
        num_place_actions = self.board_size * 7
        move_action_start = num_place_actions + 1
        
        for y in range(self.board_height):
            for x in range(self.board_width):
                cell = self.board_state.get((x, y), {})
                if cell.get('owner_id') != player_id:
                    continue
                
                resident_str = cell.get('resident', 'water')
                resident = self._resident_to_int(resident_str)
                
                # Only unmoved warriors can move
                if not (Resident.WARRIOR1.value <= resident <= Resident.WARRIOR4.value):
                    continue
                
                # Get valid moves from game rules
                valid_moves = rules.get_valid_movements(x, y)
                
                for (tx, ty) in valid_moves:
                    # Encode move action
                    hex_idx = y * self.board_width + x
                    # Calculate direction based on target
                    direction = self._get_direction(x, y, tx, ty)
                    if direction is not None:
                        action_idx = move_action_start + hex_idx * 6 + direction
                        valid.append(action_idx)
        
        return valid

    def get_valid_actions_from_server(self) -> List[int]:
        """
        Get valid actions directly from the game server.
        Matches the server's rule logic exactly.
        """
        if not self.connected:
            return self.get_valid_actions()
            
        server_actions = self.client.request_valid_actions()
        valid_indices = {0}  # End turn is always valid
        
        # Clear the cache for this turn
        self.valid_action_sources = {}
        
        # Reverse unit map
        # unit_map = [2, 3, 4, 5, 10, 12, 13]
        resident_to_idx = {
            2: 0, 3: 1, 4: 2, 5: 3,  # Warriors
            10: 4,                   # Farm
            12: 5,                   # Tower
            13: 6                    # Strong Tower
        }
        
        place_offset = 1
        move_offset = self.board_size * 7 + 1
        
        for action in server_actions:
            act_type = action[0]
            
            if act_type == 'place':
                # ('place', resident, fx, fy, tx, ty)
                resident, fx, fy, tx, ty = action[1:]
                
                # Cache the source castle for this placement
                self.valid_action_sources[(tx, ty, resident)] = (fx, fy)
                
                # We encode based on TARGET hex (tx, ty)
                if resident in resident_to_idx:
                    unit_idx = resident_to_idx[resident]
                    hex_idx = ty * self.board_width + tx
                    action_idx = place_offset + (hex_idx * 7) + unit_idx
                    valid_indices.add(action_idx)
                    
            elif act_type == 'move':
                # ('move', fx, fy, tx, ty)
                fx, fy, tx, ty = action[1:]
                
                direction = self._get_direction(fx, fy, tx, ty)
                if direction is not None:
                    hex_idx = fy * self.board_width + fx
                    action_idx = move_offset + (hex_idx * 6) + direction
                    valid_indices.add(action_idx)
        
        return list(valid_indices)
