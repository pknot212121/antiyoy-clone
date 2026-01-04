"""
Game utilities for hexagon math and board operations.
"""

from enum import IntEnum
from typing import List, Optional, Tuple, Set


class Resident(IntEnum):
    """Resident types on hexagons - must match C++ enum exactly."""
    Water = 0
    Empty = 1
    
    Warrior1 = 2
    Warrior2 = 3
    Warrior3 = 4
    Warrior4 = 5
    
    Warrior1Moved = 6
    Warrior2Moved = 7
    Warrior3Moved = 8
    Warrior4Moved = 9
    
    Farm = 10
    Castle = 11
    Tower = 12
    StrongTower = 13
    
    PalmTree = 14
    PineTree = 15
    Gravestone = 16


# Unit costs and incomes (matches C++ incomeBoard)
INCOME_TABLE = {
    Resident.Water: 0,
    Resident.Empty: 0,  # +1 is implicit for each owned hex
    Resident.Warrior1: -2,
    Resident.Warrior2: -6,
    Resident.Warrior3: -18,
    Resident.Warrior4: -36,
    Resident.Warrior1Moved: -2,
    Resident.Warrior2Moved: -6,
    Resident.Warrior3Moved: -18,
    Resident.Warrior4Moved: -36,
    Resident.Farm: 4,
    Resident.Castle: 0,
    Resident.Tower: -1,
    Resident.StrongTower: -6,
    Resident.PalmTree: -1,
    Resident.PineTree: -1,
    Resident.Gravestone: 0,
}

# Unit purchase prices
UNIT_PRICES = {
    Resident.Warrior1: 10,
    Resident.Warrior2: 20,
    Resident.Warrior3: 30,
    Resident.Warrior4: 40,
    Resident.Farm: 12,
    Resident.Tower: 15,
    Resident.StrongTower: 35,
}

# Unit strengths
UNIT_STRENGTH = {
    Resident.Warrior1: 1,
    Resident.Warrior2: 2,
    Resident.Warrior3: 3,
    Resident.Warrior4: 4,
    Resident.Warrior1Moved: 1,
    Resident.Warrior2Moved: 2,
    Resident.Warrior3Moved: 3,
    Resident.Warrior4Moved: 4,
}


