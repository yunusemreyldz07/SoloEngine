#!/usr/bin/env python3
"""
SoloEngine Self-Play Legal Move Tester
Tests engine for illegal moves by playing games against itself
"""

import subprocess
import sys
import chess
import time

def run_engine_command(process, command):
    """Send command to engine and get response"""
    process.stdin.write(command + "\n")
    process.stdin.flush()

def get_bestmove(process, position_cmd, depth=6, timeout=30):
    """Get bestmove from engine"""
    run_engine_command(process, position_cmd)
    run_engine_command(process, f"go depth {depth}")
    
    start = time.time()
    while time.time() - start < timeout:
        line = process.stdout.readline().strip()
        if line.startswith("bestmove"):
            parts = line.split()
            if len(parts) >= 2:
                return parts[1]
    return None

def is_legal_move(board, move_uci):
    """Check if move is legal in python-chess"""
    try:
        move = chess.Move.from_uci(move_uci)
        return move in board.legal_moves
    except:
        return False

def selfplay_game(engine_path, depth=6, max_moves=200, random_opening=8):
    """Play one game, return list of (move, legal) tuples"""
    import random
    
    process = subprocess.Popen(
        [engine_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )
    
    # Init UCI
    run_engine_command(process, "uci")
    time.sleep(0.1)
    run_engine_command(process, "isready")
    
    # Wait for readyok
    while True:
        line = process.stdout.readline().strip()
        if line == "readyok":
            break
    
    board = chess.Board()
    moves = []
    results = []
    
    # Random opening - play N random legal moves
    for _ in range(random_opening):
        if board.is_game_over():
            break
        legal_moves = list(board.legal_moves)
        if not legal_moves:
            break
        random_move = random.choice(legal_moves)
        board.push(random_move)
        moves.append(random_move.uci())
        results.append((len(moves), random_move.uci(), True, "RANDOM_OPENING"))
    
    for move_num in range(max_moves):
        if board.is_game_over():
            break
        
        # Build position command
        if moves:
            position_cmd = f"position startpos moves {' '.join(moves)}"
        else:
            position_cmd = "position startpos"
        
        # Get engine move
        move_uci = get_bestmove(process, position_cmd, depth)
        
        if not move_uci or move_uci == "0000":
            # Check if this is actually a draw position
            if board.is_stalemate() or board.is_insufficient_material() or \
               board.can_claim_draw() or board.is_seventyfive_moves() or \
               board.is_fivefold_repetition():
                results.append((move_num + 1, "0000", True, "DRAW_POSITION"))
            else:
                results.append((move_num + 1, "0000", False, "NULL_MOVE"))
            break
        
        # Check legality
        legal = is_legal_move(board, move_uci)
        
        if legal:
            board.push_uci(move_uci)
            moves.append(move_uci)
            results.append((move_num + 1, move_uci, True, "OK"))
        else:
            # Get all legal moves for debugging
            legal_moves = [m.uci() for m in board.legal_moves]
            results.append((move_num + 1, move_uci, False, f"ILLEGAL (legal: {legal_moves[:5]}...)"))
            break
    
    # Cleanup
    run_engine_command(process, "quit")
    process.wait(timeout=2)
    
    return results, moves, board.result() if board.is_game_over() else "*"

def run_tests(engine_path, num_games=10, depth=6):
    """Run multiple selfplay games and report results"""
    print(f"{'='*60}")
    print(f"SoloEngine Self-Play Legal Move Tester")
    print(f"{'='*60}")
    print(f"Engine: {engine_path}")
    print(f"Games: {num_games}")
    print(f"Depth: {depth}")
    print(f"{'='*60}\n")
    
    total_moves = 0
    legal_moves = 0
    illegal_moves = 0
    null_moves = 0
    failed_games = []
    
    for game_num in range(1, num_games + 1):
        print(f"Game {game_num}/{num_games}...", end=" ", flush=True)
        
        try:
            results, moves, result = selfplay_game(engine_path, depth)
            
            game_legal = 0
            game_illegal = 0
            game_null = 0
            
            for move_num, move, is_legal, status in results:
                total_moves += 1
                if status == "NULL_MOVE":
                    null_moves += 1
                    game_null += 1
                elif is_legal:
                    legal_moves += 1
                    game_legal += 1
                else:
                    illegal_moves += 1
                    game_illegal += 1
                    failed_games.append((game_num, move_num, move, status))
            
            status_str = "✅" if game_illegal == 0 and game_null == 0 else "❌"
            print(f"{status_str} {len(results)} moves, result: {result}")
            
            if game_illegal > 0 or game_null > 0:
                print(f"   ⚠️  Issues: {game_illegal} illegal, {game_null} null")
                for move_num, move, is_legal, status in results:
                    if not is_legal:
                        print(f"      Move {move_num}: {move} - {status}")
                        
        except Exception as e:
            print(f"❌ Error: {e}")
            failed_games.append((game_num, 0, "ERROR", str(e)))
    
    print(f"\n{'='*60}")
    print(f"SUMMARY")
    print(f"{'='*60}")
    print(f"Total moves: {total_moves}")
    print(f"Legal moves: {legal_moves} ({legal_moves*100/max(1,total_moves):.1f}%)")
    print(f"Illegal moves: {illegal_moves}")
    print(f"Null moves: {null_moves}")
    print(f"{'='*60}")
    
    if illegal_moves == 0 and null_moves == 0:
        print(f"✅ ALL MOVES LEGAL! Engine passed test.")
        return True
    else:
        print(f"❌ FAILED! Found {illegal_moves} illegal and {null_moves} null moves.")
        return False

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Test SoloEngine for illegal moves")
    parser.add_argument("--engine", default="./soloengine_mac", help="Path to engine")
    parser.add_argument("--games", type=int, default=10, help="Number of games")
    parser.add_argument("--depth", type=int, default=6, help="Search depth")
    args = parser.parse_args()
    
    success = run_tests(args.engine, args.games, args.depth)
    sys.exit(0 if success else 1)
