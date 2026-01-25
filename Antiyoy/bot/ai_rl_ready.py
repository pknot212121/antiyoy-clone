"""
RL-Ready AI Architecture for Antiyoy.

This separates:
1. HARDCODED actions (always make sense, no decision needed)
2. RL-CONTROLLED actions (strategic spending decisions)

The RL model only decides HOW TO SPEND MONEY each turn.
Everything else is handled algorithmically.
"""

import random
import time
from typing import List, Optional, Tuple, Set, Dict
from enum import IntEnum
from .game_utils import GameUtils, Resident, UNIT_PRICES, INCOME_TABLE
from .province import Province, ProvinceManager

MOVE_DELAY = 0.5


class RLAction(IntEnum):
    """High-level actions for RL to choose from."""
    ATTACK = 0      # Build attack units, assault enemies
    DEFEND = 1      # Build towers, defensive units
    FARM = 2        # Build farms for income
    EXPAND = 3      # Grab neutral territory with cheap units
    WAIT = 4        # Save money for next turn


class GameState:
    """Normalized state features for RL model."""
    
    def __init__(self):
        # Economy (3 features)
        self.money_normalized = 0.0      # money / 100
        self.income_normalized = 0.0     # income / 20
        self.can_afford_unit = 0.0       # 1 if can afford Warrior1
        
        # Territory (3 features)
        self.my_territory_pct = 0.0      # my hexes / total
        self.enemy_territory_pct = 0.0   # enemy hexes / total
        self.neutral_pct = 0.0           # neutral hexes / total
        
        # Military (3 features)
        self.my_unit_count = 0.0         # my units / 20
        self.enemy_units_nearby = 0.0    # adjacent enemy units / 10
        self.threat_level = 0.0          # sum of enemy strength / 20
        
        # Defense (2 features)
        self.front_line_size = 0.0       # front line hexes / 20
        self.undefended_pct = 0.0        # undefended front line %
        
        # Opportunities (3 features)
        self.neutral_lands_nearby = 0.0  # grabbable neutral / 10
        self.attackable_farms = 0.0      # enemy farms we can hit / 5
        self.can_attack_castle = 0.0     # 1 if enemy castle attackable
    
    def to_vector(self) -> List[float]:
        """Convert to feature vector for neural network."""
        return [
            self.money_normalized,
            self.income_normalized,
            self.can_afford_unit,
            self.my_territory_pct,
            self.enemy_territory_pct,
            self.neutral_pct,
            self.my_unit_count,
            self.enemy_units_nearby,
            self.threat_level,
            self.front_line_size,
            self.undefended_pct,
            self.neutral_lands_nearby,
            self.attackable_farms,
            self.can_attack_castle,
        ]
    
    @staticmethod
    def feature_count() -> int:
        return 14