class GameUtils:
    """Utility class for game operations."""
    
    @staticmethod
    def is_water(resident: Resident) -> bool:
        return resident == Resident.Water
    
    @staticmethod
    def is_empty(resident: Resident) -> bool:
        return resident == Resident.Empty
    
    @staticmethod
    def is_warrior(resident: Resident) -> bool:
        return Resident.Warrior1 <= resident <= Resident.Warrior4Moved
    
    @staticmethod
    def is_unmoved_warrior(resident: Resident) -> bool:
        return Resident.Warrior1 <= resident <= Resident.Warrior4
    
    @staticmethod
    def is_moved_warrior(resident: Resident) -> bool:
        return Resident.Warrior1Moved <= resident <= Resident.Warrior4Moved
    
    @staticmethod
    def is_farm(resident: Resident) -> bool:
        return resident == Resident.Farm
    
    @staticmethod
    def is_castle(resident: Resident) -> bool:
        return resident == Resident.Castle
    
    @staticmethod
    def is_tower(resident: Resident) -> bool:
        return resident in (Resident.Tower, Resident.StrongTower)
    
    @staticmethod
    def is_tree(resident: Resident) -> bool:
        return resident in (Resident.PalmTree, Resident.PineTree)
    
    @staticmethod
    def is_palm(resident: Resident) -> bool:
        return resident == Resident.PalmTree
    
    @staticmethod
    def is_pine(resident: Resident) -> bool:
        return resident == Resident.PineTree
    
    @staticmethod
    def is_gravestone(resident: Resident) -> bool:
        return resident == Resident.Gravestone
    
    @staticmethod
    def is_building(resident: Resident) -> bool:
        return resident in (Resident.Castle, Resident.Tower, Resident.StrongTower, Resident.Farm)
    
    @staticmethod
    def get_warrior_strength(resident: Resident) -> int:
        """Get the strength of a warrior unit."""
        return UNIT_STRENGTH.get(resident, 0)
    
    @staticmethod
    def strength_to_warrior(strength: int) -> Resident:
        """Convert strength (1-4) to warrior resident type."""
        mapping = {
            1: Resident.Warrior1,
            2: Resident.Warrior2,
            3: Resident.Warrior3,
            4: Resident.Warrior4,
        }
        return mapping.get(strength, Resident.Empty)
    
    @staticmethod
    def get_hex_neighbors(x: int, y: int, width: int, height: int) -> List[Tuple[int, int]]:
        """
        Get valid neighbor coordinates for a hex.
        Hexagonal grid uses offset coordinates (odd-q / column-based).
        Must match C++ board.cpp evenDirections/oddDirections.
        """
        neighbors = []
        
        # Different offsets based on COLUMN (x), not row - must match C++!
        if x % 2 == 0:  # Even column
            offsets = [
                ( 0, -1),  # top
                (-1, -1),  # left-top  
                (-1,  0),  # left-bottom
                ( 0,  1),  # bottom
                ( 1,  0),  # right-bottom
                ( 1, -1),  # right-top
            ]
        else:  # Odd column
            offsets = [
                ( 0, -1),  # top
                (-1,  0),  # left-top
                (-1,  1),  # left-bottom
                ( 0,  1),  # bottom
                ( 1,  1),  # right-bottom
                ( 1,  0),  # right-top
            ]
        
        for dx, dy in offsets:
            nx, ny = x + dx, y + dy
            if 0 <= nx < width and 0 <= ny < height:
                neighbors.append((nx, ny))
        
        return neighbors
    
    @staticmethod
    def get_defense_level(hex_obj, board) -> int:
        """
        Calculate the defense level of a hex.
        - Empty hex: 0
        - Warrior: strength
        - Tower: 2
        - Strong Tower: 3
        - Castle: 1
        - Adjacent tower/castle adds to defense
        """
        resident = hex_obj.resident
        
        if GameUtils.is_water(resident):
            return 999  # Can't attack water
        
        defense = 0
        
        if GameUtils.is_warrior(resident):
            defense = max(defense, GameUtils.get_warrior_strength(resident))
        elif resident == Resident.Tower:
            defense = max(defense, 2)
        elif resident == Resident.StrongTower:
            defense = max(defense, 3)
        elif resident == Resident.Castle:
            defense = max(defense, 1)
        
        # Check for adjacent tower/castle protection (same owner provides defense)
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, board.width, board.height)
        for nx, ny in neighbors:
            neighbor = board.get_hex(nx, ny)
            if neighbor and neighbor.owner_id == hex_obj.owner_id:
                if neighbor.resident == Resident.Tower:
                    defense = max(defense, 2)
                elif neighbor.resident == Resident.StrongTower:
                    defense = max(defense, 3)
                elif neighbor.resident == Resident.Castle:
                    defense = max(defense, 1)  # Castle also provides level 1 protection
        
        return defense
    
    @staticmethod
    def can_attack(attacker_strength: int, target_hex, board) -> bool:
        """Check if an attacker with given strength can attack a target hex."""
        defense = GameUtils.get_defense_level(target_hex, board)
        return attacker_strength > defense
    
    @staticmethod
    def calculate_province_income(hexes: List, board) -> int:
        """Calculate the income for a province (list of hexes)."""
        income = 0
        for h in hexes:
            # +1 for each owned land hex
            if not GameUtils.is_water(h.resident):
                income += 1
            # Add/subtract resident income
            income += INCOME_TABLE.get(h.resident, 0)
        return income
    
    @staticmethod
    def get_unit_upkeep(strength: int) -> int:
        """Get the upkeep cost for a unit of given strength."""
        upkeep_map = {1: 2, 2: 6, 3: 18, 4: 36}
        return upkeep_map.get(strength, 0)
    
    @staticmethod
    def get_merged_strength(s1: int, s2: int) -> int:
        """Get the strength of a merged unit. Returns 0 if can't merge."""
        if s1 == s2 and s1 < 4:
            return s1 + 1
        return 0
    
    @staticmethod
    def hex_is_free(resident: Resident) -> bool:
        """Check if a hex can have something placed on it."""
        return resident == Resident.Empty or resident == Resident.Gravestone
    
    @staticmethod
    def get_tower_defense(resident: Resident) -> int:
        """Get the defense level provided by a tower/castle."""
        if resident == Resident.Tower:
            return 2
        elif resident == Resident.StrongTower:
            return 3
        elif resident == Resident.Castle:
            return 1
        return 0

