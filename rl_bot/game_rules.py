"""
Game Rules Validator for Antiyoy
Implements the game logic from board.cpp to validate actions during training.
"""

import numpy as np
from typing import Dict, List, Tuple, Set, Optional
from socket_client import Resident

# Hex neighbor directions for even/odd columns (from board.cpp:469-487)
EVEN_DIRECTIONS = [
    (0, -1),   # top
    (-1, -1),  # top-left
    (-1, 0),   # bottom-left
    (0, 1),    # bottom
    (1, 0),    # bottom-right
    (1, -1)    # top-right
]

ODD_DIRECTIONS = [
    (0, -1),   # top
    (-1, 0),   # top-left
    (-1, 1),   # bottom-left
    (0, 1),    # bottom
    (1, 1),    # bottom-right
    (1, 0)     # top-right
]


def get_neighbors(x: int, y: int, width: int, height: int) -> List[Tuple[int, int]]:
    """Get valid neighbor coordinates for a hex."""
    directions = EVEN_DIRECTIONS if x % 2 == 0 else ODD_DIRECTIONS
    neighbors = []
    for dx, dy in directions:
        nx, ny = x + dx, y + dy
        if 0 <= nx < width and 0 <= ny < height:
            neighbors.append((nx, ny))
    return neighbors


def power(resident: int) -> int:
    """
    Get combat power of a resident (from board.cpp:24-32).
    Returns: -1 for land/trees/graves, 0 for farm, 1-4 for warriors/buildings
    """
    if resident == Resident.FARM.value:
        return 0
    if resident in [Resident.WARRIOR1.value, Resident.WARRIOR1_MOVED.value, Resident.CASTLE.value]:
        return 1
    if resident in [Resident.WARRIOR2.value, Resident.WARRIOR2_MOVED.value, Resident.TOWER.value]:
        return 2
    if resident in [Resident.WARRIOR3.value, Resident.WARRIOR3_MOVED.value, Resident.STRONG_TOWER.value]:
        return 3
    if resident in [Resident.WARRIOR4.value, Resident.WARRIOR4_MOVED.value]:
        return 4
    return -1


def is_warrior(resident: int) -> bool:
    """Check if resident is a warrior."""
    return Resident.WARRIOR1.value <= resident <= Resident.WARRIOR4_MOVED.value


def is_unmoved_warrior(resident: int) -> bool:
    """Check if resident is an unmoved warrior."""
    return Resident.WARRIOR1.value <= resident <= Resident.WARRIOR4.value


def is_building(resident: int) -> bool:
    """Check if resident is a building."""
    return Resident.FARM.value <= resident <= Resident.STRONG_TOWER.value


def is_tree(resident: int) -> bool:
    """Check if resident is a tree."""
    return resident in [Resident.PALM_TREE.value, Resident.PINE_TREE.value]


def is_empty(resident: int) -> bool:
    """Check if hex is empty."""
    return resident == Resident.EMPTY.value


def is_gravestone(resident: int) -> bool:
    """Check if hex is a gravestone."""
    return resident == Resident.GRAVESTONE.value


def is_castle(resident: int) -> bool:
    """Check if hex is a castle."""
    return resident == Resident.CASTLE.value


def is_water(resident: int) -> bool:
    """Check if hex is water."""
    return resident == Resident.WATER.value


