import sys
import socket
import struct
import select
import random
import math
from collections import deque

from enum import IntEnum

# Force unbuffered output
import functools
print = functools.partial(print, flush=True)

# Nazewnictwo i kolejność odpowiadają tym z gry, ich zmiana może uszkodzić rozczytywanie planszy
class Resident(IntEnum):
    Water = 0 # Woda liczy się jako rezydent
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

    def is_unit(self):
        return self in [Resident.Warrior1, Resident.Warrior2, Resident.Warrior3, Resident.Warrior4,
                        Resident.Warrior1Moved, Resident.Warrior2Moved, Resident.Warrior3Moved, Resident.Warrior4Moved]
    
    def is_ready_to_move(self):
        return self in [Resident.Warrior1, Resident.Warrior2, Resident.Warrior3, Resident.Warrior4]

    def get_strength(self):
        if self in [Resident.Warrior1, Resident.Warrior1Moved]: return 1
        if self in [Resident.Warrior2, Resident.Warrior2Moved]: return 2
        if self in [Resident.Warrior3, Resident.Warrior3Moved]: return 3
        if self in [Resident.Warrior4, Resident.Warrior4Moved]: return 4
        return 0
    
    def is_building(self):
        return self in [Resident.Farm, Resident.Tower, Resident.StrongTower, Resident.Castle]
    
    def is_empty(self):
        return self == Resident.Empty
    
    def is_tree(self):
        return self in [Resident.PalmTree, Resident.PineTree]

class Hex:
    def __init__(self, x, y, owner_id, resident, money):
        self.x = x
        self.y = y
        self.owner_id = owner_id
        self.resident = resident
        self.money = money

    def __repr__(self):
        return f"Hex(x={self.x}, y={self.y}, owner={self.owner_id}, resident={self.resident}, money={self.money})"

    def get_neighbors(self, board):
        neighbors = []
        # Even x (columns 0, 2, 4...)
        even_directions = [
            (0, -1), (-1, -1), (-1, 0), (0, 1), (1, 0), (1, -1)
        ]
        # Odd x (columns 1, 3, 5...)
        odd_directions = [
            (0, -1), (-1, 0), (-1, 1), (0, 1), (1, 1), (1, 0)
        ]
        
        directions = even_directions if self.x % 2 == 0 else odd_directions
        
        for dx, dy in directions:
            nx, ny = self.x + dx, self.y + dy
            if 0 <= nx < board.width and 0 <= ny < board.height:
                neighbors.append(board.get_hex(nx, ny))
        return neighbors


class Board:
    def __init__(self, width, height):
        self.width = width
        self.height = height
        self.hexes = []

    def add_hex(self, hexagon):
        self.hexes.append(hexagon)

    def get_hex(self, x, y):
        if 0 <= x < self.width and 0 <= y < self.height:
            return self.hexes[y * self.width + x]
        return None

    def __repr__(self):
        return f"Board({self.width}x{self.height}, {len(self.hexes)} hexes)"
    
    def print_owners(self):
        print("Owners:")
        for y in range(self.height):
            row = []
            for x in range(self.width):
                h = self.hexes[y * self.width + x]
                row.append(f"{h.owner_id:2d}")
            print(" ".join(row))
        print()

    def print_residents(self):
        print("Residents:")
        for y in range(self.height):
            row = []
            for x in range(self.width):
                h = self.hexes[y * self.width + x]
                row.append(f"{h.resident.value:2d}")
            print(" ".join(row))
        print()

    def print_money(self):
        print("Money:")
        for y in range(self.height):
            row = []
            for x in range(self.width):
                h = self.hexes[y * self.width + x]
                row.append(f"{h.money:3d}")
            print(" ".join(row))
        print()

    def swap_players(self, id1, id2):
        for h in self.hexes:
            if h.owner_id == id1:
                h.owner_id = id2
            elif h.owner_id == id2:
                h.owner_id = id1

    def get_provinces(self, player_id):
        provinces = []
        visited = set()
        
        for h in self.hexes:
            if h.owner_id == player_id and h not in visited:
                # Start BFS/DFS to find connected component
                province_hexes = []
                queue = deque([h])
                visited.add(h)
                while queue:
                    curr = queue.popleft()
                    province_hexes.append(curr)
                    for n in curr.get_neighbors(self):
                        if n.owner_id == player_id and n not in visited:
                            visited.add(n)
                            queue.append(n)
                provinces.append(Province(province_hexes, self))
        return provinces


class Province:
    def __init__(self, hex_list, board):
        self.hex_list = hex_list
        self.board = board
        self.capital = self._find_capital()
        self.money = self.capital.money if self.capital else 0
        self.fraction = self.hex_list[0].owner_id if self.hex_list else 0

    def _find_capital(self):
        for h in self.hex_list:
            if h.resident == Resident.Castle:
                return h
        return self.hex_list[0] if self.hex_list else None

    def can_afford_unit(self, strength):
        cost = 10 * strength
        return self.money >= cost

    def has_money_for_tower(self):
        return self.money >= 15
    
    def has_money_for_farm(self):
        return self.money >= self.get_current_farm_price()
    
    def get_current_farm_price(self):
        """Calculate current farm price - increases with each farm"""
        farm_count = sum(1 for h in self.hex_list if h.resident == Resident.Farm)
        return 15 + farm_count * 6
    
    def get_extra_farm_cost(self):
        """Calculate extra upkeep cost from farms - used to limit farm spam"""
        farm_count = sum(1 for h in self.hex_list if h.resident == Resident.Farm)
        if farm_count <= 1:
            return 0
        # Each farm beyond the first costs 6 extra upkeep
        return (farm_count - 1) * 6

    def get_units(self):
        return [h for h in self.hex_list if h.resident.is_unit()]


class AiBase:
    def __init__(self, player_id):
        self.player_id = player_id
        self.targeted_hexes = set()  # Track hexes targeted this turn

    def make_move(self, board, action_builder):
        pass

