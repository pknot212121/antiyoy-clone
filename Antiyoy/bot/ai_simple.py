"""
Simple AI implementation for Antiyoy.
Based on the Java AiEasy implementation.
"""

import random
import time
from typing import List, Optional, Tuple
from .game_utils import GameUtils, Resident, UNIT_PRICES
from .province import Province, ProvinceManager

# Delay between moves (in seconds) for visual feedback
MOVE_DELAY = 0.5


class SimpleAI:
    """
    A simple AI that:
    1. Moves existing units (attacks enemies or clears trees)
    2. Builds new units when affordable
    3. Ends turn
    """
    
    def __init__(self, board, my_player_id: int, action_builder, receive_func=None, debug: bool = True):
        self.board = board
        self.my_player_id = my_player_id
        self.action_builder = action_builder
        self.receive_func = receive_func
        self.debug = debug
        self.province_manager = ProvinceManager(board, my_player_id)
        self.failed_targets = set()  # Track hexes we failed to attack
    
    def log(self, msg: str):
        """Print debug message if debug is enabled."""
        if self.debug:
            print(f"[AI] {msg}")
    
    def make_move(self):
        """Execute the AI's turn."""
        self.log(f"Starting turn for player {self.my_player_id}")
        self.log(f"Found {len(self.province_manager.my_provinces)} provinces")
        
        # Phase 1: Move existing units
        self.move_units()
        
        # Phase 2: Spend money on new units
        self.spend_money()
        
        # Phase 3: End turn
        self.log("Ending turn")
        self.action_builder.add_end_turn()
    
    def move_units(self):
        """Move all unmoved warrior units."""
        for province in self.province_manager.my_provinces:
            movable_units = province.get_movable_units()
            self.log(f"Province has {len(movable_units)} movable units, {province.money} gold")
            
            for unit_hex in movable_units:
                self.decide_unit_action(unit_hex, province)
    
    def decide_unit_action(self, unit_hex, province: Province):
        """Decide what to do with a unit."""
        strength = GameUtils.get_warrior_strength(unit_hex.resident)
        self.log(f"Considering unit at ({unit_hex.x}, {unit_hex.y}) with strength {strength}")
        
        # Get possible moves, excluding failed targets
        attackable = self.province_manager.get_attackable_hexes(unit_hex, strength)
        attackable = [h for h in attackable if (h.x, h.y) not in self.failed_targets]
        
        if attackable:
            # Attack the most attractive hex
            target = self.find_best_attack_target(attackable, province)
            if target:
                self.log(f"  -> Attacking ({target.x}, {target.y})")
                if self.send_move(unit_hex, target):
                    return  # Success
                # If rejected, try another target or fall through to tree clearing
        
        # No attack possible - try to clear trees in our territory
        tree_target = self.find_tree_to_clear(unit_hex, province)
        if tree_target:
            self.log(f"  -> Clearing tree at ({tree_target.x}, {tree_target.y})")
            self.send_move(unit_hex, tree_target)
            return
        
        # Nothing to do, stay put
        self.log(f"  -> No action available")
    
    def find_best_attack_target(self, attackable: List, province: Province) -> Optional:
        """
        Find the most attractive hex to attack.
        Prefers hexes that are adjacent to more of our territory.
        Skips hexes we've already failed to attack.
        """
        if not attackable:
            return None
        
        best_target = None
        best_score = -1
        
        for h in attackable:
            # Skip hexes we've already failed to attack
            if (h.x, h.y) in self.failed_targets:
                continue
                
            score = self.get_attack_allure(h)
            
            # Bonus for attacking castles (will eliminate enemy province)
            if h.resident == Resident.Castle:
                score += 10
            # Bonus for attacking towers
            elif GameUtils.is_tower(h.resident):
                score += 5
            
            if score > best_score:
                best_score = score
                best_target = h
        
        return best_target
    
    def get_attack_allure(self, hex_obj) -> int:
        """
        Calculate how attractive a hex is to attack.
        More friendly neighbors = better (we gain territory that connects well).
        """
        score = 0
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, self.board.width, self.board.height)
        
        for nx, ny in neighbors:
            neighbor = self.board.get_hex(nx, ny)
            if neighbor and neighbor.owner_id == self.my_player_id:
                score += 1
        
        return score
    
    def find_tree_to_clear(self, unit_hex, province: Province) -> Optional:
        """Find a tree in our territory that we can clear."""
        neighbors = GameUtils.get_hex_neighbors(unit_hex.x, unit_hex.y, self.board.width, self.board.height)
        
        for nx, ny in neighbors:
            neighbor = self.board.get_hex(nx, ny)
            if neighbor and neighbor.owner_id == self.my_player_id:
                if GameUtils.is_tree(neighbor.resident):
                    return neighbor
        
        return None
    
    def spend_money(self):
        """Spend money on new units."""
        for province in self.province_manager.my_provinces:
            self.log(f"Province with castle at ({province.castle_hex.x if province.castle_hex else '?'}, {province.castle_hex.y if province.castle_hex else '?'}): {province.money} gold")
            
            # Try to build units - start with strength 1, increase if needed
            units_built = 0
            max_units_per_turn = 5  # Limit to prevent infinite loops
            
            while units_built < max_units_per_turn:
                built_any = False
                
                # Try each strength level
                for strength in range(1, 5):
                    if not province.can_afford_unit(strength):
                        continue
                    
                    result = self.try_build_unit(province, strength)
                    if result == "success":
                        # Update money after successful building
                        price = UNIT_PRICES[GameUtils.strength_to_warrior(strength)]
                        province.money -= price
                        units_built += 1
                        built_any = True
                        break  # Start over with strength 1
                    elif result == "no_targets":
                        # No valid targets left for any strength
                        break
                    # If "rejected", try next strength level
                
                if not built_any:
                    break  # No more units can be built
    
    def try_build_unit(self, province: Province, strength: int) -> str:
        """
        Try to build a unit of given strength.
        Returns: "success", "rejected", or "no_targets"
        """
        if not province.castle_hex:
            return "no_targets"
        
        # Get places where we can put a new unit
        placeable = self.province_manager.get_placeable_hexes(province, strength)
        
        # Filter out hexes we've already failed to attack
        placeable = [h for h in placeable if (h.x, h.y) not in self.failed_targets]
        
        # Filter to only attack positions (we want to expand, not just place units)
        attack_targets = [h for h in placeable if h.owner_id != self.my_player_id]
        
        if attack_targets:
            # Attack the best target
            target = self.find_best_attack_target(attack_targets, province)
            if target:
                resident = GameUtils.strength_to_warrior(strength)
                self.log(f"  -> Building {resident.name} and attacking ({target.x}, {target.y})")
                if self.send_place(resident, province.castle_hex, target):
                    return "success"
                else:
                    return "rejected"
        
        # If no attack targets, try placing inside province (to clear trees or build up forces)
        friendly_targets = [h for h in placeable if h.owner_id == self.my_player_id]
        if friendly_targets:
            # Prefer trees (to clear them) or border hexes
            tree_targets = [h for h in friendly_targets if GameUtils.is_tree(h.resident)]
            if tree_targets:
                target = random.choice(tree_targets)
            else:
                # Place on border hex for defense
                border = province.get_border_hexes(self.board)
                border_empty = [h for h in friendly_targets if h in border]
                if border_empty:
                    target = random.choice(border_empty)
                elif friendly_targets:
                    target = random.choice(friendly_targets)
                else:
                    return "no_targets"
            
            resident = GameUtils.strength_to_warrior(strength)
            self.log(f"  -> Building {resident.name} at ({target.x}, {target.y})")
            if self.send_place(resident, province.castle_hex, target):
                return "success"
            else:
                return "rejected"
        
        return "no_targets"
    
    def send_move(self, from_hex, to_hex) -> bool:
        """Send a move action. Returns True if approved."""
        self.action_builder.add_move(from_hex.x, from_hex.y, to_hex.x, to_hex.y)
        self.action_builder.send()
        
        # Wait for confirmation using passed-in function
        if self.receive_func:
            tag, payload = self.receive_func()
            if tag == 4:  # CONFIRMATION_SOCKET_TAG
                approved, awaiting = payload
                self.log(f"  Move {'approved' if approved else 'REJECTED'}")
                if not approved:
                    self.failed_targets.add((to_hex.x, to_hex.y))
                else:
                    time.sleep(MOVE_DELAY)  # Visual delay on success
                return approved
        return True  # Assume success if no receive function
        
    def send_place(self, resident: Resident, from_hex, to_hex) -> bool:
        """Send a place action. Returns True if approved."""
        self.action_builder.add_place(int(resident), from_hex.x, from_hex.y, to_hex.x, to_hex.y)
        self.action_builder.send()
        
        # Wait for confirmation using passed-in function
        if self.receive_func:
            tag, payload = self.receive_func()
            if tag == 4:  # CONFIRMATION_SOCKET_TAG
                approved, awaiting = payload
                self.log(f"  Place {'approved' if approved else 'REJECTED'}")
                if not approved:
                    self.failed_targets.add((to_hex.x, to_hex.y))
                else:
                    time.sleep(MOVE_DELAY)  # Visual delay on success
                return approved
        return True  # Assume success if no receive function

