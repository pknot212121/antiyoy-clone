#!/usr/bin/env python3
"""
Test script for game rules validator.
Verifies that game rules correctly filter invalid actions.
"""

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from game_rules import GameRules, power, is_warrior, is_unmoved_warrior
from socket_client import Resident

def test_power_function():
    """Test that power function works correctly."""
    print("Testing power function...")
    assert power(Resident.WARRIOR1.value) == 1
    assert power(Resident.WARRIOR2.value) == 2
    assert power(Resident.WARRIOR3.value) == 3
    assert power(Resident.WARRIOR4.value) == 4
    assert power(Resident.FARM.value) == 0
    assert power(Resident.EMPTY.value) == -1
    print("  ✓ Power function works correctly")

def test_warrior_placement():
    """Test that warrior placement follows combat rules."""
    print("\nTesting warrior placement rules...")
    
    # Create simple board: Player 1 has a castle, player 2 has territory
    board_state = {}
    
    # Player 1 territory (5x5 in top-left)
    for y in range(5):
        for x in range(5):
            if x == 2 and y == 2:
                board_state[(x, y)] = {'owner_id': 1, 'resident': 'castle'}
            else:
                board_state[(x, y)] = {'owner_id': 1, 'resident': 'empty'}
    
    # Player 2 territory (adjacent)
    for y in range(5):
        for x in range(5, 10):
            board_state[(x, y)] = {'owner_id': 2, 'resident': 'empty'}
    
    # Player 2 has warrior3 at border
    board_state[(5, 2)] = {'owner_id': 2, 'resident': 'warrior3'}
    
    # Create rules validator
    rules = GameRules(board_state, 10, 5, {1: 100, 2: 100})
    
    # Test: Can place warrior1 on own territory
    valid = rules.get_valid_placements(1, Resident.WARRIOR1.value)
    assert len(valid) > 0, "Should be able to place warrior1 on own territory"
    assert (2, 2) not in valid, "Cannot place on castle"
    print("  ✓ Can place warriors on own territory")
    
    # Test: Cannot attack warrior3 with warrior2 (adjacent)
    # Warrior2 power=2 cannot attack warrior3 power=3 adjacent
    can_attack = rules.allows_warrior(5, 2, Resident.WARRIOR2.value, 1)
    assert not can_attack, "Warrior2 should not be able to attack Warrior3 with adjacency"
    print("  ✓ Combat power rules enforced (cannot attack stronger adjacent units)")
    
    # Test: Warrior4 can attack anything
    can_attack_w4 = rules.allows_warrior(5, 2, Resident.WARRIOR4.value, 1)
    assert can_attack_w4, "Warrior4 should be able to attack anything"
    print("  ✓ Warrior4 can attack any unit")

def test_farm_placement():
    """Test that farms must be adjacent to castle or other farms."""
    print("\nTesting farm placement rules...")
    
    # Create board with castle
    board_state = {}
    for y in range(5):
        for x in range(5):
            board_state[(x, y)] = {'owner_id': 1, 'resident': 'empty'}
    
    board_state[(2, 2)] = {'owner_id': 1, 'resident': 'castle'}
    
    rules = GameRules(board_state, 5, 5, {1: 100})
    
    # Get valid farm placements
    valid = rules.get_valid_placements(1, Resident.FARM.value)
    
    # Should only be hexes adjacent to castle
    # Neighbors of (2,2) in even column
    expected_neighbors = [(2, 1), (1, 1), (1, 2), (2, 3), (3, 2), (3, 1)]
    
    assert len(valid) > 0, "Should have valid farm placements"
    
    for pos in valid:
        # All valid positions should be neighbors of (2,2) or another farm
        assert pos in expected_neighbors, f"Farm at {pos} should be adjacent to castle"
    
    print(f"  ✓ Farms can only be placed adjacent to castle ({len(valid)} valid positions)")

def test_money_tracking():
    """Test that money requirements are enforced."""
    print("\nTesting money/affordability...")
    
    board_state = {}
    for y in range(3):
        for x in range(3):
            board_state[(x, y)] = {'owner_id': 1, 'resident': 'empty'}
    board_state[(1, 1)] = {'owner_id': 1, 'resident': 'castle'}
    
    # Player has only 20 money
    rules = GameRules(board_state, 3, 3, {1: 20})
    
    # Can afford warrior1 (cost 10) and warrior2 (cost 20)
    assert rules.can_afford(1, Resident.WARRIOR1.value), "Should afford warrior1"
    assert rules.can_afford(1, Resident.WARRIOR2.value), "Should afford warrior2"
    
    # Cannot afford warrior3 (cost 30)
    assert not rules.can_afford(1, Resident.WARRIOR3.value), "Should not afford warrior3"
    
    print("  ✓ Money requirements enforced correctly")

def test_movement_range():
    """Test that warriors can move max 3 hexes."""
    print("\nTesting movement range...")
    
    # Create board with warrior
    board_state = {}
    for y in range(10):
        for x in range(10):
            board_state[(x, y)] = {'owner_id': 1, 'resident': 'empty'}
    
    board_state[(5, 5)] = {'owner_id': 1, 'resident': 'warrior1'}
    board_state[(2, 2)] = {'owner_id': 1, 'resident': 'castle'}
    
    rules = GameRules(board_state, 10, 10, {1: 100})
    
    # Get valid movements from (5,5)
    valid = rules.get_valid_movements(5, 5)
    
    # Should have many valid positions (within 3 hex range in own territory)
    assert len(valid) > 6, f"Should have many valid moves, got {len(valid)}"
    
    # All should be within reasonable distance
    for (x, y) in valid:
        # Rough distance check (manhattan distance as proxy)
        dist = abs(x - 5) + abs(y - 5)
        assert dist <= 6, f"Position ({x},{y}) too far from (5,5)"
    
    print(f"  ✓ Movement range enforced ({len(valid)} valid destinations)")

def main():
    """Run all tests."""
    print("=" * 60)
    print("GAME RULES VALIDATOR TESTS")
    print("=" * 60)
    
    try:
        test_power_function()
        test_warrior_placement()
        test_farm_placement()
        test_money_tracking()
        test_movement_range()
        
        print("\n" + "=" * 60)
        print("✓ ALL TESTS PASSED")
        print("=" * 60)
        return 0
    except AssertionError as e:
        print(f"\n✗ TEST FAILED: {e}")
        import traceback
        traceback.print_exc()
        return 1
    except Exception as e:
        print(f"\n✗ ERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