class AiEasy(AiBase):
    def make_move(self, board, action_builder):
        print("[AI] make_move started")
        # Clear targeted hexes at the start of turn
        self.targeted_hexes = set()
        
        provinces = board.get_provinces(self.player_id)
        print(f"[AI] Found {len(provinces)} provinces for player {self.player_id}")
        for i, province in enumerate(provinces):
            if province.capital:
                print(f"[AI] Province {i}: {len(province.hex_list)} hexes, capital at ({province.capital.x}, {province.capital.y}) with money {province.money}")
            else:
                print(f"[AI] Province {i}: {len(province.hex_list)} hexes, NO CAPITAL, money {province.money}")
            if not province.hex_list:
                print(f"[AI] Skipping empty province")
                continue
            self.move_units(province, board, action_builder)
            # TODO: Farm and tower building require BUILD action in game engine
            # Currently only PLACE and MOVE actions are supported
            # self.try_to_build_towers(province, board, action_builder)
            # self.try_to_build_farms(province, board, action_builder)
            self.spend_money_and_merge(province, board, action_builder)

    def move_units(self, province, board, action_builder):
        units = [h for h in province.hex_list if h.resident.is_unit() and h.resident.is_ready_to_move()]
        print(f"[AI] Found {len(units)} units ready to move in province")
        for unit_hex in units:
            print(f"[AI] ========================================")
            print(f"[AI] Processing unit at ({unit_hex.x}, {unit_hex.y}), strength={unit_hex.resident.get_strength()}")
            if not unit_hex.resident.is_ready_to_move(): continue
            
            move_zone = self.detect_move_zone(unit_hex, unit_hex.resident.get_strength(), board)
            print(f"[AI] Move zone has {len(move_zone)} hexes")
            
            # Log what's in the move zone
            neutral_count = sum(1 for h in move_zone if h.owner_id == 0)
            enemy_count = sum(1 for h in move_zone if h.owner_id != 0 and h.owner_id != self.player_id)
            friendly_count = sum(1 for h in move_zone if h.owner_id == self.player_id)
            print(f"[AI] Move zone: {neutral_count} neutral, {enemy_count} enemy, {friendly_count} friendly")
            
            self.exclude_friendly_units(move_zone, self.player_id)
            self.exclude_friendly_buildings(move_zone, self.player_id)
            
            print(f"[AI] After filtering: {len(move_zone)} valid targets")
            if not move_zone:
                print(f"[AI] No valid targets, skipping unit")
                continue
            
            self.decide_about_unit(unit_hex, move_zone, province, action_builder)

    def decide_about_unit(self, unit_hex, move_zone, province, action_builder):
        # Cleaning palms has highest priority (for weak units)
        strength = unit_hex.resident.get_strength()
        if strength <= 2 and self.check_to_clean_some_palms(unit_hex, move_zone, province, action_builder):
            return
        
        # Find attackable hexes (enemy or neutral territory that we can actually conquer)
        # Also exclude hexes that have already been targeted this turn
        attackable_hexes = [h for h in self.find_attackable_hexes(move_zone, strength, province.board)
                           if (h.x, h.y) not in self.targeted_hexes]
        
        print(f"[AI] Unit at ({unit_hex.x}, {unit_hex.y}): {len(attackable_hexes)} attackable hexes")
        if attackable_hexes:
            for h in attackable_hexes[:5]:  # Log first 5 targets
                print(f"[AI]   Attackable: ({h.x}, {h.y}) owner={h.owner_id} resident={h.resident}")
        
        if attackable_hexes:
            # Attack the most attractive hex
            best_hex = self.find_most_attractive_hex(attackable_hexes, province, strength, province.board)
            if best_hex:
                print(f"[AI] ✓ Unit ATTACKING from ({unit_hex.x}, {unit_hex.y}) to ({best_hex.x}, {best_hex.y}) (owner={best_hex.owner_id})")
                action_builder.add_move(unit_hex.x, unit_hex.y, best_hex.x, best_hex.y)
                self.targeted_hexes.add((best_hex.x, best_hex.y))  # Mark as targeted
                return
        else:
            print(f"[AI] No attackable hexes for this unit")
            # Nothing to attack - try to clean trees
            cleaned_trees = self.check_to_clean_some_trees(unit_hex, move_zone, province, action_builder)
            if cleaned_trees:
                print(f"[AI] ✓ Unit cleaned trees")
            elif self.is_in_perimeter(unit_hex, province.board):
                print(f"[AI] Unit on perimeter, pushing to better defense")
                self.push_unit_to_better_defense(unit_hex, move_zone, province, action_builder)
            else:
                print(f"[AI] Unit has nothing to do")

    def find_attackable_hexes(self, move_zone, strength, board):
        """Find all hexes that are not owned by us and that we can conquer"""
        return [h for h in move_zone if h.owner_id != self.player_id and self.can_conquer(h, strength, board)]

    def check_to_clean_some_palms(self, unit_hex, move_zone, province, action_builder):
        for h in move_zone:
            if (h.resident == Resident.PalmTree and h.owner_id == self.player_id 
                and (h.x, h.y) not in self.targeted_hexes):  # Check if not already targeted
                print(f"[AI] Unit cleaning palm from ({unit_hex.x}, {unit_hex.y}) to ({h.x}, {h.y})")
                action_builder.add_move(unit_hex.x, unit_hex.y, h.x, h.y)
                self.targeted_hexes.add((h.x, h.y))  # Mark as targeted
                return True
        return False

    def is_in_perimeter(self, hex, board):
        """Check if hex is on the border of our territory"""
        for n in hex.get_neighbors(board):
            if n.owner_id != self.player_id and n.resident != Resident.Water:
                return True
        return False

    def push_unit_to_better_defense(self, unit_hex, move_zone, province, action_builder):
        """Move unit to a safer position away from enemies"""
        for h in move_zone:
            if h.owner_id == self.player_id and self.hex_is_free(h):
                # Check if this hex has no enemy neighbors
                enemy_neighbors = 0
                for n in h.get_neighbors(province.board):
                    if n.owner_id != self.player_id and n.owner_id != 0 and n.resident != Resident.Water:
                        enemy_neighbors += 1
                if enemy_neighbors == 0:
                    print(f"[AI] Unit retreating from ({unit_hex.x}, {unit_hex.y}) to ({h.x}, {h.y})")
                    action_builder.add_move(unit_hex.x, unit_hex.y, h.x, h.y)
                    return
    
    def hex_is_free(self, hex):
        """Check if hex is free for movement"""
        return (hex.resident == Resident.Empty or 
                hex.resident == Resident.Gravestone or
                hex.resident.is_tree())

    def check_to_clean_some_trees(self, unit_hex, move_zone, province, action_builder):
        for h in move_zone:
            if (h.resident.is_tree() and h.owner_id == self.player_id
                and (h.x, h.y) not in self.targeted_hexes):  # Check if not already targeted
                action_builder.add_move(unit_hex.x, unit_hex.y, h.x, h.y)
                self.targeted_hexes.add((h.x, h.y))  # Mark as targeted
                return True
        return False

    def move_towards_enemy(self, unit_hex, move_zone, province, action_builder):
        """Move unit towards the nearest enemy territory"""
        board = province.board
        
        # Find all enemy hexes on the board
        enemy_hexes = [h for h in board.hexes if h.owner_id != self.player_id and h.owner_id != 0 and h.resident != Resident.Water]
        if not enemy_hexes:
            return False
        
        # Find the best hex in move_zone that gets us closer to enemies
        best_hex = None
        best_distance = float('inf')
        
        # Calculate current distance to nearest enemy
        current_min_dist = float('inf')
        for eh in enemy_hexes:
            dist = abs(unit_hex.x - eh.x) + abs(unit_hex.y - eh.y)
            current_min_dist = min(current_min_dist, dist)
        
        # Find hex in move_zone that minimizes distance to enemies
        for h in move_zone:
            if h.owner_id != self.player_id:
                continue  # Only move within our territory
            if not self.hex_is_free(h):
                continue
            
            # Calculate minimum distance from this hex to any enemy
            min_dist = float('inf')
            for eh in enemy_hexes:
                dist = abs(h.x - eh.x) + abs(h.y - eh.y)
                min_dist = min(min_dist, dist)
            
            # Only consider if it gets us closer
            if min_dist < current_min_dist and min_dist < best_distance:
                best_distance = min_dist
                best_hex = h
        
        if best_hex:
            print(f"[AI] Unit moving towards enemy from ({unit_hex.x}, {unit_hex.y}) to ({best_hex.x}, {best_hex.y})")
            action_builder.add_move(unit_hex.x, unit_hex.y, best_hex.x, best_hex.y)
            return True
        
        return False

    def spend_money_and_merge(self, province, board, action_builder):
        print(f"[AI] spend_money_and_merge: money={province.money}")
        self.try_to_build_units_on_palms(province, board, action_builder)

        # Track units built this turn for this province
        units_built_this_turn = 0
        build_limit = self.get_build_limit_for_province(province)
        print(f"[AI] Build limit for this province: {build_limit}")

        # Try to attack with units of increasing strength
        for i in range(1, 5):
            print(f"[AI] Checking strength {i}, can_afford={province.can_afford_unit(i)}, money={province.money}, built={units_built_this_turn}/{build_limit}")
            if not province.can_afford_unit(i): 
                break  # Can't afford this strength, so can't afford higher ones either
            
            # Try to build multiple units of this strength (but respect build limit)
            while self.can_province_build_unit(province, i) and units_built_this_turn < build_limit:
                success = self.try_to_attack_with_strength(province, i, board, action_builder)
                if not success:
                    break  # Can't attack with this strength, try next strength level
                units_built_this_turn += 1
                print(f"[AI] Built unit, total this turn: {units_built_this_turn}/{build_limit}")

        # Kick start province - if we have few units, try to build one to attack
        if province.can_afford_unit(1) and len(province.get_units()) <= 1 and units_built_this_turn < build_limit:
             self.try_to_attack_with_strength(province, 1, board, action_builder)

        # Merge units (increased probability to help break stalemates)
        if random.random() < 0.5:  # Increased from 0.25 to 0.5
            self.merge_units(province, board, action_builder)
    
    def try_to_build_towers(self, province, board, action_builder):
        """Build towers for defense - Java: ArtificialIntelligence.tryToBuildTowers"""
        while province.has_money_for_tower():
            hex_for_tower = self.find_hex_that_needs_tower(province, board)
            if not hex_for_tower:
                return
            print(f"[AI] Building tower at ({hex_for_tower.x}, {hex_for_tower.y})")
            action_builder.add_build(Resident.Tower, hex_for_tower.x, hex_for_tower.y)
            province.money -= 15
            if province.capital:
                province.capital.money = province.money
            hex_for_tower.resident = Resident.Tower
    
    def find_hex_that_needs_tower(self, province, board):
        """Find a hex that would benefit from a tower - Java: ArtificialIntelligence.findHexThatNeedsTower"""
        for hex in province.hex_list:
            if self.need_tower_on_hex(hex, board):
                return hex
        return None
    
    def need_tower_on_hex(self, hex, board):
        """Check if a hex needs a tower - Java: ArtificialIntelligence.needTowerOnHex"""
        if not hex.resident.is_empty():
            return False
        
        # Tower should protect at least 5 hexes
        defense_gain = self.get_predicted_defense_gain_by_new_tower(hex, board)
        return defense_gain >= 5
    
    def get_predicted_defense_gain_by_new_tower(self, hex, board):
        """Calculate how many hexes would be protected by a tower here - Java: ArtificialIntelligence.getPredictedDefenseGainByNewTower"""
        count = 0
        
        # Count this hex if not already defended
        if not self.is_defended_by_tower(hex, board):
            count += 1
        
        # Count adjacent friendly hexes that aren't defended
        for neighbor in hex.get_neighbors(board):
            if neighbor.owner_id == hex.owner_id:
                if not self.is_defended_by_tower(neighbor, board):
                    count += 1
                # Penalize if neighbor already has a tower (redundant)
                if neighbor.resident in [Resident.Tower, Resident.StrongTower]:
                    count -= 1
        
        return count
    
    def try_to_build_farms(self, province, board, action_builder):
        """Build farms for income - Java: ArtificialIntelligenceGeneric.tryToBuildFarms"""
        MAX_EXTRA_FARM_COST = 80
        
        # Don't build too many farms if upkeep is too high
        if province.get_extra_farm_cost() > MAX_EXTRA_FARM_COST:
            return
        
        while province.has_money_for_farm():
            # Check if it's a good time to build a farm
            if not self.is_ok_to_build_new_farm(province, board):
                return
            
            hex_for_farm = self.find_good_hex_for_farm(province, board)
            if not hex_for_farm:
                return
            
            print(f"[AI] Building farm at ({hex_for_farm.x}, {hex_for_farm.y})")
            action_builder.add_build(Resident.Farm, hex_for_farm.x, hex_for_farm.y)
            province.money -= 15
            if province.capital:
                province.capital.money = province.money
            hex_for_farm.resident = Resident.Farm
    
    def is_ok_to_build_new_farm(self, province, board):
        """Check if it's a good time to build a new farm - Java: ArtificialIntelligenceGeneric.isOkToBuildNewFarm"""
        # If we have lots of money, always ok
        if province.money > 2 * province.get_current_farm_price():
            return True
        
        # If there are towers that need building, prioritize them
        if self.find_hex_that_needs_tower(province, board) is not None:
            return False
        
        return True
    
    def find_good_hex_for_farm(self, province, board):
        """Find a good hex for a farm - Java: ArtificialIntelligenceGeneric.findGoodHexForFarm"""
        # Check if province has any good hexes for farms
        candidates = [h for h in province.hex_list if self.is_hex_good_for_farm(h, board)]
        if not candidates:
            return None
        
        # Pick a random good hex
        return random.choice(candidates)
    
    def is_hex_good_for_farm(self, hex, board):
        """Check if a hex is good for a farm - Java: ArtificialIntelligenceGeneric.isHexGoodForFarm"""
        if hex.resident != Resident.Empty:
            return False
        
        # Farm must be adjacent to castle or another farm
        for neighbor in hex.get_neighbors(board):
            if neighbor.owner_id == hex.owner_id:
                if neighbor.resident in [Resident.Castle, Resident.Farm]:
                    return True
        
        return False
    
    def get_build_limit_for_province(self, province):
        """Get maximum units that can be built per turn for a province"""
        # Match Java implementation: max(3, province_size / 4), capped at 10
        bottom = max(3, len(province.hex_list) // 4)
        return min(bottom, 10)

    def try_to_build_units_on_palms(self, province, board, action_builder):
        if not province.can_afford_unit(1): return
        if not province.capital: return

        max_iterations = 10  # Safety limit
        iterations = 0
        while self.can_province_build_unit(province, 1) and iterations < max_iterations:
            iterations += 1
            # Get move zone from capital to find reachable palms
            move_zone = self.detect_move_zone(province.capital, 1, board)
            killed_palm = False
            for h in move_zone:
                if h.resident == Resident.PalmTree and h.owner_id == self.player_id:
                    print(f"[AI] Building unit on palm at ({h.x}, {h.y})")
                    action_builder.add_place(Resident.Warrior1, province.capital.x, province.capital.y, h.x, h.y)
                    province.money -= 10
                    if province.capital:  # Update the actual hex money
                        province.capital.money = province.money
                    h.resident = Resident.Warrior1Moved  # Mark as no longer a palm
                    killed_palm = True
                    break
            if not killed_palm: break

    def merge_units(self, province, board, action_builder):
        for h in province.hex_list:
            if h.resident.is_unit() and h.resident.is_ready_to_move():
                self.try_to_merge_with_someone(province, h, board, action_builder)

    def try_to_merge_with_someone(self, province, unit_hex, board, action_builder):
        move_zone = self.detect_move_zone(unit_hex, unit_hex.resident.get_strength(), board)
        if not move_zone: return
        
        for target in move_zone:
            if self.merge_conditions(province, unit_hex, target):
                action_builder.add_move(unit_hex.x, unit_hex.y, target.x, target.y)
                break

    def merge_conditions(self, province, unit_hex, target_hex):
        if target_hex.owner_id != self.player_id: return False
        if not target_hex.resident.is_unit(): return False
        if not target_hex.resident.is_ready_to_move(): return False
        if unit_hex == target_hex: return False
        
        # Check if merge is possible (sum of strengths <= 4)
        new_strength = unit_hex.resident.get_strength() + target_hex.resident.get_strength()
        if new_strength > 4: return False
        
        # AiEasy specific: only merge 1+1
        if unit_hex.resident.get_strength() != 1 or target_hex.resident.get_strength() != 1:
            return False
            
        if not province.can_afford_unit(new_strength): return False
        return True

    def can_province_build_unit(self, province, strength):
        return province.can_afford_unit(strength)

    def try_to_build_unit_inside_province(self, province, strength, board, action_builder):
        for h in province.hex_list:
            if self.nothing_blocks_way_for_unit(h) and self.is_allowed_to_build_new_unit(province):
                if province.capital:
                    resident = self.get_unit_resident(strength)
                    action_builder.add_place(resident, province.capital.x, province.capital.y, h.x, h.y)
                    return True
        return False

    def try_to_attack_with_strength(self, province, strength, board, action_builder):
        if not province.capital: return False
        
        #Find all hexes reachable from capital with this unit strength
        move_zone = self.detect_move_zone(province.capital, strength, board)
        
        # Find attackable hexes (enemy/neutral that we can conquer)
        attackable_hexes = [h for h in move_zone 
                           if h.owner_id != self.player_id 
                           and h.resident != Resident.Water
                           and self.can_conquer(h, strength, board)
                           and (h.x, h.y) not in self.targeted_hexes]
        
        print(f"[AI] try_to_attack_with_strength({strength}): province_hexes={len(province.hex_list)}, attackable={len(attackable_hexes)}, money={province.money}")
        if attackable_hexes and len(attackable_hexes) <= 5:
            for h in attackable_hexes:
                print(f"[AI]   Can place strength-{strength} at ({h.x}, {h.y}) owner={h.owner_id}")
        
        if not attackable_hexes:
            print(f"[AI] No hexes attackable with strength {strength}")
            return False
        
        best_hex = self.find_most_attractive_hex(attackable_hexes, province, strength, board)
        if best_hex and province.capital:
            resident = self.get_unit_resident(strength)
            print(f"[AI] Placing unit {resident} from capital ({province.capital.x}, {province.capital.y}) to ({best_hex.x}, {best_hex.y})")
            action_builder.add_place(resident, province.capital.x, province.capital.y, best_hex.x, best_hex.y)
            province.money -= 10 * strength
            if province.capital:  # Update the actual hex money to keep in sync
                province.capital.money = province.money
            self.targeted_hexes.add((best_hex.x, best_hex.y))  # Mark as targeted
            return True
        return False

    def get_possible_placements(self, province, strength, board):
        valid = []
        
        # Check internal placements
        for h in province.hex_list: 
             if self.can_place_on(h, strength, board, self.player_id):
                 valid.append(h)
        
        # Check border placements
        for h in province.hex_list:
            for n in h.get_neighbors(board):
                if n.owner_id != self.player_id:
                    if self.can_place_on(n, strength, board, self.player_id):
                        valid.append(n)
        
        return list(set(valid))

    def can_place_on(self, hex, strength, board, owner_id):
        if hex.resident == Resident.Water: return False
        if hex.owner_id == owner_id:
            return True
        
        # Attacking enemy hex - check defense
        attacker_power = strength
        if attacker_power == 4: return True
        
        # Check if defended by tower in neighboring hex
        for n in hex.get_neighbors(board):
            if n.owner_id == hex.owner_id:
                if n.resident == Resident.Tower and attacker_power < 3:
                    return False
                if n.resident == Resident.StrongTower and attacker_power < 4:
                    return False
        
        # Check resident defense on the hex itself
        defender_strength = self.get_defense_strength(hex)
        if attacker_power <= defender_strength:
            return False
        
        return True

    def get_defense_strength(self, hex):
        """Returns the defense strength of a hex based on its resident"""
        if hex.resident == Resident.Tower: return 2
        if hex.resident == Resident.StrongTower: return 3
        if hex.resident == Resident.Castle: return 1
        if hex.resident.is_unit(): return hex.resident.get_strength()
        return 0

    def nothing_blocks_way_for_unit(self, hex):
        """Check if a hex is free for a new unit placement"""
        if hex.resident == Resident.Water: return False
        if hex.resident.is_unit(): return False
        if hex.resident.is_building(): return False
        if hex.resident == Resident.Gravestone: return False
        return True

    def is_allowed_to_build_new_unit(self, province):
        """Check if the province is allowed to build a new unit"""
        # For simple AI, always allow
        return True

    def get_unit_resident(self, strength):
        if strength == 1: return Resident.Warrior1
        if strength == 2: return Resident.Warrior2
        if strength == 3: return Resident.Warrior3
        if strength == 4: return Resident.Warrior4
        return Resident.Warrior1

    def detect_move_zone(self, start_hex, strength, board):
        """
        Detect all hexes reachable from start_hex within movement limit.
        Units can only move through friendly territory, but can step into
        enemy/neutral territory as the final destination (if they can conquer it).
        """
        limit = 4
        visited = {start_hex: 0}
        queue = deque([start_hex])
        move_zone = []
        
        while queue:
            curr = queue.popleft()
            dist = visited[curr]
            
            if dist > 0: 
                move_zone.append(curr)
            
            if dist >= limit: continue
            
            for n in curr.get_neighbors(board):
                if n in visited:
                    continue
                if n.resident == Resident.Water:
                    continue
                    
                # Check if we can enter this hex
                if n.owner_id == self.player_id:
                    # Friendly territory - can always pass through
                    visited[n] = dist + 1
                    queue.append(n)
                elif self.can_conquer(n, strength, board):
                    # Enemy/neutral territory - can enter but not pass through
                    visited[n] = dist + 1
                    move_zone.append(n)  # Add to move_zone immediately
                    # Don't add to queue - can't continue through enemy territory
        
        return move_zone
    
    def can_conquer(self, hex, strength, board):
        """Check if a unit with given strength can conquer this hex
        
        This MUST match C++'s Hexagon::allows() function exactly!
        """
        if hex.resident == Resident.Water:
            return False
        
        # If it's our own hex
        if hex.owner_id == self.player_id:
            if hex.resident.is_unit():
                # Check if we can merge warriors
                merged = self.merge_warriors(hex.resident, self.get_unit_resident(strength))
                return merged is not None
            # Can step on own land (power < 0), trees, graves
            power = self.get_defense_strength(hex)
            return power < 0 or hex.resident in [Resident.Empty, Resident.Gravestone, Resident.PalmTree, Resident.PineTree]
        
        # Enemy or neutral hex - check combat rules
        if strength == 4:
            return True  # Warrior4 crushes everything
        
        # CRITICAL: Check target hex AND ALL its neighbors for defenders
        # This matches C++'s allows() function exactly
        neighbors = hex.get_neighbors(board)
        all_hexes_to_check = neighbors + [hex]  # Include the hex itself
        
        for n in all_hexes_to_check:
            if n.owner_id == hex.owner_id:  # Same owner as target (defender)
                defender_power = self.get_defense_strength(n)
                if defender_power >= strength:
                    return False  # Defender blocks attack
        
        return True

    def can_move_to(self, hex, strength, board, owner_id):
        """Check if unit can move to this hex (used for existing unit movement)"""
        if hex.resident == Resident.Water:
            return False
        if hex.owner_id == owner_id:
            return True
        return self.can_conquer(hex, strength, board)

    def exclude_friendly_units(self, move_zone, player_id):
        for i in range(len(move_zone) - 1, -1, -1):
            h = move_zone[i]
            if h.owner_id == player_id and h.resident.is_unit():
                del move_zone[i]

    def exclude_friendly_buildings(self, move_zone, player_id):
        for i in range(len(move_zone) - 1, -1, -1):
            h = move_zone[i]
            if h.owner_id == player_id and h.resident.is_building():
                del move_zone[i]

    def find_most_attractive_hex(self, hexes, province, strength, board):
        if strength == 3 or strength == 4:
            h = self.find_hex_attractive_to_baron(hexes, strength, board)
            if h: return h

        result = None
        curr_max = -1
        for h in hexes:
            curr_num = self.get_attack_allure(h, province.fraction, board)
            if curr_num > curr_max:
                curr_max = curr_num
                result = h
        return result

    def find_hex_attractive_to_baron(self, hexes, strength, board):
        for h in hexes:
            if h.resident == Resident.Tower: return h
            if strength == 4 and h.resident == Resident.StrongTower: return h
        
        for h in hexes:
            if self.is_defended_by_tower(h, board): return h
        return None

    def is_defended_by_tower(self, hexagon, board):
        for n in hexagon.get_neighbors(board):
            if n.owner_id == hexagon.owner_id and (n.resident == Resident.Tower or n.resident == Resident.StrongTower):
                return True
        return False

    def get_attack_allure(self, hexagon, fraction, board):
        c = 0
        for n in hexagon.get_neighbors(board):
            if n.owner_id == fraction:
                c += 1
        # Prioritize enemy hexes over neutral (owner_id > 0 means it's a player's hex)
        if hexagon.owner_id > 0 and hexagon.owner_id != fraction:
            c += 10  # Big bonus for attacking enemy territory
        return c


MAGIC_SOCKET_TAG = 0 # Magiczne numerki wysyłane na początku do socketa by mieć pewność że jesteśmy odpowiednio połączeni
CONFIGURATION_SOCKET_TAG = 1 # Dane gry wysyłane przy rozpoczęciu nowej gry
BOARD_SOCKET_TAG = 2 # Plansza (spłaszczona dwuwymiarowa tablica heksagonów)
ACTION_SOCKET_TAG = 3 # Ruchy (Położenie, przesunięcie lub koniec tury)
CONFIRMATION_SOCKET_TAG = 4 # Potwierdzenie wysyłane przez grę po otrzymaniu ruchu składające się z 2 booleanów: czy zatwierdzono ruch oraz czy nadal wyczekuje ruchu
TURN_CHANGE_SOCKET_TAG = 5 # Numer gracza zaczynającego turę (zaczynając od 1, nie od 0 bo gra uznaje 0 za brak gracza)
PLAYER_ELIMINATED_SOCKET_TAG = 6 # Numer wyeliminowanego (przegranego) gracza (zaczynając od 1, nie od 0 bo gra uznaje 0 za brak gracza)
GAME_OVER_SOCKET_TAG = 7 # Numery graczy w kolejności od wygranego do pierwszego który odpadł

SOCKET_MAGIC_NUMBERS = b'ANTIYOY'

sock = None # Socket



def recv_size(sock, size):
    """Odbiera size bajtów lub rzuca wyjątek"""
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise RuntimeError("Socket disconnected during recv_all()")
        data.extend(chunk)
    return bytes(data)


def receive_magic() -> bool:
    """
    Odbiera magiczne numerki, zwraca bool czy pasują czy nie.
    Zakłada że tag został już odebrany
    """
    magic_len = len(SOCKET_MAGIC_NUMBERS)
    data = bytearray()
    while len(data) < magic_len:
        chunk = sock.recv(magic_len - len(data))
        if not chunk:
            raise RuntimeError("Failed to receive magic numbers")
        data.extend(chunk)

    return data == SOCKET_MAGIC_NUMBERS

def receive_config():
    """
    Odbiera GameConfigData.
    Zakłada że tag został już odebrany
    """

    # 1) x, y (uint16)
    header = recv_size(sock, 2 + 2)
    x, y = struct.unpack("!HH", header)

    # 2) seed, minProvinceSize, maxProvinceSize (uint32)
    numbers = recv_size(sock, 4 * 3)
    seed, minProv, maxProv = struct.unpack("!III", numbers)

    # 3) Rozmiar playerMarkers (bajt)
    size_player_markers = recv_size(sock, 1)[0]

    # 4) Zawartość playerMarkers
    markers_raw = recv_size(sock, size_player_markers)
    playerMarkers = markers_raw.decode("ascii")

    # 5) Rozmiar maxMoveTimes (bajt)
    size_move_times = recv_size(sock, 1)[0]

    # 6) Zawartość maxMoveTimes
    move_times_raw = recv_size(sock, 4 * size_move_times)
    maxMoveTimes = list(struct.unpack("!" + "I" * size_move_times, move_times_raw))

    return {
        "x": x,
        "y": y,
        "seed": seed,
        "minProvinceSize": minProv,
        "maxProvinceSize": maxProv,
        "playerMarkers": playerMarkers,
        "maxMoveTimes": maxMoveTimes,
    }

def receive_board() -> Board:
    """
    Odbiera planszę z pieniędzmi.
    Zakłada że tag został już odebrany
    """
    header = recv_size(sock, 4)
    width, height = struct.unpack("!HH", header)

    hexes_number = width * height

    # Każdy HexData ma 4 bajty:
    # ownerId (1)
    # resident (1)
    # money (uint16)
    data_size = hexes_number * 4

    data = recv_size(sock, data_size)

    board = Board(width, height)

    offset = 0
    for y in range(height):
        for x in range(width):
            owner_id = data[offset]
            resident_raw = data[offset + 1]
            money_raw = struct.unpack_from("!H", data, offset + 2)[0]

            resident = Resident(resident_raw)
            money = money_raw

            hexagon = Hex(x, y, owner_id, resident, money)
            board.add_hex(hexagon)

            offset += 4

    return board

def receive_action():
    """
    Odbiera zestaw akcji.
    Zakłada że tag został już odebrany
    """
    num_raw = recv_size(sock, 1)
    num = num_raw[0]

    actions = []

    for _ in range(num):
        action_type_raw = recv_size(sock, 1)
        action_type = action_type_raw[0]

        if action_type == ActionType.END_TURN:
            actions.append(action_type_raw)

        elif action_type == ActionType.PLACE:
            rest = recv_size(sock, 9)
            actions.append(action_type_raw + rest)

        elif action_type == ActionType.MOVE:
            rest = recv_size(sock, 8)
            actions.append(action_type_raw + rest)

        else:
            raise RuntimeError(f"Unknown action type received: {action_type}")

    return actions

def receive_confirmation() -> tuple[bool, bool]:
    """
    Odbiera potwierdzenie ruchu (czy zatwierdzono i czy oczekuje dalszych ruchów).
    Zakłada że tag został już odebrany
    """
    data = bytearray()
    while len(data) < 2:
        chunk = sock.recv(2 - len(data))
        if not chunk:
            raise RuntimeError("Failed to receive confirmation")
        data.extend(chunk)

    approved = bool(data[0])
    awaiting = bool(data[1])
    return approved, awaiting

def receive_turn_change() -> int:
    """
    Odbiera numer gracza zaczynającego turę.
    Zakłada że tag został już odebrany
    """
    data = sock.recv(1)
    if len(data) < 1:
        raise RuntimeError("Failed to receive player number (turn change)")
    return data[0]

def receive_player_eliminated() -> int:
    """
    Odbiera numer wyeliminowanego (przegranego) gracza.
    Zakłada że tag został już odebrany
    """
    data = sock.recv(1)
    if len(data) < 1:
        raise RuntimeError("Failed to receive player number (turn change)")
    return data[0]

def receive_game_over() -> list[int]:
    """
    Odbiera listę wyników (kolejność graczy od wygranego do ostatniego).
    Zakłada że tag został już odebrany
    """
    size_data = sock.recv(1)
    if len(size_data) < 1:
        raise RuntimeError("Failed to receive results array size")

    size = size_data[0]

    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise RuntimeError("Failed to receive full results data")
        data.extend(chunk)

    return list(data)


def receive_all():
    """
    Odbiera wszystko z socketa
    """
    result = []
    tag = sock.recv(1)
    while len(tag) > 0:
        tag = struct.unpack('B', tag)[0]

        if tag == MAGIC_SOCKET_TAG:
            result.append((MAGIC_SOCKET_TAG, receive_magic()))

        elif tag == CONFIGURATION_SOCKET_TAG:
            result.append((CONFIGURATION_SOCKET_TAG, receive_config()))

        elif tag == BOARD_SOCKET_TAG:
            result.append((BOARD_SOCKET_TAG, receive_board()))

        elif tag == ACTION_SOCKET_TAG:
            result.append((ACTION_SOCKET_TAG, receive_action()))

        elif tag == CONFIRMATION_SOCKET_TAG:
            result.append((CONFIRMATION_SOCKET_TAG, receive_confirmation()))

        elif tag == TURN_CHANGE_SOCKET_TAG:
            result.append((TURN_CHANGE_SOCKET_TAG, receive_turn_change()))

        elif tag == PLAYER_ELIMINATED_SOCKET_TAG:
            result.append((PLAYER_ELIMINATED_SOCKET_TAG, receive_player_eliminated()))

        elif tag == GAME_OVER_SOCKET_TAG:
            result.append((GAME_OVER_SOCKET_TAG, receive_game_over()))
        
        else: # Odebrano nieznany tag, socket najpewniej jest zawalony śmieciami z którymi nie wiemy co zrobić, nie możemy wydedukować stanu gry, trzeba zakończyć
            sock.close()
            raise RuntimeError("Received incorrect data")

        ready, _, _ = select.select([sock], [], [], 0)
        if not ready: # Jeśli nie ma więcej danych do pobrania kończymy
            return result

        tag = sock.recv(1)

    return result

def receive_next():
    """
    Odbiera jedną rzecz z socketa
    """
    result = (None, None)
    tag = sock.recv(1)
    if not tag:
        return result
    tag = struct.unpack('B', tag)[0]

    if tag == MAGIC_SOCKET_TAG:
        result = (MAGIC_SOCKET_TAG, receive_magic())

    elif tag == CONFIGURATION_SOCKET_TAG:
        result = (CONFIGURATION_SOCKET_TAG, receive_config())

    elif tag == BOARD_SOCKET_TAG:
        result = (BOARD_SOCKET_TAG, receive_board())

    elif tag == ACTION_SOCKET_TAG:
        result = (ACTION_SOCKET_TAG, receive_action())

    elif tag == CONFIRMATION_SOCKET_TAG:
        result = (CONFIRMATION_SOCKET_TAG, receive_confirmation())

    elif tag == TURN_CHANGE_SOCKET_TAG:
        result = (TURN_CHANGE_SOCKET_TAG, receive_turn_change())

    elif tag == PLAYER_ELIMINATED_SOCKET_TAG:
        result = (PLAYER_ELIMINATED_SOCKET_TAG, receive_player_eliminated())

    elif tag == GAME_OVER_SOCKET_TAG:
        result = (GAME_OVER_SOCKET_TAG, receive_game_over())
    
    else: # Odebrano nieznany tag, socket najpewniej jest zawalony śmieciami z którymi nie wiemy co zrobić, nie możemy wydedukować stanu gry, trzeba zakończyć
        sock.close()
        raise RuntimeError("Received incorrect data")

    return result


# Klasa do budowy odpowiedzi wysyłanej przez AI
class ActionType:
    END_TURN = 0
    PLACE = 1
    MOVE = 2

class ActionBuilder:

    def __init__(self):
        self.buffer = bytearray()
        self.num = 0

    def add_place(self, resident: int, x_from: int, y_from: int, x_to: int, y_to: int):
        """
        Położenie jednostki z pozycji (capital) na pozycję docelową
        """
        self.buffer.append(ActionType.PLACE)
        self.buffer.append(resident)
        self.buffer.extend(struct.pack("!HHHH", x_from, y_from, x_to, y_to))
        self.num += 1
        if self.num == 255:
            self.send()
    
    def add_build(self, resident: int, x: int, y: int):
        """Build a structure (farm or tower) on a hex"""
        self.buffer.append(ActionType.BUILD)
        self.buffer.append(resident)
        self.buffer.extend(struct.pack("!HH", x, y))
        self.num += 1
        if self.num == 255:
            self.send()

    def add_move(self, x_from: int, y_from: int, x_to: int, y_to: int):
        """
        Przesunięcie jednostki z pozycji na pozycję
        """
        self.buffer.append(ActionType.MOVE)
        self.buffer.extend(struct.pack("!HHHH", x_from, y_from, x_to, y_to))
        self.num += 1
        if self.num == 255:
            self.send()

    def add_end_turn(self):
        """
        Zakończenie tury, wstawienie tego od razu wywołuje send() (bo już nic dalej nie można zrobić)
        """
        self.buffer.append(ActionType.END_TURN)
        self.num += 1
        self.send()

    def send_from_line(self):
        """
        Wpisanie jednego ruchu z klawiatury.
        Obsługuje ruchy:
        et                                      ---> zakończenie tury
        p resident x_from y_from x_to y_to      ---> położenie
        m x_from y_from x_to y_to               ---> przesunięcie
        """

        line = input("\nInput action: ").strip().lower()
        if not line:
            self.send_from_line()
            return

        parts = line.split()
        cmd = parts[0]

        if cmd in ["et", "e", "t", "0"]:
            action_type = ActionType.END_TURN
        elif cmd in ["p", "1"]:
            action_type = ActionType.PLACE
        elif cmd in ["m", "2"]:
            action_type = ActionType.MOVE
        else:
            self.send_from_line()
            return

        if action_type == ActionType.END_TURN:
            self.add_end_turn()

        elif action_type == ActionType.PLACE:
            if len(parts) != 6:
                print("Format: p resident x_from y_from x_to y_to")
                self.send_from_line()
                return

            resident = int(parts[1])
            x_from = int(parts[2])
            y_from = int(parts[3])
            x_to = int(parts[4])
            y_to = int(parts[5])

            self.add_place(resident, x_from, y_from, x_to, y_to)
            self.send()

        elif action_type == ActionType.MOVE:
            if len(parts) != 5:
                print("Format: m x_from y_from x_to y_to")
                self.send_from_line()
                return

            x_from = int(parts[1])
            y_from = int(parts[2])
            x_to = int(parts[3])
            y_to = int(parts[4])

            self.add_move(x_from, y_from, x_to, y_to)
            self.send()

    def send(self):
        """
        Wysyła wszystkie ruchy jako jeden pakiet
        """
        if not self.buffer:
            return

        print(f"[DEBUG] Sending {self.num} action(s), buffer size: {len(self.buffer)} bytes")
        sock.sendall(bytes([ACTION_SOCKET_TAG]))
        sock.sendall(bytes([self.num]))
        sock.sendall(self.buffer)

        self.num = 0
        self.buffer.clear()



print("Started!")

# Program odpalany przez std::system("start python receiver.py 127.0.0.1 2137"); (nazwa, adres i port pochodzą z config.txt)
HOST = '127.0.0.1'
PORT = 2137

if len(sys.argv) >= 2:
    HOST = sys.argv[1]
if len(sys.argv) >= 3:
    PORT = int(sys.argv[2])


currentBotPlayer = 0
last_rejected_board_hash = None  # Track board hash where moves were rejected


sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    sock.connect((HOST, PORT))

    tag, payload = receive_next() # Na początku oczekujemy magicznych numerków

    if tag is None: # Jeśli nie otrzymamy danych
        print("Server disconnected")
        input()
        sys.exit(1)
    
    if tag != MAGIC_SOCKET_TAG: # Jeśli otrzymamy coś innego niż magiczne numerki
        print(f"Unexpected content received. Tag: {tag}")
        input()
        sys.exit(1)

    if payload: # Czy numerki się zgadzają
        print("Correct magic numbers!")
    else:
        print("Wrong magic numbers!")
        input()
        sys.exit(1)

    while True: # Pętla główna
        tag, payload = receive_next() # Na początku każdej gry mamy otrzymać konfigurację

        if tag is None: # Jeśli nie otrzymamy danych
            print("Server disconnected")
            input()
            sys.exit(1)
    
        if tag != CONFIGURATION_SOCKET_TAG: # Jeśli otrzymamy coś innego niż konfiguracja
            print(f"Unexpected content received. Tag: {tag}")
            input()
            sys.exit(1)

        print("Configuration received:") # Można coś zrobić z konfiguracją
        print(payload)

        while True: # Pętla meczu
            tag, payload = receive_next()

            if tag is None: # Jeśli nie otrzymamy danych
                print("Server disconnected")
                input()
                sys.exit(1)

            elif tag == ACTION_SOCKET_TAG: # Ruchy niebotowych graczy, botom raczej nie są one potrzebne
                print("Received action")

            elif tag == TURN_CHANGE_SOCKET_TAG: # Kiedy zaczynamy turę najpierw dostaniemy informację o zmianie tury
                currentBotPlayer = payload
                print("\n-----------------------------------------")
                print(f"Playing as Player {payload}")

            elif tag == BOARD_SOCKET_TAG: # Kiedy otrzymamy planszę to otrzymujemy ruch
                #global last_rejected_board_hash  # Declare at the top
                
                print(payload)
                payload.print_owners()
                payload.print_residents()
                payload.print_money()

                # DON'T swap players - use actual player ID
                # The swap was causing the bot to try to move enemy units!
                
                # Create a simple board hash to detect if we've seen this exact board before
                board_hash = hash((tuple(h.owner_id for h in payload.hexes), 
                                  tuple(h.resident for h in payload.hexes),
                                  tuple(h.money for h in payload.hexes)))
                
                # DODAĆ LOGIKĘ AI (najlepiej przez ActionBuilder)
                ab = ActionBuilder()
                
                # Check if we just got rejected on this exact board
                if last_rejected_board_hash is not None and last_rejected_board_hash == board_hash:
                    print(f"[AI] Skipping moves - was just rejected on this board")
                    last_rejected_board_hash = None  # Reset for next time
                else:
                    # Initialize AI with the actual current player ID
                    ai = AiEasy(currentBotPlayer)
                    try:
                        ai.make_move(payload, ab)
                    except Exception as e:
                        print(f"[AI ERROR] {e}")
                        import traceback
                        traceback.print_exc()
                
                # Send moves and handle responses
                still_awaiting = True
                moves_were_rejected = False
                while still_awaiting:
                    if ab.buffer and not moves_were_rejected:
                        ab.send()
                        tag, conf = receive_next()
                        if tag == CONFIRMATION_SOCKET_TAG:
                            approved, still_awaiting = conf
                            print(f"Approved: {approved}, Still awaiting: {still_awaiting}")
                            if not approved:
                                print(f"[AI] Move rejected, will send END_TURN on next board")
                                # Server already sent a new BOARD message after this rejection
                                # Store board hash so we skip moves if we see this board again
                                last_rejected_board_hash = board_hash
                                ab.buffer.clear()
                                ab.num = 0
                                moves_were_rejected = True
                                # Don't break - continue to send END_TURN
                        else:
                            print(f"[AI] Unexpected response: {tag}")
                            # Unexpected response - clear buffer and break
                            ab.buffer.clear()
                            ab.num = 0
                            break
                    else:
                        # No more moves to send (or moves were rejected), end turn
                        if moves_were_rejected:
                            print(f"[AI] Sending END_TURN after rejection")
                        else:
                            print(f"[AI] Sending END_TURN")
                        ab.add_end_turn()
                        # After END_TURN, break and let outer loop handle the response
                        break

            elif tag == PLAYER_ELIMINATED_SOCKET_TAG:
                print(f"Player {payload} eliminated!")
            
            elif tag == GAME_OVER_SOCKET_TAG: # Koniec gry
                print("Game over!\nLeaderboard:")
                for p in payload:
                    print(f"Player {p}")
                break # Koniec gry wyciąga nas z tej pętli (zaczynamy nowy mecz, oczekujemy nowej konfiguracji)

except Exception as e:
    print("Connection error", e)
    input()

if sock:
    sock.close()