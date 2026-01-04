"""
Province detection and management.
A province is a connected group of hexes owned by the same player.
"""

from typing import List, Optional, Set, Tuple, Dict
from .game_utils import GameUtils, Resident, UNIT_PRICES, INCOME_TABLE, UNIT_STRENGTH


class Province:
    """Represents a connected territory owned by a player."""
    
    def __init__(self, owner_id: int):
        self.owner_id = owner_id
        self.hexes: List = []
        self.castle_hex = None
        self.money = 0
    
    def add_hex(self, hex_obj):
        """Add a hex to this province."""
        self.hexes.append(hex_obj)
        if hex_obj.resident == Resident.Castle:
            self.castle_hex = hex_obj
            self.money = hex_obj.money
    
    def get_income(self) -> int:
        """Calculate the income for this province."""
        income = 0
        for h in self.hexes:
            # +1 for each land hex
            if not GameUtils.is_water(h.resident):
                income += 1
            # Add resident income/upkeep
            income += INCOME_TABLE.get(h.resident, 0)
        return income
    
    def can_afford(self, resident: Resident) -> bool:
        """Check if province can afford to build a unit/building."""
        price = UNIT_PRICES.get(resident, 999)
        return self.money >= price
    
    def can_afford_unit(self, strength: int) -> bool:
        """Check if province can afford a unit of given strength."""
        resident = GameUtils.strength_to_warrior(strength)
        return self.can_afford(resident)
    
    def get_movable_units(self) -> List:
        """Get all unmoved warriors in this province."""
        units = []
        for h in self.hexes:
            if GameUtils.is_unmoved_warrior(h.resident):
                units.append(h)
        return units
    
    def get_empty_hexes(self) -> List:
        """Get all empty hexes in this province."""
        return [h for h in self.hexes if h.resident == Resident.Empty]
    
    def get_border_hexes(self, board) -> List:
        """Get hexes that border enemy or neutral territory."""
        border = []
        for h in self.hexes:
            neighbors = GameUtils.get_hex_neighbors(h.x, h.y, board.width, board.height)
            for nx, ny in neighbors:
                neighbor = board.get_hex(nx, ny)
                if neighbor and neighbor.owner_id != self.owner_id and not GameUtils.is_water(neighbor.resident):
                    border.append(h)
                    break
        return border
    
    def get_palms(self) -> List:
        """Get all palm trees in this province (they cost money!)."""
        return [h for h in self.hexes if GameUtils.is_palm(h.resident)]
    
    def get_trees(self) -> List:
        """Get all trees in this province."""
        return [h for h in self.hexes if GameUtils.is_tree(h.resident)]
    
    def can_afford_with_upkeep(self, strength: int) -> bool:
        """Check if province can afford a unit AND its ongoing upkeep."""
        resident = GameUtils.strength_to_warrior(strength)
        price = UNIT_PRICES.get(resident, 999)
        if self.money < price:
            return False
        
        # Check if income can support new unit
        new_upkeep = GameUtils.get_unit_upkeep(strength)
        future_income = self.get_income() - new_upkeep
        # Allow slightly negative income (-2) like expert AI
        return future_income >= -2
    
    def get_hexes_needing_tower(self, board) -> List:
        """Find hexes where a tower would defend 5+ hexes."""
        candidates = []
        for h in self.hexes:
            if not GameUtils.hex_is_free(h.resident):
                continue
            
            defense_gain = self._calculate_tower_defense_gain(h, board)
            if defense_gain >= 5:
                candidates.append((h, defense_gain))
        
        # Sort by defense gain (best first)
        candidates.sort(key=lambda x: -x[1])
        return [h for h, _ in candidates]
    
    def _calculate_tower_defense_gain(self, hex_obj, board) -> int:
        """Calculate how many hexes would gain defense from a tower here."""
        gain = 0
        
        # The hex itself
        if not self._is_defended_by_tower(hex_obj, board):
            gain += 1
        
        # Check neighbors
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, board.width, board.height)
        for nx, ny in neighbors:
            neighbor = board.get_hex(nx, ny)
            if neighbor and neighbor.owner_id == self.owner_id:
                if not self._is_defended_by_tower(neighbor, board):
                    gain += 1
                if GameUtils.is_tower(neighbor.resident):
                    gain -= 1  # Penalty for adjacent tower
        
        return gain
    
    def _is_defended_by_tower(self, hex_obj, board) -> bool:
        """Check if a hex is defended by an adjacent tower/castle."""
        if GameUtils.is_tower(hex_obj.resident) or GameUtils.is_castle(hex_obj.resident):
            return True
        
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, board.width, board.height)
        for nx, ny in neighbors:
            neighbor = board.get_hex(nx, ny)
            if neighbor and neighbor.owner_id == self.owner_id:
                if GameUtils.is_tower(neighbor.resident) or GameUtils.is_castle(neighbor.resident):
                    return True
        return False
    
    def get_farm_locations(self, board) -> List:
        """Get good locations for farms (adjacent to castle or other farms)."""
        candidates = []
        for h in self.hexes:
            if not GameUtils.hex_is_free(h.resident):
                continue
            
            # Check if adjacent to castle or farm
            neighbors = GameUtils.get_hex_neighbors(h.x, h.y, board.width, board.height)
            for nx, ny in neighbors:
                neighbor = board.get_hex(nx, ny)
                if neighbor and neighbor.owner_id == self.owner_id:
                    if GameUtils.is_castle(neighbor.resident) or GameUtils.is_farm(neighbor.resident):
                        candidates.append(h)
                        break
        
        return candidates
    
    def count_units(self) -> int:
        """Count warrior units in this province."""
        return sum(1 for h in self.hexes if GameUtils.is_warrior(h.resident))
    
    def __repr__(self):
        return f"Province(owner={self.owner_id}, hexes={len(self.hexes)}, money={self.money})"


