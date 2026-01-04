"""
Expert AI implementation for Antiyoy.
Based on the Java AiExpertGenericRules.

Key fixes from Java AI:
1. unitCanMoveSafely() - Only move if not leaving hexes undefended
2. Simple attack allure scoring (friendly neighbors + castle bonus)
3. Farm building limits (MAX_EXTRA_FARM_COST)
4. Tower placement needs defense gain >= 4
5. Order: Towers -> Farms -> Units
"""

import random
import time
from typing import List, Optional, Tuple, Set, Dict
from .game_utils import GameUtils, Resident, UNIT_PRICES, INCOME_TABLE
from .province import Province, ProvinceManager

MOVE_DELAY = 0.5
MAX_EXTRA_FARM_COST = 80  # From Java: Don't build farms if cost > 80


class ImprovedAI:
    """Expert AI matching Java behavior."""
    
    def __init__(self, board, my_player_id: int, action_builder, receive_func=None, debug: bool = True):
        self.board = board
        self.my_player_id = my_player_id
        self.action_builder = action_builder
        self.receive_func = receive_func
        self.debug = debug
        self.province_manager = ProvinceManager(board, my_player_id)
        self.failed_targets: Set[Tuple[int, int]] = set()
        self.units_built_this_turn = 0
        
        # Situation tracking
        self.front_line: List = []
        self.neutral_lands: List = []
        self.enemy_threats: List = []
        self.threat_level = 0
    
    def log(self, msg: str):
        if self.debug:
            print(f"[AI] {msg}")
    
    def make_move(self):
        """Execute turn matching Java AI structure."""
        self.log(f"=== Turn for player {self.my_player_id} ===")
        self.units_built_this_turn = 0
        self.failed_targets.clear()
        
        for province in self.province_manager.my_provinces:
            self.log(f"Province: {len(province.hexes)} hexes, {province.money}g, income {province.get_income()}")
            
            # Analyze situation
            self.analyze_situation(province)
            
            # Phase 1: Move existing units (Java: moveUnits())
            self.move_units(province)
            
            # Phase 2: Spend money and merge (Java: spendMoneyAndMergeUnits())
            self.spend_money(province)
            
            # Phase 3: Move AFK units to perimeter (Java: moveAfkUnits())
            self.move_afk_units(province)
        
        self.log("Ending turn")
        self.action_builder.add_end_turn()
    
    # ==================== SITUATION ANALYSIS ====================
    
    def analyze_situation(self, province: Province):
        """Analyze threats and opportunities."""
        self.front_line = []
        self.neutral_lands = []
        self.enemy_threats = []
        self.threat_level = 0
        
        for h in province.hexes:
            neighbors = GameUtils.get_hex_neighbors(h.x, h.y, self.board.width, self.board.height)
            is_perimeter = False
            
            for nx, ny in neighbors:
                neighbor = self.board.get_hex(nx, ny)
                if not neighbor or GameUtils.is_water(neighbor.resident):
                    continue
                
                if neighbor.owner_id == 0:
                    if neighbor not in self.neutral_lands:
                        self.neutral_lands.append(neighbor)
                elif neighbor.owner_id != self.my_player_id:
                    is_perimeter = True
                    if GameUtils.is_warrior(neighbor.resident):
                        strength = GameUtils.get_warrior_strength(neighbor.resident)
                        self.enemy_threats.append((neighbor, strength))
                        self.threat_level += strength
            
            if is_perimeter:
                self.front_line.append(h)
        
        self.log(f"  Perimeter: {len(self.front_line)}, Neutral: {len(self.neutral_lands)}, Threats: {self.threat_level}")
    
    # ==================== MOVE UNITS (Java: moveUnits) ====================
    
    def move_units(self, province: Province):
        """Move all unmoved units."""
        movable = province.get_movable_units()
        self.log(f"  Moving {len(movable)} units")
        
        for unit_hex in movable:
            self.decide_about_unit(unit_hex, province)
    
    def decide_about_unit(self, unit_hex, province: Province):
        """Java: decideAboutUnit() - decide what to do with unit."""
        strength = GameUtils.get_warrior_strength(unit_hex.resident)
        
        # Get ONLY directly adjacent hexes for this unit
        adjacent = self.get_adjacent_attackable(unit_hex, strength)
        
        # Priority 1: Clear palms (highest priority, from Java)
        if strength <= 2:
            for h in adjacent:
                if h.owner_id == self.my_player_id and GameUtils.is_palm(h.resident):
                    self.log(f"    Unit ({unit_hex.x},{unit_hex.y}) clearing palm")
                    if self.send_move(unit_hex, h):
                        return
        
        # Priority 2: Attack enemies OR expand to neutral
        attackable = [h for h in adjacent if h.owner_id != self.my_player_id]
        attackable = [h for h in attackable if (h.x, h.y) not in self.failed_targets]
        
        if attackable:
            enemies = [h for h in attackable if h.owner_id != 0]
            neutrals = [h for h in attackable if h.owner_id == 0]
            
            if enemies:
                self.try_to_attack_something(unit_hex, province, enemies, strength)
                return
            elif neutrals:
                target = self.find_most_attractive_hex(neutrals, strength)
                if target:
                    self.log(f"    Unit ({unit_hex.x},{unit_hex.y}) expanding to ({target.x},{target.y})")
                    if self.send_move(unit_hex, target):
                        return
        
        # Priority 3: Clear trees in adjacent hexes
        for h in adjacent:
            if h.owner_id == self.my_player_id and GameUtils.is_tree(h.resident):
                self.log(f"    Unit ({unit_hex.x},{unit_hex.y}) clearing tree")
                if self.send_move(unit_hex, h):
                    return
        
        # Priority 4: Move through owned territory to find targets
        # BFS to find nearest attackable hex
        nearest_target = self.find_nearest_attackable(unit_hex, strength)
        if nearest_target:
            # Move one step toward it
            next_step = self.get_next_step_toward(unit_hex, nearest_target)
            if next_step:
                self.log(f"    Unit ({unit_hex.x},{unit_hex.y}) moving toward ({nearest_target.x},{nearest_target.y})")
                if self.send_move(unit_hex, next_step):
                    return
        
        # Priority 5: Push to better defense if in perimeter
        if self.is_in_perimeter(unit_hex):
            self.push_unit_to_better_defense(unit_hex, province)
    
    def get_adjacent_attackable(self, unit_hex, strength: int) -> List:
        """Get hexes directly adjacent to unit that it can move to."""
        result = []
        neighbors = GameUtils.get_hex_neighbors(unit_hex.x, unit_hex.y, self.board.width, self.board.height)
        
        for nx, ny in neighbors:
            h = self.board.get_hex(nx, ny)
            if not h:
                continue
            if GameUtils.is_water(h.resident):
                continue
            
            # Can always move to own territory
            if h.owner_id == self.my_player_id:
                result.append(h)
            else:
                # Can attack if strong enough
                defense = GameUtils.get_defense_level(h, self.board)
                if strength > defense:
                    result.append(h)
        
        return result
    
    def find_nearest_attackable(self, unit_hex, strength: int):
        """BFS to find nearest attackable enemy/neutral hex."""
        from collections import deque
        
        visited = set()
        queue = deque([(unit_hex, 0)])  # (hex, distance)
        
        while queue:
            current, dist = queue.popleft()
            key = (current.x, current.y)
            
            if key in visited:
                continue
            visited.add(key)
            
            # Check if this hex is attackable (enemy or neutral, not ours)
            if current.owner_id != self.my_player_id and current != unit_hex:
                defense = GameUtils.get_defense_level(current, self.board)
                if strength > defense:
                    return current
            
            # Only continue BFS through our territory
            if current.owner_id != self.my_player_id and current != unit_hex:
                continue
            
            neighbors = GameUtils.get_hex_neighbors(current.x, current.y, self.board.width, self.board.height)
            for nx, ny in neighbors:
                if (nx, ny) in visited:
                    continue
                h = self.board.get_hex(nx, ny)
                if h and not GameUtils.is_water(h.resident):
                    queue.append((h, dist + 1))
        
        return None
    
    def get_next_step_toward(self, unit_hex, target_hex):
        """Get the next step from unit toward target through our territory."""
        # Simple: check adjacent hexes, pick the one that's ours and closest to target
        neighbors = GameUtils.get_hex_neighbors(unit_hex.x, unit_hex.y, self.board.width, self.board.height)
        
        best = None
        best_dist = 999
        
        for nx, ny in neighbors:
            h = self.board.get_hex(nx, ny)
            if not h:
                continue
            
            # Must be our territory to move through
            if h.owner_id != self.my_player_id:
                continue
            if not GameUtils.hex_is_free(h.resident):
                continue  # Can't move to occupied hex
            
            dist = abs(nx - target_hex.x) + abs(ny - target_hex.y)
            if dist < best_dist:
                best_dist = dist
                best = h
        
        return best
    
    def try_to_attack_something(self, unit_hex, province: Province, attackable: List, strength: int):
        """Java: tryToAttackSomething() - attack only if safe."""
        # KEY FIX: Only attack if unit can move safely
        if not self.unit_can_move_safely(unit_hex):
            self.log(f"    Unit ({unit_hex.x},{unit_hex.y}) stays (would leave hexes undefended)")
            return
        
        target = self.find_most_attractive_hex(attackable, strength)
        if target:
            self.log(f"    Unit ({unit_hex.x},{unit_hex.y}) attacking ({target.x},{target.y})")
            self.send_move(unit_hex, target)
    
    def unit_can_move_safely(self, unit_hex) -> bool:
        """
        Java: unitCanMoveSafely()
        Only move if not leaving more than 3 perimeter hexes undefended.
        """
        left_behind = 0
        neighbors = GameUtils.get_hex_neighbors(unit_hex.x, unit_hex.y, self.board.width, self.board.height)
        
        for nx, ny in neighbors:
            adj = self.board.get_hex(nx, ny)
            if not adj or adj.owner_id != self.my_player_id:
                continue
            
            if not self.is_in_perimeter(adj):
                continue
            
            # Would this hex be undefended if we leave?
            if not self.is_hex_defended_by_something_else(adj, unit_hex):
                left_behind += 1
        
        return left_behind <= 3
    
    def is_hex_defended_by_something_else(self, hex_obj, excluding_unit_hex) -> bool:
        """Java: isHexDefendedBySomethingElse() - check if hex has other defense."""
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, self.board.width, self.board.height)
        
        for nx, ny in neighbors:
            adj = self.board.get_hex(nx, ny)
            if not adj or adj.owner_id != self.my_player_id:
                continue
            
            # Building defends
            if GameUtils.is_tower(adj.resident) or adj.resident == Resident.Castle:
                return True
            
            # Another unit defends (not the one we're moving)
            if GameUtils.is_warrior(adj.resident) and (adj.x, adj.y) != (excluding_unit_hex.x, excluding_unit_hex.y):
                return True
        
        return False
    
    def is_in_perimeter(self, hex_obj) -> bool:
        """Check if hex is on the perimeter (adjacent to enemy)."""
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, self.board.width, self.board.height)
        for nx, ny in neighbors:
            adj = self.board.get_hex(nx, ny)
            if adj and adj.owner_id != self.my_player_id and adj.owner_id != 0:
                if not GameUtils.is_water(adj.resident):
                    return True
        return False
    
    def find_most_attractive_hex(self, attackable: List, strength: int) -> Optional:
        """
        Java: findMostAttractiveHex()
        Simple scoring: count friendly neighbors, bonus for castle.
        """
        if not attackable:
            return None
        
        # For strong units, try baron logic first (target towers)
        if strength >= 3:
            baron_target = self.find_hex_attractive_to_baron(attackable, strength)
            if baron_target:
                return baron_target
        
        # Standard allure calculation
        best = None
        best_allure = -1
        
        for h in attackable:
            allure = self.get_attack_allure(h)
            if allure > best_allure:
                best_allure = allure
                best = h
        
        return best
    
    def get_attack_allure(self, hex_obj) -> int:
        """
        Java: getAttackAllure()
        Simple: count friendly neighbors, +5 for castle nearby, x2 for farms.
        """
        allure = 0
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, self.board.width, self.board.height)
        
        for nx, ny in neighbors:
            adj = self.board.get_hex(nx, ny)
            if not adj or adj.owner_id != self.my_player_id:
                continue
            
            allure += 1  # Friendly neighbor
            
            if adj.resident == Resident.Castle:
                allure += 5  # Castle bonus
        
        # Double allure for enemy farms
        if hex_obj.resident == Resident.Farm:
            allure *= 2
        
        return allure
    
    def find_hex_attractive_to_baron(self, attackable: List, strength: int) -> Optional:
        """Java: findHexAttractiveToBaron() - strong units target towers."""
        for h in attackable:
            if GameUtils.is_tower(h.resident):
                defense = GameUtils.get_defense_level(h, self.board)
                if strength > defense:
                    return h
        return None
    
    def push_unit_to_better_defense(self, unit_hex, province: Province):
        """Java: pushUnitToBetterDefense() - move to safer spot."""
        neighbors = GameUtils.get_hex_neighbors(unit_hex.x, unit_hex.y, self.board.width, self.board.height)
        
        for nx, ny in neighbors:
            adj = self.board.get_hex(nx, ny)
            if not adj or adj.owner_id != self.my_player_id:
                continue
            if not GameUtils.hex_is_free(adj.resident):
                continue
            
            # Move to hex with no enemy neighbors
            if self.count_enemy_neighbors(adj) == 0:
                self.log(f"    Unit ({unit_hex.x},{unit_hex.y}) moving to safer spot ({adj.x},{adj.y})")
                self.send_move(unit_hex, adj)
                break
    
    def count_enemy_neighbors(self, hex_obj) -> int:
        """Count how many enemy hexes are adjacent."""
        count = 0
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, self.board.width, self.board.height)
        for nx, ny in neighbors:
            adj = self.board.get_hex(nx, ny)
            if adj and adj.owner_id != self.my_player_id and adj.owner_id != 0:
                if not GameUtils.is_water(adj.resident):
                    count += 1
        return count
    
    # ==================== SPEND MONEY (Java: spendMoney) ====================
    
    def spend_money(self, province: Province):
        """Java order: Towers -> Farms -> Units."""
        # Step 1: Build towers
        self.try_to_build_towers(province)
        
        # Step 2: Build farms
        self.try_to_build_farms(province)
        
        # Step 3: Build units
        self.try_to_build_units(province)
    
    def try_to_build_towers(self, province: Province):
        """Java: tryToBuildTowers() - build towers on perimeter."""
        while province.money >= UNIT_PRICES[Resident.Tower]:
            hex_needing_tower = self.find_hex_that_needs_tower(province)
            if not hex_needing_tower:
                break
            
            self.log(f"    -> Tower at ({hex_needing_tower.x},{hex_needing_tower.y})")
            if self.send_place(Resident.Tower, province.castle_hex, hex_needing_tower):
                province.money -= UNIT_PRICES[Resident.Tower]
            else:
                break
    
    def find_hex_that_needs_tower(self, province: Province) -> Optional:
        """Java: needTowerOnHex() - need defense gain >= 4."""
        best = None
        best_gain = 0
        
        for h in self.front_line:
            if not GameUtils.hex_is_free(h.resident):
                continue
            
            gain = self.get_predicted_defense_gain(h)
            
            # Java requires gain >= 4
            if gain >= 4 and gain > best_gain:
                best_gain = gain
                best = h
        
        return best
    
    def get_predicted_defense_gain(self, hex_obj) -> int:
        """Count how many hexes this tower would defend."""
        if self.is_defended_by_tower(hex_obj):
            return 0
        
        gain = 1  # The hex itself
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, self.board.width, self.board.height)
        
        for nx, ny in neighbors:
            adj = self.board.get_hex(nx, ny)
            if not adj or adj.owner_id != self.my_player_id:
                        continue
                    
            if not self.is_defended_by_tower(adj):
                gain += 1
        
        return gain
    
    def is_defended_by_tower(self, hex_obj) -> bool:
        """Check if hex is defended by tower or castle."""
        if GameUtils.is_tower(hex_obj.resident) or hex_obj.resident == Resident.Castle:
            return True
        
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, self.board.width, self.board.height)
        for nx, ny in neighbors:
            adj = self.board.get_hex(nx, ny)
            if adj and adj.owner_id == self.my_player_id:
                if GameUtils.is_tower(adj.resident) or adj.resident == Resident.Castle:
                    return True
        return False
    
    def try_to_build_farms(self, province: Province):
        """Java: tryToBuildFarms() with MAX_EXTRA_FARM_COST limit."""
        farm_count = sum(1 for h in province.hexes if h.resident == Resident.Farm)
        extra_cost = farm_count * 2  # Each farm costs 2 more
        
        if extra_cost > MAX_EXTRA_FARM_COST:
            return
        
        while province.money >= UNIT_PRICES[Resident.Farm]:
            if not self.is_ok_to_build_new_farm(province):
                return
            
            hex_for_farm = self.find_good_hex_for_farm(province)
            if not hex_for_farm:
                return
            
            self.log(f"    -> Farm at ({hex_for_farm.x},{hex_for_farm.y})")
            if self.send_place(Resident.Farm, province.castle_hex, hex_for_farm):
                province.money -= UNIT_PRICES[Resident.Farm]
            else:
                        break
    
    def is_ok_to_build_new_farm(self, province: Province) -> bool:
        """
        Java: isOkToBuildNewFarm()
        Build farms if:
        - Have enough money (2x farm price), OR
        - No hexes need towers
        """
        farm_count = sum(1 for h in province.hexes if h.resident == Resident.Farm)
        current_price = UNIT_PRICES[Resident.Farm] + farm_count * 2
        
        if province.money > 2 * current_price:
            return True
        
        if self.find_hex_that_needs_tower(province) is not None:
            return False
        
        return True
    
    def find_good_hex_for_farm(self, province: Province) -> Optional:
        """Java: findGoodHexForFarm() - must be near castle or farm."""
        candidates = []
        
        for h in province.hexes:
            if not GameUtils.hex_is_free(h.resident):
                continue
            
            # Don't build on front line
            if h in self.front_line:
                continue
            
            # Must be adjacent to castle or farm
            neighbors = GameUtils.get_hex_neighbors(h.x, h.y, self.board.width, self.board.height)
            for nx, ny in neighbors:
                adj = self.board.get_hex(nx, ny)
                if adj and adj.owner_id == self.my_player_id:
                    if adj.resident in (Resident.Castle, Resident.Farm):
                        candidates.append(h)
                        break
        
        if candidates:
            return random.choice(candidates)
        return None
    
    def try_to_build_units(self, province: Province):
        """Java: tryToBuildUnits() - build from strength 1-4."""
        # First: clear palms with cheap units
        self.try_to_build_units_on_palms(province)
        
        # PRIORITY: If there are neutral lands, expand aggressively!
        if self.neutral_lands:
            self.expand_to_neutrals(province)
        
        # Then: attack enemies with increasing strength
        for strength in range(1, 5):
            if not self.province_has_enough_income(province, strength):
                break
            
            while self.can_province_build_unit(province, strength):
                if not self.try_to_attack_with_strength(province, strength):
                    break
        
        # Kickstart: build at least one unit if province is weak
        if self.can_province_build_unit(province, 1):
            unit_count = len([h for h in province.hexes if GameUtils.is_warrior(h.resident)])
            if unit_count <= 1:
                self.try_to_attack_with_strength(province, 1)
    
    def expand_to_neutrals(self, province: Province):
        """Aggressively expand into neutral territory."""
        while self.can_province_build_unit(province, 1) and self.units_built_this_turn < 10:
            if not province.castle_hex:
                break
            
            # Find neutral hexes we can place a unit on
            placeable = self.province_manager.get_placeable_hexes(province, 1)
            neutral_targets = [h for h in placeable if h.owner_id == 0]
            neutral_targets = [h for h in neutral_targets if (h.x, h.y) not in self.failed_targets]
            
            if not neutral_targets:
                break
            
            # Prefer hexes with most friendly neighbors (consolidate territory)
            neutral_targets.sort(key=lambda h: -self.count_friendly_neighbors(h))
            target = neutral_targets[0]
            
            self.log(f"    -> Warrior1 expands to ({target.x},{target.y})")
            if self.send_place(Resident.Warrior1, province.castle_hex, target):
                province.money -= UNIT_PRICES[Resident.Warrior1]
                self.units_built_this_turn += 1
                # Remove from neutral_lands list
                if target in self.neutral_lands:
                    self.neutral_lands.remove(target)
            else:
                self.failed_targets.add((target.x, target.y))
                break
    
    def count_friendly_neighbors(self, hex_obj) -> int:
        """Count how many friendly hexes are adjacent."""
        count = 0
        neighbors = GameUtils.get_hex_neighbors(hex_obj.x, hex_obj.y, self.board.width, self.board.height)
        for nx, ny in neighbors:
            adj = self.board.get_hex(nx, ny)
            if adj and adj.owner_id == self.my_player_id:
                count += 1
        return count
    
    def try_to_build_units_on_palms(self, province: Province):
        """Clear palms with new units (they pay for themselves +3)."""
        while province.can_afford_unit(1):
            palms = province.get_palms()
            if not palms:
                break
            
            target = palms[0]
            self.log(f"    -> Peasant clears palm at ({target.x},{target.y})")
            if self.send_place(Resident.Warrior1, province.castle_hex, target):
                province.money -= UNIT_PRICES[Resident.Warrior1]
            else:
                break
    
    def province_has_enough_income(self, province: Province, strength: int) -> bool:
        """Java: provinceHasEnoughIncomeForUnit() - check income sustainability."""
        upkeep = GameUtils.get_unit_upkeep(strength)
        new_income = province.get_income() - upkeep
        return new_income >= -2
    
    def can_province_build_unit(self, province: Province, strength: int) -> bool:
        """Check if province can afford to build unit."""
        resident = GameUtils.strength_to_warrior(strength)
        price = UNIT_PRICES.get(resident, 999)
        return province.money >= price
    
    def try_to_attack_with_strength(self, province: Province, strength: int) -> bool:
        """Build a unit and attack with it."""
        if not province.castle_hex:
            return False
        
        placeable = self.province_manager.get_placeable_hexes(province, strength)
        attack_targets = [h for h in placeable if h.owner_id != self.my_player_id]
        attack_targets = [h for h in attack_targets if (h.x, h.y) not in self.failed_targets]
        
        if not attack_targets:
            return False
        
        target = self.find_most_attractive_hex(attack_targets, strength)
        if not target:
            return False
        
        resident = GameUtils.strength_to_warrior(strength)
        self.log(f"    -> {resident.name} attacks ({target.x},{target.y})")
        
        if self.send_place(resident, province.castle_hex, target):
            province.money -= UNIT_PRICES[resident]
            self.units_built_this_turn += 1
            return True
        
        return False
    
    # ==================== MOVE AFK UNITS (Java: moveAfkUnits) ====================
    
    def move_afk_units(self, province: Province):
        """Java: moveAfkUnits() - move idle units to perimeter."""
        if len(province.hexes) < 15:
            return  # Small province, skip
        
        movable = province.get_movable_units()
        
        for unit_hex in movable:
            # If not on perimeter, try to move towards it
            if not self.is_in_perimeter(unit_hex):
                perimeter_hex = self.find_random_perimeter_hex(province)
                if perimeter_hex and GameUtils.hex_is_free(perimeter_hex.resident):
                    self.log(f"    Moving AFK unit to perimeter ({perimeter_hex.x},{perimeter_hex.y})")
                    self.send_move(unit_hex, perimeter_hex)
    
    def find_random_perimeter_hex(self, province: Province) -> Optional:
        """Find a random hex on the perimeter."""
        perimeter = [h for h in province.hexes if self.is_in_perimeter(h)]
        if perimeter:
            return random.choice(perimeter)
        return None
    
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
                    self.log(f"      MOVE REJECTED: ({from_hex.x},{from_hex.y}) -> ({to_hex.x},{to_hex.y})")
                    self.failed_targets.add((to_hex.x, to_hex.y))
                else:
                    self.log(f"      Move OK: ({from_hex.x},{from_hex.y}) -> ({to_hex.x},{to_hex.y})")
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
