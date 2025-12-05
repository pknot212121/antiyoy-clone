# ACTUAL WIN CONDITION

**Castles CANNOT be attacked directly!**

## How to Win in Antiyoy:

1. **Capture all enemy territory around their castle**
2. When a castle's "province" (connected territory) has only 1 hex (the castle itself), **the castle is automatically removed**
3. When a player has **0 castles**, they are eliminated

## Key code from board.cpp:

```cpp
// Line 276-278: Remove castle if isolated
if(province.size() == 1) // If only castle remains
{
    castlesToRemove.push_back(province[0]);
    continue;
}

// Line 789-791: Eliminate player with no castles  
if(board->getCountry(oldOwnerId)->getCastles().size() == 0)
{
    board->eliminateCountry(oldOwnerId);
}
```

## What This Means for Training:

**Wrong strategy:** Attack castles directly ❌  
**Right strategy:** Capture territory to isolate castles ✅

The agent must learn to:
1. **Expand territory aggressively** 
2. **Capture enemy hexes**
3. **Surround enemy castles**
4. **Wait for automatic castle removal**

This explains why 0 castles were captured in 160 games - the agents were trying to do something impossible!
