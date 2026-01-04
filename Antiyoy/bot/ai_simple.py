"""
Expert AI implementation for Antiyoy.
Based on the Java AiMaster with DefenseManager and AttackManager.

Key strategies:
1. DETECT THREATS - identify enemy units adjacent to our territory
2. DEFEND FIRST - when under attack, prioritize defense
3. EXPAND EARLY - grab neutral land with cheap units before farms
4. PROTECT FRONT LINE - pull units to perimeter, build towers
5. ATTACK STRATEGICALLY - target farms and weak points
"""

import random
import time
from typing import List, Optional, Tuple, Set, Dict
from .game_utils import GameUtils, Resident, UNIT_PRICES, INCOME_TABLE
from .province import Province, ProvinceManager

# Delay between moves (in seconds) for visual feedback
MOVE_DELAY = 0.5


class ImprovedAI:
    """Expert AI with threat detection and active defense."""
    
    def __init__(self, board, my_player_id: int, action_builder, receive_func=None, debug: bool = True):
        self.board = board
        self.my_player_id = my_player_id
        self.action_builder = action_builder
        self.receive_func = receive_func
        self.debug = debug
        self.province_manager = ProvinceManager(board, my_player_id)
        self.failed_targets: Set[Tuple[int, int]] = set()
        self.units_built_this_turn = 0
        
        # Threat tracking
        self.enemy_threats: List = []  # Enemy units adjacent to our territory
        self.threat_level = 0  # 0 = safe, higher = more danger
        self.front_line: List = []  # Our hexes adjacent to enemies
        self.neutral_lands: List = []  # Neutral hexes we can grab
    
    def log(self, msg: str):
        if self.debug:
            print(f"[AI] {msg}")
    
    def make_move(self):
        """Execute the AI's turn with threat-aware decision making."""
        self.log(f"=== Turn for player {self.my_player_id} ===")
        self.units_built_this_turn = 0
        
        for province in self.province_manager.my_provinces:
            self.log(f"Province: {len(province.hexes)} hexes, {province.money}g, income {province.get_income()}")
            
            # Phase 0: Analyze the situation
            self.analyze_situation(province)
            
            # Phase 1: DEFEND if under threat
            if self.threat_level > 0:
                self.defend_province(province)
            
            # Phase 2: Move existing units (expand, attack, clear trees)
            self.move_units(province)
            
            # Phase 3: Spend money based on situation
            self.spend_money(province)
            
            # Phase 4: Pull idle units to front line
            self.pull_units_to_front_line(province)
        
        self.log("Ending turn")
        self.action_builder.add_end_turn()
    
    # ==================== SITUATION ANALYSIS ====================
    
    def analyze_situation(self, province: Province):
        """Analyze threats and opportunities."""
        self.enemy_threats = []
        self.front_line = []
        self.neutral_lands = []
        self.threat_level = 0
        
        # Find front line hexes and threats
        for h in province.hexes:
            neighbors = GameUtils.get_hex_neighbors(h.x, h.y, self.board.width, self.board.height)
            is_front_line = False
            
            for nx, ny in neighbors:
                neighbor = self.board.get_hex(nx, ny)
                if not neighbor or GameUtils.is_water(neighbor.resident):
                    continue
                
                if neighbor.owner_id == 0:  # Neutral
                    if neighbor not in self.neutral_lands:
                        self.neutral_lands.append(neighbor)
                elif neighbor.owner_id != self.my_player_id:  # Enemy
                    is_front_line = True
                    
                    # Check for enemy units (threats!)
                    if GameUtils.is_warrior(neighbor.resident):
                        strength = GameUtils.get_warrior_strength(neighbor.resident)
                        self.enemy_threats.append((neighbor, strength))
                        self.threat_level += strength
            
            if is_front_line:
                self.front_line.append(h)
        
        self.log(f"  Front line: {len(self.front_line)} hexes, Threats: {len(self.enemy_threats)}, Level: {self.threat_level}")
        self.log(f"  Neutral lands nearby: {len(self.neutral_lands)}")
    
    # ==================== DEFENSE ====================
    
    def defend_province(self, province: Province):
        """Actively defend against enemy threats."""
        if not self.enemy_threats:
            return
        
        self.log(f"  DEFENDING against {len(self.enemy_threats)} threats")
        
        # Sort threats by danger (highest first)
        self.enemy_threats.sort(key=lambda x: -x[1])
        
        for threat_hex, threat_strength in self.enemy_threats:
            # Find our hexes adjacent to this threat
            our_hexes_in_danger = self.get_our_hexes_adjacent_to(threat_hex)
            
            if not our_hexes_in_danger:
                continue
            
            # Check if we have defense
            best_defense = self.get_best_defense_at(our_hexes_in_danger)
            
            if best_defense >= threat_strength:
                continue  # We're safe
            
            self.log(f"    Threat at ({threat_hex.x},{threat_hex.y}) str={threat_strength}, our defense={best_defense}")
            
            # Try to counter the threat
            self.counter_threat(province, threat_hex, threat_strength, our_hexes_in_danger)
    
    def get_our_hexes_adjacent_to(self, hex_obj) -> List:
        """Get our hexes adjacent to a given hex."""
        result = []
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, self.board.width, self.board.height)
        for nx, ny in neighbors:
            neighbor = self.board.get_hex(nx, ny)
            if neighbor and neighbor.owner_id == self.my_player_id:
                result.append(neighbor)
        return result
    
    def get_best_defense_at(self, hexes: List) -> int:
        """Get the best defense level among given hexes."""
        best = 0
        for h in hexes:
            defense = GameUtils.get_defense_level(h, self.board)
            if defense > best:
                best = defense
        return best
    
    def counter_threat(self, province: Province, threat_hex, threat_strength: int, danger_hexes: List):
        """Counter an enemy threat."""
        # Strategy 1: Attack the threat if we can
        if self.try_attack_threat(province, threat_hex, threat_strength):
            return
        
        # Strategy 2: Build a unit strong enough to defend
        if self.try_build_defender(province, threat_strength, danger_hexes):
            return
        
        # Strategy 3: Build a tower for defense
        if self.try_build_defensive_tower(province, danger_hexes):
            return
        
        # Strategy 4: Move existing units to defend
        self.move_units_to_defend(province, danger_hexes)
    
    def try_attack_threat(self, province: Province, threat_hex, threat_strength: int) -> bool:
        """Try to attack and eliminate the threat."""
        # Find units that can attack this threat
        for h in province.hexes:
            if not GameUtils.is_unmoved_warrior(h.resident):
                continue
            
            our_strength = GameUtils.get_warrior_strength(h.resident)
            if our_strength <= threat_strength:
                continue  # Can't beat it
            
            # Check if we can reach the threat
            reachable = self.province_manager._get_move_zone(h, our_strength)
            if threat_hex in reachable:
                self.log(f"    -> Counter-attacking threat at ({threat_hex.x},{threat_hex.y})")
                if self.send_move(h, threat_hex):
                    return True
        
        return False
    
    def try_build_defender(self, province: Province, threat_strength: int, danger_hexes: List) -> bool:
        """Build a unit to defend against the threat."""
        needed_strength = threat_strength + 1
        if needed_strength > 4:
            needed_strength = 4
        
        if not province.can_afford_unit(needed_strength):
            # Try a cheaper option
            for s in range(needed_strength - 1, 0, -1):
                if province.can_afford_unit(s):
                    needed_strength = s
                    break
            else:
                return False
        
        # Find a good hex to place the defender (adjacent to danger)
        for dh in danger_hexes:
            if GameUtils.hex_is_free(dh.resident):
                resident = GameUtils.strength_to_warrior(needed_strength)
                self.log(f"    -> Building {resident.name} to defend at ({dh.x},{dh.y})")
                if self.send_place(resident, province.castle_hex, dh):
                    province.money -= UNIT_PRICES[resident]
                    return True
        
        return False
    
    def try_build_defensive_tower(self, province: Province, danger_hexes: List) -> bool:
        """Build a tower to defend."""
        if province.money < UNIT_PRICES[Resident.Tower]:
            return False
        
        for dh in danger_hexes:
            if GameUtils.hex_is_free(dh.resident):
                self.log(f"    -> Building defensive Tower at ({dh.x},{dh.y})")
                if self.send_place(Resident.Tower, province.castle_hex, dh):
                    province.money -= UNIT_PRICES[Resident.Tower]
                    return True
        
        return False
    
    def move_units_to_defend(self, province: Province, danger_hexes: List):
        """Move existing units closer to danger zones."""
        movable = province.get_movable_units()
        
        for unit_hex in movable:
            # Find a danger hex we can move to
            reachable = self.province_manager._get_move_zone(unit_hex, 1)
            
            for dh in danger_hexes:
                if dh in reachable and GameUtils.hex_is_free(dh.resident):
                    self.log(f"    -> Moving unit to defend at ({dh.x},{dh.y})")
                    self.send_move(unit_hex, dh)
                    break
    
    # ==================== UNIT MOVEMENT ====================
    
    def move_units(self, province: Province):
        """Move all unmoved units."""
        movable = province.get_movable_units()
        self.log(f"  Moving {len(movable)} units")
        
        for unit_hex in movable:
            self.decide_unit_action(unit_hex, province)
    
    def decide_unit_action(self, unit_hex, province: Province):
        """Decide what to do with a unit."""
        strength = GameUtils.get_warrior_strength(unit_hex.resident)
        
        # Priority 1: Clear palm trees (they cost money)
        if strength <= 2:
            palm = self.find_palm_to_clear(unit_hex)
            if palm:
                self.log(f"    Unit ({unit_hex.x},{unit_hex.y}) clearing palm")
                if self.send_move(unit_hex, palm):
                    return
        
        # Priority 2: Attack enemies (prefer safe attacks)
        attackable = self.province_manager.get_attackable_hexes(unit_hex, strength)
        attackable = [h for h in attackable if (h.x, h.y) not in self.failed_targets]
        
        if attackable:
            target = self.find_best_attack_target(attackable, strength)
            if target:
                self.log(f"    Unit ({unit_hex.x},{unit_hex.y}) attacking ({target.x},{target.y})")
                if self.send_move(unit_hex, target):
                    return
        
        # Priority 3: Clear other trees
        tree = self.find_tree_to_clear(unit_hex)
        if tree:
            self.log(f"    Unit ({unit_hex.x},{unit_hex.y}) clearing tree")
            if self.send_move(unit_hex, tree):
                return
    
    def find_palm_to_clear(self, unit_hex) -> Optional:
        """Find a palm tree to clear."""
        reachable = self.province_manager._get_move_zone(unit_hex, 1)
        for h in reachable:
            if h.owner_id == self.my_player_id and GameUtils.is_palm(h.resident):
                return h
        return None
    
    def find_tree_to_clear(self, unit_hex) -> Optional:
        """Find any tree to clear."""
        reachable = self.province_manager._get_move_zone(unit_hex, 1)
        for h in reachable:
            if h.owner_id == self.my_player_id and GameUtils.is_tree(h.resident):
                return h
        return None
    
    def find_best_attack_target(self, attackable: List, strength: int) -> Optional:
        """Find the best hex to attack."""
        if not attackable:
            return None
        
        best = None
        best_score = -999
        
        for h in attackable:
            if (h.x, h.y) in self.failed_targets:
                continue
            
            score = 0
            
            # High priority: enemy farms (hurt their income)
            if h.resident == Resident.Farm:
                score += 50
            
            # Very high priority: enemy castles
            if h.resident == Resident.Castle:
                score += 100
            
            # Priority: enemy units (remove threats)
            if GameUtils.is_warrior(h.resident):
                enemy_strength = GameUtils.get_warrior_strength(h.resident)
                score += enemy_strength * 10
            
            # Bonus for hexes with many friendly neighbors (easy to hold)
            friendly = self.count_friendly_neighbors(h)
            score += friendly * 5
            
            # Penalty for expensive units attacking exposed positions
            if strength >= 3 and friendly < 2:
                score -= 20
            
            if score > best_score:
                best_score = score
                best = h
        
        return best
    
    def count_friendly_neighbors(self, hex_obj) -> int:
        """Count neighbors owned by us."""
        count = 0
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, self.board.width, self.board.height)
        for nx, ny in neighbors:
            n = self.board.get_hex(nx, ny)
            if n and n.owner_id == self.my_player_id:
                count += 1
        return count
    
    # ==================== SPENDING MONEY ====================
    
    def spend_money(self, province: Province):
        """Spend money based on game situation."""
        income = province.get_income()
        
        # EARLY GAME: Expand with cheap units if neutral lands available
        if len(self.neutral_lands) > 3:
            self.log(f"  Early expansion phase ({len(self.neutral_lands)} neutral hexes)")
            self.expand_with_cheap_units(province)
        
        # If under threat or front line not defended, build towers
        if self.threat_level > 0 or self.has_undefended_front_line(province):
            self.build_defensive_towers(province)
        
        # Build farms only when safe and income is low
        if self.threat_level == 0 and income < 10:
            self.try_build_farms(province)
        
        # Build attack units
        self.build_attack_units(province)
    
    def expand_with_cheap_units(self, province: Province):
        """Expand by grabbing neutral territory with cheap units."""
        while province.can_afford_unit(1) and self.units_built_this_turn < 10:
            # Find attackable neutral hexes
            if not province.castle_hex:
                break
            
            placeable = self.province_manager.get_placeable_hexes(province, 1)
            neutral_targets = [h for h in placeable if h.owner_id == 0]
            
            if not neutral_targets:
                break
            
            # Prefer hexes with many friendly neighbors
            neutral_targets.sort(key=lambda h: -self.count_friendly_neighbors(h))
            target = neutral_targets[0]
            
            self.log(f"    -> Expanding to ({target.x},{target.y})")
            if self.send_place(Resident.Warrior1, province.castle_hex, target):
                province.money -= UNIT_PRICES[Resident.Warrior1]
                self.units_built_this_turn += 1
            else:
                break
    
    def has_undefended_front_line(self, province: Province) -> bool:
        """Check if front line has undefended hexes."""
        for h in self.front_line:
            defense = GameUtils.get_defense_level(h, self.board)
            if defense == 0:
                return True
        return False
    
    def build_defensive_towers(self, province: Province):
        """Build towers on the front line."""
        while province.money >= UNIT_PRICES[Resident.Tower]:
            # Find front line hex that needs a tower
            best = None
            best_gain = 0
            
            for h in self.front_line:
                if not GameUtils.hex_is_free(h.resident):
                    continue
                if self.is_defended_by_tower(h):
                    continue
                
                # Calculate defense gain
                gain = 1  # The hex itself
                neighbors = GameUtils.get_hex_neighbors(h.x, h.y, self.board.width, self.board.height)
                for nx, ny in neighbors:
                    n = self.board.get_hex(nx, ny)
                    if n and n.owner_id == self.my_player_id:
                        if not self.is_defended_by_tower(n):
                            gain += 1
                
                if gain > best_gain:
                    best_gain = gain
                    best = h
            
            if best is None or best_gain < 3:
                break
            
            self.log(f"    -> Building Tower at ({best.x},{best.y}) (gain={best_gain})")
            if self.send_place(Resident.Tower, province.castle_hex, best):
                province.money -= UNIT_PRICES[Resident.Tower]
            else:
                break
    
    def is_defended_by_tower(self, hex_obj) -> bool:
        """Check if hex is defended by adjacent tower."""
        if GameUtils.is_tower(hex_obj.resident) or hex_obj.resident == Resident.Castle:
            return True
        
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, self.board.width, self.board.height)
        for nx, ny in neighbors:
            n = self.board.get_hex(nx, ny)
            if n and n.owner_id == self.my_player_id:
                if GameUtils.is_tower(n.resident) or n.resident == Resident.Castle:
                    return True
        return False
    
    def try_build_farms(self, province: Province):
        """Build farms for income."""
        farm_count = sum(1 for h in province.hexes if h.resident == Resident.Farm)
        if farm_count > 10:
            return  # Enough farms
        
        while province.money >= UNIT_PRICES[Resident.Farm]:
            # Find good farm location (adjacent to castle or farm, NOT on front line)
            best = None
            for h in province.hexes:
                if not GameUtils.hex_is_free(h.resident):
                    continue
                if h in self.front_line:
                    continue  # Don't build farms on front line
                
                neighbors = GameUtils.get_hex_neighbors(h.x, h.y, self.board.width, self.board.height)
                for nx, ny in neighbors:
                    n = self.board.get_hex(nx, ny)
                    if n and n.owner_id == self.my_player_id:
                        if n.resident == Resident.Castle or n.resident == Resident.Farm:
                            best = h
                            break
                
                if best:
                    break
            
            if not best:
                break
            
            self.log(f"    -> Building Farm at ({best.x},{best.y})")
            if self.send_place(Resident.Farm, province.castle_hex, best):
                province.money -= UNIT_PRICES[Resident.Farm]
            else:
                break
    
    def build_attack_units(self, province: Province):
        """Build units to attack enemies."""
        income = province.get_income()
        
        # First: clear palms with cheap units
        while province.can_afford_unit(1):
            palms = province.get_palms()
            if not palms:
                break
            self.log(f"    -> Building Peasant to clear palm")
            if self.send_place(Resident.Warrior1, province.castle_hex, palms[0]):
                province.money -= UNIT_PRICES[Resident.Warrior1]
                self.units_built_this_turn += 1
            else:
                break
        
        # Then: build attack units - PREFER CHEAP UNITS
        # Only build expensive units if we REALLY need them (tower busting)
        for strength in range(1, 5):
            # Strict income check for expensive units
            upkeep = GameUtils.get_unit_upkeep(strength)
            
            # Warrior3/4 - only build if:
            # 1. Income can support it (upkeep < 30% of income)
            # 2. There's actually a target that NEEDS this strength
            if strength >= 3:
                if upkeep > income * 0.3:
                    self.log(f"    Skipping str {strength}: upkeep {upkeep} > 30% of {income}")
                    continue
                if not self.needs_strong_unit(province, strength):
                    self.log(f"    Skipping str {strength}: no targets need it")
                    continue
            
            if not self.can_safely_build(province, strength):
                continue
            
            # Only build 1 expensive unit per turn
            max_units = 5 if strength <= 2 else 1
            built = 0
            
            while self.can_safely_build(province, strength) and built < max_units:
                if not self.try_attack_with_strength(province, strength):
                    break
                built += 1
    
    def needs_strong_unit(self, province: Province, strength: int) -> bool:
        """Check if there's a target that actually needs this strength."""
        if not province.castle_hex:
            return False
        
        placeable = self.province_manager.get_placeable_hexes(province, strength)
        
        for h in placeable:
            if h.owner_id == self.my_player_id:
                continue
            
            defense = GameUtils.get_defense_level(h, self.board)
            
            # Only need this strength if defense requires it
            if defense >= strength - 1:
                # Check: is the target safe enough for an expensive unit?
                friendly_neighbors = self.count_friendly_neighbors(h)
                if friendly_neighbors >= 2:  # At least 2 friendly neighbors = safer
                    return True
        
        return False
    
    def can_safely_build(self, province: Province, strength: int) -> bool:
        """Check if we can build a unit without going bankrupt."""
        resident = GameUtils.strength_to_warrior(strength)
        price = UNIT_PRICES.get(resident, 999)
        
        if province.money < price:
            return False
        
        upkeep = GameUtils.get_unit_upkeep(strength)
        new_income = province.get_income() - upkeep
        
        return new_income >= -2
    
    def try_attack_with_strength(self, province: Province, strength: int) -> bool:
        """Try to build a unit and attack."""
        if not province.castle_hex:
            return False
        
        placeable = self.province_manager.get_placeable_hexes(province, strength)
        placeable = [h for h in placeable if (h.x, h.y) not in self.failed_targets]
        
        # Filter to attack targets
        attack_targets = [h for h in placeable if h.owner_id != self.my_player_id]
        
        if not attack_targets:
            return False
        
        # For expensive units (3-4), only attack SAFE targets
        if strength >= 3:
            safe_targets = [h for h in attack_targets if self.count_friendly_neighbors(h) >= 2]
            if not safe_targets:
                return False
            attack_targets = safe_targets
        
        target = self.find_best_attack_target(attack_targets, strength)
        if not target:
            return False
        
        resident = GameUtils.strength_to_warrior(strength)
        self.log(f"    -> {resident.name} attacks ({target.x},{target.y})")
        
        if self.send_place(resident, province.castle_hex, target):
            province.money -= UNIT_PRICES[resident]
            self.units_built_this_turn += 1
            return True
        return False
    
    # ==================== UNIT POSITIONING ====================
    
    def pull_units_to_front_line(self, province: Province):
        """Move idle units toward the front line."""
        if len(province.hexes) < 15:
            return  # Small province, skip
        
        movable = province.get_movable_units()
        
        for unit_hex in movable:
            # Check if unit is already on front line
            if unit_hex in self.front_line:
                continue
            
            # Find closest front line hex
            closest = None
            min_dist = 999
            
            for fl in self.front_line:
                if not GameUtils.hex_is_free(fl.resident):
                    continue
                dist = abs(fl.x - unit_hex.x) + abs(fl.y - unit_hex.y)
                if dist < min_dist:
                    min_dist = dist
                    closest = fl
            
            if closest:
                self.log(f"    Pulling unit to front line ({closest.x},{closest.y})")
                self.send_move(unit_hex, closest)
    
    # ==================== COMMUNICATION ====================
    
    def send_move(self, from_hex, to_hex) -> bool:
        """Send a move action."""
        self.action_builder.add_move(from_hex.x, from_hex.y, to_hex.x, to_hex.y)
        self.action_builder.send()
        
        if self.receive_func:
            tag, payload = self.receive_func()
            if tag == 4:
                approved, awaiting = payload
                if not approved:
                    self.failed_targets.add((to_hex.x, to_hex.y))
                else:
                    time.sleep(MOVE_DELAY)
                return approved
        return True
    
    def send_place(self, resident: Resident, from_hex, to_hex) -> bool:
        """Send a place action."""
        self.action_builder.add_place(int(resident), from_hex.x, from_hex.y, to_hex.x, to_hex.y)
        self.action_builder.send()
        
        if self.receive_func:
            tag, payload = self.receive_func()
            if tag == 4:
                approved, awaiting = payload
                if not approved:
                    self.failed_targets.add((to_hex.x, to_hex.y))
                else:
                    time.sleep(MOVE_DELAY)
                return approved
        return True


# Backwards compatibility
SimpleAI = ImprovedAI