class GameRules:
    """Validates game actions according to Antiyoy rules."""
    
    def __init__(self, board_state: Dict[Tuple[int, int], Dict], 
                 board_width: int = 20, board_height: int = 20,
                 player_money: Optional[Dict[int, int]] = None):
        """
        Initialize game rules validator.
        
        Args:
            board_state: Dict mapping (x,y) to {'owner_id': int, 'resident': str}
            board_width: Width of board
            board_height: Height of board
            player_money: Dict mapping player_id to money amount
        """
        self.board_state = board_state
        self.width = board_width
        self.height = board_height
        self.player_money = player_money or {}
    
    def _get_cell(self, x: int, y: int) -> Optional[Dict]:
        """Get cell at position, None if out of bounds."""
        if 0 <= x < self.width and 0 <= y < self.height:
            return self.board_state.get((x, y))
        return None
    
    def _resident_str_to_int(self, resident_str: str) -> int:
        """Convert resident string to enum int."""
        mapping = {
            'water': Resident.WATER.value,
            'empty': Resident.EMPTY.value,
            'warrior1': Resident.WARRIOR1.value,
            'warrior2': Resident.WARRIOR2.value,
            'warrior3': Resident.WARRIOR3.value,
            'warrior4': Resident.WARRIOR4.value,
            'warrior1_moved': Resident.WARRIOR1_MOVED.value,
            'warrior2_moved': Resident.WARRIOR2_MOVED.value,
            'warrior3_moved': Resident.WARRIOR3_MOVED.value,
            'warrior4_moved': Resident.WARRIOR4_MOVED.value,
            'farm': Resident.FARM.value,
            'castle': Resident.CASTLE.value,
            'tower': Resident.TOWER.value,
            'strong_tower': Resident.STRONG_TOWER.value,
            'palm_tree': Resident.PALM_TREE.value,
            'pine_tree': Resident.PINE_TREE.value,
            'gravestone': Resident.GRAVESTONE.value,
        }
        return mapping.get(resident_str.lower(), Resident.EMPTY.value)
    
    def get_province(self, x: int, y: int) -> Set[Tuple[int, int]]:
        """
        Get all connected hexes with same owner (BFS province finding).
        Implements neighbours() from board.cpp:514
        """
        cell = self._get_cell(x, y)
        if not cell or cell['owner_id'] == 0:
            return set()
        
        owner_id = cell['owner_id']
        visited = set()
        queue = [(x, y)]
        
        while queue:
            cx, cy = queue.pop(0)
            if (cx, cy) in visited:
                continue
            
            visited.add((cx, cy))
            
            for nx, ny in get_neighbors(cx, cy, self.width, self.height):
                if (nx, ny) not in visited:
                    neighbor = self._get_cell(nx, ny)
                    if neighbor and neighbor['owner_id'] == owner_id:
                        queue.append((nx, ny))
        
        return visited
    
    def find_castle_in_province(self, province: Set[Tuple[int, int]]) -> Optional[Tuple[int, int]]:
        """Find castle hex in a province."""
        for x, y in province:
            cell = self._get_cell(x, y)
            if cell and is_castle(self._resident_str_to_int(cell.get('resident', 'water'))):
                return (x, y)
        return None
    
    def get_unit_price(self, resident_type: int, player_id: int) -> int:
        """
        Calculate price for unit type (from board.cpp:34-45).
        Returns 0 if can't be purchased.
        """
        if is_unmoved_warrior(resident_type):
            return power(resident_type) * 10
        elif resident_type == Resident.FARM.value:
            # Farm price = 12 + 2 * number_of_farms
            farms_count = sum(1 for cell in self.board_state.values()
                            if cell['owner_id'] == player_id and 
                            self._resident_str_to_int(cell.get('resident', 'water')) == Resident.FARM.value)
            return 12 + 2 * farms_count
        elif resident_type == Resident.TOWER.value:
            return 15
        elif resident_type == Resident.STRONG_TOWER.value:
            return 35
        return 0
    
    def can_afford(self, player_id: int, resident_type: int) -> bool:
        """Check if player has enough money for unit."""
        if player_id not in self.player_money:
            return True  # Default to True if we don't track money
        
        price = self.get_unit_price(resident_type, player_id)
        return self.player_money[player_id] >= price
    
    def allows_warrior(self, x: int, y: int, warrior_type: int, owner_id: int) -> bool:
        """
        Check if warrior can enter this hex (from board.cpp:700-717).
        Implements combat rules.
        """
        if not is_unmoved_warrior(warrior_type):
            return False
        
        cell = self._get_cell(x, y)
        if not cell:
            return False
        
        resident = self._resident_str_to_int(cell.get('resident', 'water'))
        
        # Can't move onto water
        if is_water(resident):
            return False
        
        # Moving within own territory
        if cell['owner_id'] == owner_id:
            # Can merge with own warriors
            if is_warrior(resident):
                # Check if warriors can merge (sum <= 4)
                return power(warrior_type) + power(resident) <= 4
            # Can walk on own land, trees, graves
            return power(resident) < 0
        
        # Attacking enemy territory
        attacker_power = power(warrior_type)
        
        # Warrior4 can attack anything
        if attacker_power == 4:
            return True
        
        # Check this hex and its neighbors for blocking units
        neighbors = get_neighbors(x, y, self.width, self.height)
        for nx, ny in neighbors:
            neighbor = self._get_cell(nx, ny)
            if neighbor and neighbor['owner_id'] == cell['owner_id']:
                neighbor_resident = self._resident_str_to_int(neighbor.get('resident', 'water'))
                if power(neighbor_resident) >= attacker_power:
                    return False
        
        # Check the target hex itself
        if power(resident) >= attacker_power:
            return False
        
        return True
    
    def get_valid_placements(self, player_id: int, resident_type: int) -> List[Tuple[int, int]]:
        """
        Get valid hexes for placing unit - EXACTLY matches board.cpp Hexagon::place() logic.
        
        Args:
            player_id: Player making the placement
            resident_type: Unit type to place
            
        Returns:
            List of (x, y) coordinates where unit can be placed
        """
        valid = []
        
        # Find all player hexes
        player_hexes = [(x, y) for (x, y), cell in self.board_state.items()
                       if cell['owner_id'] == player_id]
        
        if not player_hexes:
            return []
        
        # Get a province with a castle (C++ line 912: castleHex = province(board)[0])
        castle_pos = None
        for px, py in player_hexes:
            province = self.get_province(px, py)
            castle_pos = self.find_castle_in_province(province)
            if castle_pos:
                break
        
        if not castle_pos:
            return []  # No castle, can't place anything (C++ line 913)
        
        # Get castle's province
        castle_province = self.get_province(castle_pos[0], castle_pos[1])
        
        # Check if player can afford this unit (C++ lines 914-917)
        price = self.get_unit_price(resident_type, player_id)
        if price == 0:
            return []  # Unit can't be bought (C++ line 915)
        
        if not self.can_afford(player_id, resident_type):
            return []  # Not enough money (C++ line 917)
        
        # Warriors: check combat rules (allows_warrior)
        if is_unmoved_warrior(resident_type):
            # Can place in province or attack adjacent enemies
            for x, y in castle_province:
                if self.allows_warrior(x, y, resident_type, player_id):
                    valid.append((x, y))
            
            # Also check border hexes (enemies adjacent to province)
            border = set()
            for px, py in castle_province:
                for nx, ny in get_neighbors(px, py, self.width, self.height):
                    neighbor = self._get_cell(nx, ny)
                    if neighbor and neighbor['owner_id'] != player_id:
                        if self.allows_warrior(nx, ny, resident_type, player_id):
                            border.add((nx, ny))
            valid.extend(list(border))
        
        # Farms: must be in castle's province, adjacent to castle or farm (C++ lines 956-973)
        elif resident_type == Resident.FARM.value:
            for x, y in castle_province:
                cell = self._get_cell(x, y)
                if not cell:
                    continue
                
                # Must be owned by player (C++ line 958)
                if cell['owner_id'] != player_id:
                    continue
                
                resident = self._resident_str_to_int(cell.get('resident', 'water'))
                
                # Must be empty or gravestone (C++ line 970)
                if not (is_empty(resident) or is_gravestone(resident)):
                    continue
                
                # Check if adjacent to castle or farm (C++ lines 959-969)
                neighbors = get_neighbors(x, y, self.width, self.height)
                adjacent_to_farm_or_castle = False
                for nx, ny in neighbors:
                    neighbor = self._get_cell(nx, ny)
                    if neighbor and neighbor['owner_id'] == player_id:
                        neighbor_resident = self._resident_str_to_int(neighbor.get('resident', 'water'))
                        if is_castle(neighbor_resident) or neighbor_resident == Resident.FARM.value:
                            adjacent_to_farm_or_castle = True
                            break
                
                if adjacent_to_farm_or_castle:
                    valid.append((x, y))
        
        # Towers: must be in castle's province (C++ lines 974-988)
        elif resident_type == Resident.TOWER.value or resident_type == Resident.STRONG_TOWER.value:
            for x, y in castle_province:
                cell = self._get_cell(x, y)
                if not cell:
                    continue
                
                # Must be owned by player (C++ line 976)
                if cell['owner_id'] != player_id:
                    continue
                
                resident = self._resident_str_to_int(cell.get('resident', 'water'))
                
                # Tower: empty or gravestone (C++ lines 978-980)
                if resident_type == Resident.TOWER.value:
                    if is_empty(resident) or is_gravestone(resident):
                        valid.append((x, y))
                
                # Strong Tower: empty, gravestone, or regular Tower (C++ lines 982-984)
                elif resident_type == Resident.STRONG_TOWER.value:
                    if is_empty(resident) or is_gravestone(resident) or resident == Resident.TOWER.value:
                        valid.append((x, y))
        
        return valid
    
    def get_valid_movements(self, x: int, y: int) -> List[Tuple[int, int]]:
        """
        Get valid movement destinations for warrior at (x, y).
        From board.cpp:991 - warriors can move up to 3 hexes within own territory.
        """
        cell = self._get_cell(x, y)
        if not cell:
            return []
        
        resident = self._resident_str_to_int(cell.get('resident', 'water'))
        if not is_warrior(resident):
            return []
        
        owner_id = cell['owner_id']
        valid = []
        
        # BFS with max depth 3 within own territory
        visited = {(x, y)}
        border = set()
        current_layer = [(x, y)]
        
        for depth in range(3):
            next_layer = []
            for cx, cy in current_layer:
                for nx, ny in get_neighbors(cx, cy, self.width, self.height):
                    if (nx, ny) in visited or (nx, ny) in border:
                        continue
                    
                    neighbor = self._get_cell(nx, ny)
                    if not neighbor:
                        continue
                    
                    # Can expand through own territory
                    if neighbor['owner_id'] == owner_id:
                        visited.add((nx, ny))
                        next_layer.append((nx, ny))
                    else:
                        # Enemy hex - check if we can attack
                        border.add((nx, ny))
            
            current_layer = next_layer
        
        # Check all visited and border hexes for valid targets
        for tx, ty in visited:
            if (tx, ty) != (x, y) and self.allows_warrior(tx, ty, resident, owner_id):
                valid.append((tx, ty))
        
        for tx, ty in border:
            if self.allows_warrior(tx, ty, resident, owner_id):
                valid.append((tx, ty))
        
        return valid
