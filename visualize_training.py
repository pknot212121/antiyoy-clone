#!/usr/bin/env python3
"""
Training Visualization and Analysis Tool

Features:
- Real-time training progress visualization
- Performance metrics plotting
- Q-table analysis and statistics
- Win rate trends
- Convergence analysis

Usage:
    python3 visualize_training.py --model rl_policy_enhanced.json
    python3 visualize_training.py --live  # Monitor training in real-time
"""

import argparse
import json
import time
from pathlib import Path
from collections import defaultdict
from typing import Dict, List
import sys


def analyze_qtable(filepath: str) -> Dict:
    """Analyze Q-table statistics."""
    try:
        with open(filepath, 'r') as f:
            data = json.load(f)
        
        q_table = data.get('q_table_a', data.get('q_table', {}))
        
        # Basic stats
        num_states = len(q_table)
        
        # Q-value statistics
        all_q_values = []
        for state_key, q_values in q_table.items():
            all_q_values.extend(q_values)
        
        if all_q_values:
            avg_q = sum(all_q_values) / len(all_q_values)
            max_q = max(all_q_values)
            min_q = min(all_q_values)
            
            # Count positive vs negative Q-values
            positive = sum(1 for q in all_q_values if q > 0)
            negative = sum(1 for q in all_q_values if q < 0)
        else:
            avg_q = max_q = min_q = 0.0
            positive = negative = 0
        
        # Action preferences
        action_selected = defaultdict(int)
        for state_key, q_values in q_table.items():
            best_action = q_values.index(max(q_values))
            action_selected[best_action] += 1
        
        # Metadata
        epsilon = data.get('epsilon', data.get('metadata', {}).get('epsilon', 'N/A'))
        updates = data.get('updates', data.get('metadata', {}).get('updates', 'N/A'))
        
        return {
            'num_states': num_states,
            'avg_q': avg_q,
            'max_q': max_q,
            'min_q': min_q,
            'positive_q': positive,
            'negative_q': negative,
            'action_distribution': dict(action_selected),
            'epsilon': epsilon,
            'updates': updates,
        }
    
    except Exception as e:
        return {'error': str(e)}


def print_analysis(stats: Dict):
    """Print Q-table analysis."""
    if 'error' in stats:
        print(f"Error analyzing Q-table: {stats['error']}")
        return
    
    print("\n" + "="*80)
    print("Q-TABLE ANALYSIS".center(80))
    print("="*80)
    
    print(f"\n📊 BASIC STATISTICS")
    print(f"  States Learned: {stats['num_states']:,}")
    print(f"  Updates: {stats['updates']:,}" if isinstance(stats['updates'], int) else f"  Updates: {stats['updates']}")
    print(f"  Exploration (ε): {stats['epsilon']:.4f}" if isinstance(stats['epsilon'], float) else f"  Exploration: {stats['epsilon']}")
    
    print(f"\n💡 Q-VALUE STATISTICS")
    print(f"  Average Q: {stats['avg_q']:>8.2f}")
    print(f"  Max Q:     {stats['max_q']:>8.2f}")
    print(f"  Min Q:     {stats['min_q']:>8.2f}")
    print(f"  Positive:  {stats['positive_q']:>8,}")
    print(f"  Negative:  {stats['negative_q']:>8,}")
    
    if stats['action_distribution']:
        print(f"\n🎯 ACTION PREFERENCES (Best action per state)")
        action_names = ['ATTACK', 'DEFEND', 'FARM', 'EXPAND', 'WAIT']
        total = sum(stats['action_distribution'].values())
        
        for action_id in sorted(stats['action_distribution'].keys()):
            count = stats['action_distribution'][action_id]
            pct = count / total * 100 if total > 0 else 0
            action_name = action_names[action_id] if action_id < len(action_names) else f"Action{action_id}"
            bar = '█' * int(pct / 2)
            print(f"  {action_name:>8}: {count:>5,} ({pct:>5.1f}%) {bar}")
    
    print("\n" + "="*80 + "\n")


