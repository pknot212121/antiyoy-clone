#!/bin/bash
# Simple wrapper to run game + receiver together for training

set -e

GAMES=${1:-1000}
OPPONENT=${2:-E}

echo "=================================================="
echo "   ANTIJOY TRAINING - SIMPLE MODE"
echo "=================================================="
echo "Games: $GAMES"
echo "Opponent: $OPPONENT"
echo "=================================================="
echo

# Update config
python3 - <<EOF
from pathlib import Path
config_content = """10 10
0
0 0
BB
-1 -1
2137
python3 receiver.py 127.0.0.1
2138
$OPPONENT
"""
Path("Antiyoy/config.txt").write_text(config_content)
print("✓ Config updated")
EOF

# Update TARGET_GAMES in Antiyoy/receiver.py
python3 - <<EOF
from pathlib import Path
receiver_path = Path("Antiyoy/receiver.py")
content = receiver_path.read_text()
lines = content.split('\n')
for i, line in enumerate(lines):
    if 'TARGET_GAMES =' in line:
        lines[i] = f'TARGET_GAMES = $GAMES  # Set by run_training.sh'
        break
receiver_path.write_text('\n'.join(lines))
print("✓ TARGET_GAMES set to $GAMES")
EOF

echo
echo "[Training] Starting game and receiver..."
echo "[Training] Press Ctrl+C to stop"
echo

# Cleanup function
cleanup() {
    echo
    echo "[Training] Stopping processes..."
    kill $GAME_PID 2>/dev/null || true
    kill $RECEIVER_PID 2>/dev/null || true
    wait $GAME_PID 2>/dev/null || true
    wait $RECEIVER_PID 2>/dev/null || true
    echo "[Training] Cleanup complete"
    exit 0
}

trap cleanup SIGINT SIGTERM

# Start game in background
./build/anti > /dev/null 2>&1 &
GAME_PID=$!
echo "[Training] Game started (PID: $GAME_PID)"

# Give game time to start
sleep 1

# Start receiver
python3 Antiyoy/receiver.py &
RECEIVER_PID=$!
echo "[Training] Receiver started (PID: $RECEIVER_PID)"
echo

# Monitor receiver with watchdog to detect stuck games
LAST_CHECK_TIME=$(date +%s)
LAST_MODIFIED=0
STUCK_COUNT=0

while kill -0 $RECEIVER_PID 2>/dev/null; do
    sleep 5
    
    # Check if rl_policy.json is being updated
    if [ -f "rl_policy.json" ]; then
        CURRENT_MODIFIED=$(stat -c %Y rl_policy.json 2>/dev/null || stat -f %m rl_policy.json 2>/dev/null || echo 0)
        CURRENT_TIME=$(date +%s)
        TIME_DIFF=$((CURRENT_TIME - LAST_CHECK_TIME))
        
        if [ "$CURRENT_MODIFIED" = "$LAST_MODIFIED" ] && [ $TIME_DIFF -gt 30 ]; then
            # No updates for 30 seconds - game might be stuck
            STUCK_COUNT=$((STUCK_COUNT + 1))
            if [ $STUCK_COUNT -ge 3 ]; then
                echo "[Watchdog] No progress for 90+ seconds, restarting processes..."
                kill -9 $GAME_PID 2>/dev/null || true
                kill -9 $RECEIVER_PID 2>/dev/null || true
                sleep 1
                
                # Restart
                ./build/anti > /dev/null 2>&1 &
                GAME_PID=$!
                sleep 1
                python3 Antiyoy/receiver.py &
                RECEIVER_PID=$!
                
                STUCK_COUNT=0
                echo "[Watchdog] Processes restarted (Game: $GAME_PID, Receiver: $RECEIVER_PID)"
            fi
        else
            STUCK_COUNT=0
        fi
        
        LAST_MODIFIED=$CURRENT_MODIFIED
        LAST_CHECK_TIME=$CURRENT_TIME
    fi
done

# Wait for receiver to complete
wait $RECEIVER_PID
RECEIVER_EXIT=$?

# Kill game if still running
kill $GAME_PID 2>/dev/null || true
wait $GAME_PID 2>/dev/null || true

echo
echo "=================================================="
echo "   Training Complete!"
echo "=================================================="

# Analyze results
python3 visualize_training.py --model rl_policy.json

exit $RECEIVER_EXIT