class RLReadyAI:
    """
    AI with clear separation of hardcoded vs RL-controlled actions.
    
    HARDCODED (always executed):
    - Move existing units (attack/clear trees)
    - Clear palm trees (they cost money)
    - Counter immediate threats (reactive)
    - Pull idle units to front line
    - Merge units when beneficial
    
    RL-CONTROLLED (spending decisions):
    - ATTACK: Build attack units
    - DEFEND: Build towers
    - FARM: Build farms
    - EXPAND: Grab neutral land
    - WAIT: Save money
    """
    
    def __init__(self, board, my_player_id: int, action_builder, receive_func=None, 
                 debug: bool = True, rl_policy=None):
        self.board = board
        self.my_player_id = my_player_id
        self.action_builder = action_builder
        self.receive_func = receive_func
        self.debug = debug
        self.province_manager = ProvinceManager(board, my_player_id)
        self.failed_targets: Set[Tuple[int, int]] = set()
        
        # RL policy (if None, uses rule-based fallback)
        self.rl_policy = rl_policy
        
        # Turn state
        self.units_built = 0
        self.money_spent = 0
        
        # Situation analysis results
        self.state = GameState()
        self.enemy_threats: List = []
        self.front_line: List = []
        self.neutral_lands: List = []
        self.threat_level = 0
    
    def log(self, msg: str):
        if self.debug:
            print(f"[AI] {msg}")
    
    # ==================== MAIN TURN LOGIC ====================
    
    def make_move(self):
        """Execute turn with hardcoded + RL-controlled actions."""
        self.log(f"=== Turn for player {self.my_player_id} ===")
        self.units_built = 0
        self.money_spent = 0
        
        for province in self.province_manager.my_provinces:
            self.log(f"Province: {len(province.hexes)} hexes, {province.money}g, income {province.get_income()}")
            
            # ========== HARDCODED PHASE ==========
            # These always happen, no decision needed
            
            # 1. Analyze situation
            self.analyze_situation(province)
            self.extract_state_features(province)
            
            # 2. ALWAYS: Move existing units (they're paid for!)
            self.move_existing_units(province)
            
            # 3. ALWAYS: Clear palm trees (save money)
            self.clear_palms_with_new_units(province)
            
            # 4. ALWAYS: Counter immediate threats (reactive defense)
            self.counter_immediate_threats(province)
            
            # ========== RL-CONTROLLED PHASE ==========
            # RL decides how to spend remaining money
            
            action = self.choose_rl_action(province)
            self.execute_rl_action(province, action)
            
            # ========== HARDCODED CLEANUP ==========
            
            # 5. ALWAYS: Pull idle units to front line
            self.pull_idle_units_to_front(province)
        
        self.log("Ending turn")
        self.action_builder.add_end_turn()
        
        return self.state  # Return state for RL training
    
    # ==================== STATE EXTRACTION ====================
    
    def extract_state_features(self, province: Province):
        """Extract normalized features for RL model."""
        total_hexes = self.board.width * self.board.height
        
        # Economy
        self.state.money_normalized = min(province.money / 100.0, 1.0)
        self.state.income_normalized = min(max(province.get_income(), 0) / 20.0, 1.0)
        self.state.can_afford_unit = 1.0 if province.money >= UNIT_PRICES[Resident.Warrior1] else 0.0
        
        # Territory
        my_hexes = len(province.hexes)
        enemy_hexes = sum(1 for p in self.province_manager.enemy_provinces for _ in p.hexes)
        self.state.my_territory_pct = my_hexes / total_hexes
        self.state.enemy_territory_pct = enemy_hexes / total_hexes
        self.state.neutral_pct = 1.0 - self.state.my_territory_pct - self.state.enemy_territory_pct
        
        # Military
        my_units = len(province.get_movable_units()) + sum(
            1 for h in province.hexes if GameUtils.is_moved_warrior(h.resident)
        )
        self.state.my_unit_count = min(my_units / 20.0, 1.0)
        self.state.enemy_units_nearby = min(len(self.enemy_threats) / 10.0, 1.0)
        self.state.threat_level = min(self.threat_level / 20.0, 1.0)
        
        # Defense
        self.state.front_line_size = min(len(self.front_line) / 20.0, 1.0)
        if self.front_line:
            undefended = sum(1 for h in self.front_line 
                           if GameUtils.get_defense_level(h, self.board) == 0)
            self.state.undefended_pct = undefended / len(self.front_line)
        else:
            self.state.undefended_pct = 0.0
        
        # Opportunities
        self.state.neutral_lands_nearby = min(len(self.neutral_lands) / 10.0, 1.0)
        
        # Count attackable farms and castles
        attackable_farms = 0
        can_attack_castle = False
        if province.castle_hex:
            for strength in range(1, 5):
                placeable = self.province_manager.get_placeable_hexes(province, strength)
                for h in placeable:
                    if h.owner_id != self.my_player_id:
                        if h.resident == Resident.Farm:
                            attackable_farms += 1
                        if h.resident == Resident.Castle:
                            can_attack_castle = True
        
        self.state.attackable_farms = min(attackable_farms / 5.0, 1.0)
        self.state.can_attack_castle = 1.0 if can_attack_castle else 0.0
    
    # ==================== RL ACTION SELECTION ====================
    
    def choose_rl_action(self, province: Province) -> RLAction:
        """Choose action using RL policy or fallback rules."""
        if self.rl_policy is not None:
            # Use neural network policy
            state_vector = self.state.to_vector()
            return self.rl_policy.choose_action(state_vector)
        else:
            # Rule-based fallback
            return self.rule_based_action(province)
    
    def rule_based_action(self, province: Province) -> RLAction:
        """Fallback rule-based decision making."""
        income = province.get_income()
        
        # Priority 1: If under heavy threat, defend
        if self.threat_level >= 3 and self.state.undefended_pct > 0.3:
            return RLAction.DEFEND
        
        # Priority 2: Expand if lots of neutral land
        if len(self.neutral_lands) > 5:
            return RLAction.EXPAND
        
        # Priority 3: Farm if income is low and safe
        if income < 5 and self.threat_level == 0:
            return RLAction.FARM
        
        # Priority 4: Attack if we have good targets
        if self.state.attackable_farms > 0.2 or self.state.can_attack_castle:
            return RLAction.ATTACK
        
        # Priority 5: Build towers if front line exposed
        if self.state.undefended_pct > 0.5:
            return RLAction.DEFEND
        
        # Priority 6: Save money
        if province.money < 30:
            return RLAction.WAIT
        
        # Default: balanced between expand and attack
        return RLAction.EXPAND if len(self.neutral_lands) > 0 else RLAction.ATTACK
    
    def execute_rl_action(self, province: Province, action: RLAction):
        """Execute the chosen RL action."""
        self.log(f"  RL Action: {action.name}")
        
        if action == RLAction.ATTACK:
            self.action_attack(province)
        elif action == RLAction.DEFEND:
            self.action_defend(province)
        elif action == RLAction.FARM:
            self.action_farm(province)
        elif action == RLAction.EXPAND:
            self.action_expand(province)
        elif action == RLAction.WAIT:
            self.action_wait(province)
    
    # ==================== RL ACTION IMPLEMENTATIONS ====================
    
    def action_attack(self, province: Province):
        """Build attack units and assault enemies."""
        income = province.get_income()
        
        for strength in range(1, 5):
            upkeep = GameUtils.get_unit_upkeep(strength)
            
            # Income check for expensive units
            if strength >= 3 and upkeep > income * 0.3:
                continue
            
            if not self.can_safely_build(province, strength):
                continue
            
            # Build units
            max_units = 5 if strength <= 2 else 1
            for _ in range(max_units):
                if not self.try_attack_with_strength(province, strength):
                    break
    
    def action_defend(self, province: Province):
        """Build towers and defensive units."""
        # Build towers on front line
        while province.money >= UNIT_PRICES[Resident.Tower]:
            best = self.find_best_tower_location(province)
            if not best:
                break
            
            self.log(f"    -> Tower at ({best.x},{best.y})")
            if self.send_place(Resident.Tower, province.castle_hex, best):
                province.money -= UNIT_PRICES[Resident.Tower]
            else:
                break
        
        # Build defensive units on front line
        while province.can_afford_unit(1) and self.units_built < 3:
            front_empty = [h for h in self.front_line if GameUtils.hex_is_free(h.resident)]
            if not front_empty:
                break
            
            target = front_empty[0]
            self.log(f"    -> Defender at ({target.x},{target.y})")
            if self.send_place(Resident.Warrior1, province.castle_hex, target):
                province.money -= UNIT_PRICES[Resident.Warrior1]
                self.units_built += 1
            else:
                break
    
    def action_farm(self, province: Province):
        """Build farms for income."""
        farm_count = sum(1 for h in province.hexes if h.resident == Resident.Farm)
        
        while province.money >= UNIT_PRICES[Resident.Farm] and farm_count < 15:
            best = self.find_best_farm_location(province)
            if not best:
                break
            
            self.log(f"    -> Farm at ({best.x},{best.y})")
            if self.send_place(Resident.Farm, province.castle_hex, best):
                province.money -= UNIT_PRICES[Resident.Farm]
                farm_count += 1
            else:
                break
    
    def action_expand(self, province: Province):
        """Grab neutral territory with cheap units."""
        while province.can_afford_unit(1) and self.units_built < 10:
            if not province.castle_hex:
                break
            
            placeable = self.province_manager.get_placeable_hexes(province, 1)
            neutral_targets = [h for h in placeable if h.owner_id == 0]
            
            if not neutral_targets:
                break
            
            # Prefer hexes with friendly neighbors
            neutral_targets.sort(key=lambda h: -self.count_friendly_neighbors(h))
            target = neutral_targets[0]
            
            self.log(f"    -> Expand to ({target.x},{target.y})")
            if self.send_place(Resident.Warrior1, province.castle_hex, target):
                province.money -= UNIT_PRICES[Resident.Warrior1]
                self.units_built += 1
            else:
                break
    
    def action_wait(self, province: Province):
        """Save money - do nothing."""
        self.log(f"    -> Saving money ({province.money}g)")
        pass
    
    # ==================== HARDCODED ACTIONS ====================
    
    def analyze_situation(self, province: Province):
        """Analyze threats and opportunities."""
        self.enemy_threats = []
        self.front_line = []
        self.neutral_lands = []
        self.threat_level = 0
        
        for h in province.hexes:
            neighbors = GameUtils.get_hex_neighbors(h.x, h.y, self.board.width, self.board.height)
            is_front_line = False
            
            for nx, ny in neighbors:
                neighbor = self.board.get_hex(nx, ny)
                if not neighbor or GameUtils.is_water(neighbor.resident):
                    continue
                
                if neighbor.owner_id == 0:
                    if neighbor not in self.neutral_lands:
                        self.neutral_lands.append(neighbor)
                elif neighbor.owner_id != self.my_player_id:
                    is_front_line = True
                    if GameUtils.is_warrior(neighbor.resident):
                        strength = GameUtils.get_warrior_strength(neighbor.resident)
                        self.enemy_threats.append((neighbor, strength))
                        self.threat_level += strength
            
            if is_front_line:
                self.front_line.append(h)
    
    def move_existing_units(self, province: Province):
        """HARDCODED: Move all existing units (attack/clear trees)."""
        movable = province.get_movable_units()
        self.log(f"  Moving {len(movable)} existing units")
        
        for unit_hex in movable:
            strength = GameUtils.get_warrior_strength(unit_hex.resident)
            
            # Clear palms first (cost money)
            if strength <= 2:
                palm = self.find_palm(unit_hex)
                if palm:
                    self.send_move(unit_hex, palm)
                    continue
            
            # Attack if possible
            attackable = self.province_manager.get_attackable_hexes(unit_hex, strength)
            attackable = [h for h in attackable if (h.x, h.y) not in self.failed_targets]
            
            if attackable:
                target = self.find_best_attack_target(attackable, strength)
                if target:
                    self.send_move(unit_hex, target)
                    continue
            
            # Clear other trees
            tree = self.find_tree(unit_hex)
            if tree:
                self.send_move(unit_hex, tree)
    
    def clear_palms_with_new_units(self, province: Province):
        """HARDCODED: Build units to clear palms (always profitable)."""
        while province.can_afford_unit(1):
            palms = province.get_palms()
            if not palms:
                break
            
            self.log(f"    -> Clearing palm at ({palms[0].x},{palms[0].y})")
            if self.send_place(Resident.Warrior1, province.castle_hex, palms[0]):
                province.money -= UNIT_PRICES[Resident.Warrior1]
                self.units_built += 1
            else:
                break
    
    def counter_immediate_threats(self, province: Province):
        """HARDCODED: React to immediate enemy threats."""
        for threat_hex, threat_strength in self.enemy_threats:
            our_hexes = self.get_adjacent_owned_hexes(threat_hex)
            if not our_hexes:
                continue
            
            best_defense = max(GameUtils.get_defense_level(h, self.board) for h in our_hexes)
            
            if best_defense >= threat_strength:
                continue  # Safe
            
            # Try to counter-attack
            for h in province.hexes:
                if not GameUtils.is_unmoved_warrior(h.resident):
                    continue
                our_str = GameUtils.get_warrior_strength(h.resident)
                if our_str <= threat_strength:
                    continue
                
                reachable = self.province_manager._get_move_zone(h, our_str)
                if threat_hex in reachable:
                    self.log(f"    -> Counter-attack threat at ({threat_hex.x},{threat_hex.y})")
                    self.send_move(h, threat_hex)
                    break
    
    def pull_idle_units_to_front(self, province: Province):
        """HARDCODED: Pull idle units toward front line."""
        if len(province.hexes) < 15:
            return
        
        movable = province.get_movable_units()
        
        for unit_hex in movable:
            if unit_hex in self.front_line:
                continue
            
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
                self.send_move(unit_hex, closest)
    
    # ==================== HELPER METHODS ====================
    
    def find_palm(self, unit_hex) -> Optional:
        reachable = self.province_manager._get_move_zone(unit_hex, 1)
        for h in reachable:
            if h.owner_id == self.my_player_id and GameUtils.is_palm(h.resident):
                return h
        return None
    
    def find_tree(self, unit_hex) -> Optional:
        reachable = self.province_manager._get_move_zone(unit_hex, 1)
        for h in reachable:
            if h.owner_id == self.my_player_id and GameUtils.is_tree(h.resident):
                return h
        return None
    
    def find_best_attack_target(self, attackable: List, strength: int) -> Optional:
        if not attackable:
            return None
        
        best = None
        best_score = -999
        
        for h in attackable:
            score = 0
            
            if h.resident == Resident.Farm:
                score += 50
            if h.resident == Resident.Castle:
                score += 100
            if GameUtils.is_warrior(h.resident):
                score += GameUtils.get_warrior_strength(h.resident) * 10
            
            friendly = self.count_friendly_neighbors(h)
            score += friendly * 5
            
            if strength >= 3 and friendly < 2:
                score -= 30
            
            if score > best_score:
                best_score = score
                best = h
        
        return best
    
    def find_best_tower_location(self, province: Province) -> Optional:
        best = None
        best_gain = 0
        
        for h in self.front_line:
            if not GameUtils.hex_is_free(h.resident):
                continue
            if self.is_defended_by_tower(h):
                continue
            
            gain = 1
            neighbors = GameUtils.get_hex_neighbors(h.x, h.y, self.board.width, self.board.height)
            for nx, ny in neighbors:
                n = self.board.get_hex(nx, ny)
                if n and n.owner_id == self.my_player_id:
                    if not self.is_defended_by_tower(n):
                        gain += 1
            
            if gain > best_gain:
                best_gain = gain
                best = h
        
        return best if best_gain >= 3 else None
    
    def find_best_farm_location(self, province: Province) -> Optional:
        for h in province.hexes:
            if not GameUtils.hex_is_free(h.resident):
                continue
            if h in self.front_line:
                continue
            
            neighbors = GameUtils.get_hex_neighbors(h.x, h.y, self.board.width, self.board.height)
            for nx, ny in neighbors:
                n = self.board.get_hex(nx, ny)
                if n and n.owner_id == self.my_player_id:
                    if n.resident in (Resident.Castle, Resident.Farm):
                        return h
        return None
    
    def get_adjacent_owned_hexes(self, hex_obj) -> List:
        result = []
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, self.board.width, self.board.height)
        for nx, ny in neighbors:
            n = self.board.get_hex(nx, ny)
            if n and n.owner_id == self.my_player_id:
                result.append(n)
        return result
    
    def count_friendly_neighbors(self, hex_obj) -> int:
        return len(self.get_adjacent_owned_hexes(hex_obj))
    
    def is_defended_by_tower(self, hex_obj) -> bool:
        if GameUtils.is_tower(hex_obj.resident) or hex_obj.resident == Resident.Castle:
            return True
        for n in self.get_adjacent_owned_hexes(hex_obj):
            if GameUtils.is_tower(n.resident) or n.resident == Resident.Castle:
                return True
        return False
    
    def can_safely_build(self, province: Province, strength: int) -> bool:
        resident = GameUtils.strength_to_warrior(strength)
        price = UNIT_PRICES.get(resident, 999)
        if province.money < price:
            return False
        upkeep = GameUtils.get_unit_upkeep(strength)
        new_income = province.get_income() - upkeep
        return new_income >= -2
    
    def try_attack_with_strength(self, province: Province, strength: int) -> bool:
        if not province.castle_hex:
            return False
        
        placeable = self.province_manager.get_placeable_hexes(province, strength)
        placeable = [h for h in placeable if (h.x, h.y) not in self.failed_targets]
        attack_targets = [h for h in placeable if h.owner_id != self.my_player_id]
        
        if not attack_targets:
            return False
        
        if strength >= 3:
            safe = [h for h in attack_targets if self.count_friendly_neighbors(h) >= 2]
            if not safe:
                return False
            attack_targets = safe
        
        target = self.find_best_attack_target(attack_targets, strength)
        if not target:
            return False
        
        resident = GameUtils.strength_to_warrior(strength)
        if self.send_place(resident, province.castle_hex, target):
            province.money -= UNIT_PRICES[resident]
            self.units_built += 1
            return True
        return False
    
    # ==================== COMMUNICATION ====================
    
    def send_move(self, from_hex, to_hex) -> bool:
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


# Export for use
SimpleAI = RLReadyAI