def monitor_training(model_path: str, interval: int = 10):
    """Monitor training progress in real-time."""
    print("="*80)
    print("REAL-TIME TRAINING MONITOR".center(80))
    print("="*80)
    print(f"Watching: {model_path}")
    print(f"Update interval: {interval}s")
    print("Press Ctrl+C to stop\n")
    
    last_states = 0
    last_updates = 0
    last_time = time.time()
    
    try:
        while True:
            if Path(model_path).exists():
                stats = analyze_qtable(model_path)
                
                if 'error' not in stats:
                    current_time = time.time()
                    elapsed = current_time - last_time
                    
                    # Calculate rates
                    states_delta = stats['num_states'] - last_states
                    updates_delta = stats['updates'] - last_updates if isinstance(stats['updates'], int) else 0
                    
                    states_per_sec = states_delta / elapsed if elapsed > 0 else 0
                    updates_per_sec = updates_delta / elapsed if elapsed > 0 else 0
                    
                    # Print status
                    timestamp = time.strftime("%H:%M:%S")
                    updates_str = f"{stats['updates']:>8,}" if isinstance(stats['updates'], int) else f"{str(stats['updates']):>8}"
                    epsilon_str = f"{stats['epsilon']:.4f}" if isinstance(stats['epsilon'], float) else str(stats['epsilon'])
                    print(f"[{timestamp}] States: {stats['num_states']:>6,} (+{states_delta:>4,}) | "
                          f"Updates: {updates_str} | "
                          f"ε={epsilon_str} | "
                          f"Avg-Q: {stats['avg_q']:>7.2f}")
                    
                    last_states = stats['num_states']
                    last_updates = stats['updates'] if isinstance(stats['updates'], int) else 0
                    last_time = current_time
                else:
                    print(f"[{time.strftime('%H:%M:%S')}] Error: {stats['error']}")
            else:
                print(f"[{time.strftime('%H:%M:%S')}] Waiting for model file...")
            
            time.sleep(interval)
    
    except KeyboardInterrupt:
        print("\n\nMonitoring stopped.")


def compare_models(model_paths: List[str]):
    """Compare multiple Q-table models."""
    print("\n" + "="*80)
    print("MODEL COMPARISON".center(80))
    print("="*80 + "\n")
    
    results = []
    for path in model_paths:
        if Path(path).exists():
            stats = analyze_qtable(path)
            stats['path'] = path
            results.append(stats)
        else:
            print(f"⚠️  Model not found: {path}")
    
    if not results:
        print("No models to compare.")
        return
    
    # Print comparison table
    print(f"{'Model':<40} | {'States':>8} | {'Avg-Q':>8} | {'ε':>8}")
    print("-" * 80)
    
    for stats in results:
        if 'error' not in stats:
            model_name = Path(stats['path']).name
            print(f"{model_name:<40} | {stats['num_states']:>8,} | "
                  f"{stats['avg_q']:>8.2f} | "
                  f"{stats['epsilon']:>8.4f}" if isinstance(stats['epsilon'], float) else f"{'N/A':>8}")
    
    print("\n" + "="*80 + "\n")


def export_statistics(model_path: str, output_path: str):
    """Export detailed statistics to JSON."""
    stats = analyze_qtable(model_path)
    
    if 'error' not in stats:
        with open(output_path, 'w') as f:
            json.dump(stats, f, indent=2)
        print(f"✓ Statistics exported to: {output_path}")
    else:
        print(f"✗ Error: {stats['error']}")


def main():
    parser = argparse.ArgumentParser(
        description='Analyze and visualize Q-learning training',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument('--model', type=str, default='rl_policy.json',
                        help='Path to Q-table model file')
    parser.add_argument('--live', action='store_true',
                        help='Monitor training in real-time')
    parser.add_argument('--interval', type=int, default=10,
                        help='Update interval for live monitoring (seconds)')
    parser.add_argument('--compare', nargs='+',
                        help='Compare multiple models')
    parser.add_argument('--export', type=str,
                        help='Export statistics to JSON file')
    
    args = parser.parse_args()
    
    if args.compare:
        compare_models(args.compare)
    elif args.live:
        monitor_training(args.model, args.interval)
    elif args.export:
        export_statistics(args.model, args.export)
    else:
        # Single model analysis
        if not Path(args.model).exists():
            print(f"Error: Model file not found: {args.model}")
            sys.exit(1)
        
        stats = analyze_qtable(args.model)
        print_analysis(stats)
        
        # Suggest next steps
        if 'error' not in stats:
            print("💡 SUGGESTIONS:")
            
            if stats['num_states'] < 100:
                print("  • Low state count - train for more games to explore the state space")
            
            if isinstance(stats['epsilon'], float) and stats['epsilon'] > 0.3:
                print("  • High exploration rate - continue training to reduce epsilon")
            
            if stats['avg_q'] < 0:
                print("  • Negative average Q - model is learning losses, continue training")
            elif stats['avg_q'] > 50:
                print("  • High average Q - model is learning to win consistently!")
            
            # Check action distribution balance
            if stats['action_distribution']:
                counts = list(stats['action_distribution'].values())
                if max(counts) > sum(counts) * 0.6:
                    print("  • Imbalanced action distribution - model may be overfitting to one strategy")
            
            print()


if __name__ == "__main__":
    main()
