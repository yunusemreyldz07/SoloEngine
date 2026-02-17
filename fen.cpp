#include <iostream>
#include <cstdint>
#include "fen.h"

int how_many_ply = 2;

namespace {
uint32_t fen_rng_state = 1804289383u;
}

static uint32_t get_random_u32_number() {
    uint32_t number = fen_rng_state;
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;
    fen_rng_state = number;
    return number;
}

void set_fen_random_seed(uint32_t seed) {
    fen_rng_state = (seed == 0u) ? 1804289383u : seed;
}

uint64_t get_random_u64_number() {
    uint64_t n1 = static_cast<uint64_t>(get_random_u32_number()) & 0xFFFFULL;
    uint64_t n2 = static_cast<uint64_t>(get_random_u32_number()) & 0xFFFFULL;
    uint64_t n3 = static_cast<uint64_t>(get_random_u32_number()) & 0xFFFFULL;
    uint64_t n4 = static_cast<uint64_t>(get_random_u32_number()) & 0xFFFFULL;
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

std::string get_fen(const Board& board) {

    std::string fen;

    for (int r = 0; r < 8; r++) {
        int emptyCount = 0;
        for (int c = 0; c < 8; c++) {
            int sq = row_col_to_sq(r, c);
            int p = piece_at_sq(board, sq);
            if (p == 0) {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    fen += std::to_string(emptyCount);
                    emptyCount = 0;
                }
                char ch = '.';
                int pt = piece_type(p);
                switch (pt) {
                    case PAWN: ch = 'p'; break;
                    case KNIGHT: ch = 'n'; break;
                    case BISHOP: ch = 'b'; break;
                    case ROOK: ch = 'r'; break;
                    case QUEEN: ch = 'q'; break;
                    case KING: ch = 'k'; break;
                }
                if (piece_color(p) == WHITE) ch = static_cast<char>(ch - 'a' + 'A');
                fen += ch;
            }
        }
        if (emptyCount > 0) {
            fen += std::to_string(emptyCount);
        }
        if (r < 7) fen += '/';
    }

    fen += board.isWhiteTurn ? " w " : " b ";

    fen += (board.whiteCanCastleKingSide ? "K" : "");
    fen += (board.whiteCanCastleQueenSide ? "Q" : "");
    fen += (board.blackCanCastleKingSide ? "k" : "");
    fen += (board.blackCanCastleQueenSide ? "q" : "");
    if (!board.whiteCanCastleKingSide && !board.whiteCanCastleQueenSide && !board.blackCanCastleKingSide && !board.blackCanCastleQueenSide) {
        fen += "-";
    }

    fen += " ";
    if (board.enPassantCol >= 0 && board.enPassantCol < 8) {
        char fileChar = columns[board.enPassantCol];
        char rankChar = board.isWhiteTurn ? '6' : '3';
        fen += fileChar;
        fen += rankChar;
    } else {
        fen += "-";
    }
    // halfmove clock

    fen += " " + std::to_string(board.halfMoveClock);
    // fullmove number 
    fen += " " + std::to_string(board.fullMoveNumber);

    return fen;
}

void generate_fen(Board board, int current_ply){
    if (current_ply == how_many_ply) {
        std::cout << "info string genfens " << get_fen(board) << std::endl;
        return;
    }
    std::vector<Move> moves = get_all_moves(board, board.isWhiteTurn);

    if (moves.size() == 0) {
        return;
    }

    int random_idx = get_random_u64_number() % moves.size();
    Move selectedMove = moves[random_idx];

    board.makeMove(selectedMove);

    generate_fen(board, current_ply + 1);
    
    board.unmakeMove(selectedMove);

}