class ProvinceManager:
    """Manages province detection and operations."""
    
    def __init__(self, board, my_player_id: int):
        self.board = board
        self.my_player_id = my_player_id
        self.my_provinces: List[Province] = []
        self.enemy_provinces: List[Province] = []
        self._detect_provinces()
    
    def _detect_provinces(self):
        """Detect all provinces on the board using flood fill."""
        visited: Set[Tuple[int, int]] = set()
        
        for y in range(self.board.height):
            for x in range(self.board.width):
                hex_obj = self.board.get_hex(x, y)
                if (x, y) in visited:
                    continue
                if hex_obj.owner_id == 0:  # No owner (water or neutral)
                    visited.add((x, y))
                    continue
                if GameUtils.is_water(hex_obj.resident):
                    visited.add((x, y))
                    continue
                
                # Start a new province
                province = Province(hex_obj.owner_id)
                self._flood_fill(x, y, hex_obj.owner_id, province, visited)
                
                if province.hexes:
                    if province.owner_id == self.my_player_id:
                        self.my_provinces.append(province)
                    else:
                        self.enemy_provinces.append(province)
    
    def _flood_fill(self, x: int, y: int, owner_id: int, province: Province, visited: Set[Tuple[int, int]]):
        """Flood fill to find connected hexes of the same owner."""
        if (x, y) in visited:
            return
        
        hex_obj = self.board.get_hex(x, y)
        if hex_obj is None:
            return
        if hex_obj.owner_id != owner_id:
            return
        if GameUtils.is_water(hex_obj.resident):
            return
        
        visited.add((x, y))
        province.add_hex(hex_obj)
        
        # Check neighbors
        neighbors = GameUtils.get_hex_neighbors(x, y, self.board.width, self.board.height)
        for nx, ny in neighbors:
            self._flood_fill(nx, ny, owner_id, province, visited)
    
    def get_attackable_hexes(self, from_hex, strength: int) -> List:
        """
        Get hexes that can be attacked from a position with given strength.
        Returns enemy/neutral hexes adjacent to our territory that we can defeat.
        """
        attackable = []
        
        # Get all hexes reachable with this strength
        reachable = self._get_move_zone(from_hex, strength)
        
        for h in reachable:
            # Skip our own hexes
            if h.owner_id == self.my_player_id:
                continue
            # Skip water
            if GameUtils.is_water(h.resident):
                continue
            # Check if we can defeat the defense
            if GameUtils.can_attack(strength, h, self.board):
                attackable.append(h)
        
        return attackable
    
    def _get_move_zone(self, from_hex, strength: int) -> List:
        """
        Get all hexes a unit can move to.
        Units can move to adjacent hexes they own, or attack adjacent enemy hexes.
        """
        reachable = []
        visited = set()
        
        # BFS from starting position through owned territory
        queue = [(from_hex.x, from_hex.y, 0)]  # x, y, distance
        visited.add((from_hex.x, from_hex.y))
        
        while queue:
            x, y, dist = queue.pop(0)
            
            neighbors = GameUtils.get_hex_neighbors(x, y, self.board.width, self.board.height)
            for nx, ny in neighbors:
                if (nx, ny) in visited:
                    continue
                
                neighbor = self.board.get_hex(nx, ny)
                if neighbor is None:
                    continue
                
                visited.add((nx, ny))
                
                # Can always reach adjacent hexes
                reachable.append(neighbor)
                
                # Continue BFS through our own territory (for multi-hop moves)
                if neighbor.owner_id == self.my_player_id and not GameUtils.is_water(neighbor.resident):
                    # Continue through friendly territory
                    queue.append((nx, ny, dist + 1))
        
        return reachable
    
    def get_placeable_hexes(self, province: Province, strength: int) -> List:
        """
        Get hexes where we can place a new unit from the castle.
        Must be reachable from castle and either empty friendly hex or attackable enemy hex.
        """
        if not province.castle_hex:
            return []
        
        placeable = []
        reachable = self._get_move_zone(province.castle_hex, strength)
        
        for h in reachable:
            if h.owner_id == self.my_player_id:
                # Can place on empty friendly hex or tree
                if h.resident == Resident.Empty or GameUtils.is_tree(h.resident):
                    placeable.append(h)
            else:
                # Can attack enemy hex if strong enough
                if not GameUtils.is_water(h.resident) and GameUtils.can_attack(strength, h, self.board):
                    placeable.append(h)
        
        return placeable

