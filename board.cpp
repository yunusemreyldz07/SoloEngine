#include "board.h"
#include "bitboard.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

char columns[] = "abcdefgh";

namespace {

// Starting position bitboards
constexpr Bitboard START_W_PAWNS   = 0x000000000000FF00ULL;
constexpr Bitboard START_W_KNIGHTS = 0x0000000000000042ULL;
constexpr Bitboard START_W_BISHOPS = 0x0000000000000024ULL;
constexpr Bitboard START_W_ROOKS   = 0x0000000000000081ULL;
constexpr Bitboard START_W_QUEEN   = 0x0000000000000008ULL;
constexpr Bitboard START_W_KING    = 0x0000000000000010ULL;

constexpr Bitboard START_B_PAWNS   = 0x00FF000000000000ULL;
constexpr Bitboard START_B_KNIGHTS = 0x4200000000000000ULL;
constexpr Bitboard START_B_BISHOPS = 0x2400000000000000ULL;
constexpr Bitboard START_B_ROOKS   = 0x8100000000000000ULL;
constexpr Bitboard START_B_QUEEN   = 0x0800000000000000ULL;
constexpr Bitboard START_B_KING    = 0x1000000000000000ULL;

inline Bitboard bit_at_sq(int sq) {
    return 1ULL << sq;
}

inline void bb_clear(Board& board, int piece, int sq) {
    if (piece == 0) return;
    int idx = piece_type(piece) - 1;
    int c = piece_color(piece);
    Bitboard mask = bit_at_sq(sq);
    board.piece[idx] &= ~mask;
    board.color[c] &= ~mask;
}

inline void bb_set(Board& board, int piece, int sq) {
    if (piece == 0) return;
    int idx = piece_type(piece) - 1;
    int c = piece_color(piece);
    Bitboard mask = bit_at_sq(sq);
    board.piece[idx] |= mask;
    board.color[c] |= mask;
}

inline void set_start_position(Board& board) {
    board.piece[PAWN - 1]   = START_W_PAWNS | START_B_PAWNS;
    board.piece[KNIGHT - 1] = START_W_KNIGHTS | START_B_KNIGHTS;
    board.piece[BISHOP - 1] = START_W_BISHOPS | START_B_BISHOPS;
    board.piece[ROOK - 1]   = START_W_ROOKS | START_B_ROOKS;
    board.piece[QUEEN - 1]  = START_W_QUEEN | START_B_QUEEN;
    board.piece[KING - 1]   = START_W_KING | START_B_KING;

    board.color[WHITE] = START_W_PAWNS | START_W_KNIGHTS | START_W_BISHOPS | START_W_ROOKS | START_W_QUEEN | START_W_KING;
    board.color[BLACK] = START_B_PAWNS | START_B_KNIGHTS | START_B_BISHOPS | START_B_ROOKS | START_B_QUEEN | START_B_KING;

    for (int i = 0; i < 64; i++) board.mailbox[i] = 0;

    // Populate mailbox from bitboards
    for (int sq = 0; sq < 64; ++sq) {
        Bitboard mask = bit_at_sq(sq);
        if (board.color[WHITE] & mask) {
            if (board.piece[PAWN - 1] & mask) board.mailbox[sq] = W_PAWN;
            else if (board.piece[KNIGHT - 1] & mask) board.mailbox[sq] = W_KNIGHT;
            else if (board.piece[BISHOP - 1] & mask) board.mailbox[sq] = W_BISHOP;
            else if (board.piece[ROOK - 1] & mask) board.mailbox[sq] = W_ROOK;
            else if (board.piece[QUEEN - 1] & mask) board.mailbox[sq] = W_QUEEN;
            else if (board.piece[KING - 1] & mask) board.mailbox[sq] = W_KING;
        } else if (board.color[BLACK] & mask) {
            if (board.piece[PAWN - 1] & mask) board.mailbox[sq] = B_PAWN;
            else if (board.piece[KNIGHT - 1] & mask) board.mailbox[sq] = B_KNIGHT;
            else if (board.piece[BISHOP - 1] & mask) board.mailbox[sq] = B_BISHOP;
            else if (board.piece[ROOK - 1] & mask) board.mailbox[sq] = B_ROOK;
            else if (board.piece[QUEEN - 1] & mask) board.mailbox[sq] = B_QUEEN;
            else if (board.piece[KING - 1] & mask) board.mailbox[sq] = B_KING;
        }
    }
}

inline bool is_pawn_attack_possible(const Board& board, bool attackerIsWhite, int epSq) {
    Bitboard pawns = board.piece[PAWN - 1] & board.color[attackerIsWhite ? WHITE : BLACK];
    if (attackerIsWhite) {
        return (pawn_attacks[BLACK][epSq] & pawns) != 0;
    }
    return (pawn_attacks[WHITE][epSq] & pawns) != 0;
}
} // namespace

Board::Board() {
    reset();
}

void Board::reset() {
    set_start_position(*this);

    castling = CASTLE_WK | CASTLE_WQ | CASTLE_BK | CASTLE_BQ; // All castling rights available
    stm = WHITE;
    enPassant = -1;
    halfMoveClock = 0;
    std::vector<UndoState> undoStack;

    hash = position_key(*this);
}

void Board::makeMove(Move& move) {
    int from = move & 0x3F;               // First 6 bit
    int to = (move >> 6) & 0x3F;          // Other 6 bit
    int flags = (move >> 12) & 0xF;       // Last 4 bits
    int promo = get_promotion_type(move); // KNIGHT/BISHOP/ROOK/QUEEN or -1 if no promotion

    const Zobrist& z = zobrist();

    UndoState st;
    st.castling = castling;   
    st.enPassant = enPassant;
    st.halfMoveClock = halfMoveClock;

    if (flags == 5) { // En passant capture 
        st.capturedPiece = (stm == 0) ? (PAWN + 6) : PAWN; 
    } else {
        st.capturedPiece = mailbox[to]; 
    }

    undoStack.push_back(st);

    const int fromSq = move_from(move);
    const int toSq = move_to(move);
    const uint8_t movingPiece = mailbox[fromSq];
    int target_piece = mailbox[toSq];


    // Update 50-move clock: reset on pawn move or capture, otherwise increment
    if (piece_type(movingPiece) == PAWN || target_piece != 0) {
        halfMoveClock = 0;
    } else {
        halfMoveClock++;
    }
    
    // Add to move history for continuation history
    moveHistory.push_back(move);

    // Hash: remove side-to-move, old ep and old castling
    hash ^= z.side;
    if (enPassant != -1) hash ^= z.epFile[enPassant];
    int oldCastling = 0;
    if (castling & CASTLE_WK) oldCastling |= 1;
    if (castling & CASTLE_WQ) oldCastling |= 2;
    if (castling & CASTLE_BK) oldCastling |= 4;
    if (castling & CASTLE_BQ) oldCastling |= 8;
    hash ^= z.castling[oldCastling];

    // Remove moving piece from origin
    hash ^= z.piece[piece_to_zobrist_index(movingPiece)][fromSq];

    bb_clear(*this, movingPiece, fromSq);
    if (target_piece != 0) {
        bb_clear(*this, target_piece, toSq);
        hash ^= z.piece[piece_to_zobrist_index(target_piece)][toSq];
    }
    bb_set(*this, movingPiece, toSq);

    mailbox[fromSq] = 0;
    if (target_piece != 0) mailbox[toSq] = 0;

    if (piece_type(movingPiece) == PAWN && flags == 5) { // En passant capture
        int captureSq = toSq + ((stm == WHITE) ? -8 : 8); // Captured pawn is one rank behind destination from mover's perspective
        int capturedPawn = (piece_color(movingPiece) == WHITE) ? B_PAWN : W_PAWN;
        bb_clear(*this, capturedPawn, captureSq);
        mailbox[captureSq] = 0;
        hash ^= z.piece[piece_to_zobrist_index(capturedPawn)][captureSq];
    }

    if (piece_type(movingPiece) == KING && std::abs(sq_to_col(fromSq) - sq_to_col(toSq)) == 2) {
        if (sq_to_col(toSq) > sq_to_col(fromSq)) {
            int rookFromSq = row_col_to_sq(sq_to_row(toSq), sq_to_col(toSq) + 1);
            int rookToSq = row_col_to_sq(sq_to_row(toSq), sq_to_col(toSq) - 1);
            int rookPiece = make_piece(ROOK, piece_color(movingPiece));
            bb_clear(*this, rookPiece, rookFromSq);
            bb_set(*this, rookPiece, rookToSq);
            mailbox[rookToSq] = mailbox[rookFromSq];
            mailbox[rookFromSq] = 0;
            hash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookFromSq];
            hash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookToSq];
        } else {
            int rookFromSq = row_col_to_sq(sq_to_row(toSq), sq_to_col(toSq) - 2);
            int rookToSq = row_col_to_sq(sq_to_row(toSq), sq_to_col(toSq) + 1);
            int rookPiece = make_piece(ROOK, piece_color(movingPiece));
            bb_clear(*this, rookPiece, rookFromSq);
            bb_set(*this, rookPiece, rookToSq);
            mailbox[rookToSq] = mailbox[rookFromSq];
            mailbox[rookFromSq] = 0;
            hash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookFromSq];
            hash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookToSq];
        }
    }

    int placedPiece = movingPiece;
    if (piece_type(movingPiece) == PAWN) {
        if (promo != -1) {
            placedPiece = make_piece(promo, piece_color(movingPiece));
            bb_clear(*this, movingPiece, toSq);
            bb_set(*this, placedPiece, toSq);
        }
    }

    mailbox[toSq] = placedPiece;
    hash ^= z.piece[piece_to_zobrist_index(placedPiece)][toSq];

    if (piece_type(movingPiece) == PAWN && std::abs(sq_to_row(fromSq) - sq_to_row(toSq)) == 2) {
        const bool opponentWhite = (stm == BLACK);
        const int epRow = opponentWhite ? 2 : 5;
        const int epSq = row_col_to_sq(epRow, sq_to_col(toSq));
        if (is_pawn_attack_possible(*this, opponentWhite, epSq)) {
            enPassant = sq_to_col(toSq);
        } else {
            enPassant = -1;
        }
    } else {
        enPassant = -1;
    }

    if (movingPiece == W_ROOK && sq_to_row(fromSq) == 7 && sq_to_col(fromSq) == 7){
        castling &= ~CASTLE_WK;
    }
    else if (movingPiece == W_ROOK && sq_to_row(fromSq) == 7 && sq_to_col(fromSq) == 0){
        castling &= ~CASTLE_WQ;
    }
    else if (movingPiece == B_ROOK && sq_to_row(fromSq) == 0 && sq_to_col(fromSq) == 7){
        castling &= ~CASTLE_BK;
    }
    else if (movingPiece == B_ROOK && sq_to_row(fromSq) == 0 && sq_to_col(fromSq) == 0){
        castling &= ~CASTLE_BQ;
    }

    if (movingPiece == W_KING) {
        castling &= ~(CASTLE_WK | CASTLE_WQ);
    }
    if (movingPiece == B_KING) {
        castling &= ~(CASTLE_BK | CASTLE_BQ);
    }

    if (st.capturedPiece == W_ROOK && sq_to_row(toSq) == 7 && sq_to_col(toSq) == 7) {
        castling &= ~CASTLE_WK;
    }
    if (st.capturedPiece == W_ROOK && sq_to_row(toSq) == 7 && sq_to_col(toSq) == 0) {
        castling &= ~CASTLE_WQ;
    }
    if (st.capturedPiece == B_ROOK && sq_to_row(toSq) == 0 && sq_to_col(toSq) == 7) {
        castling &= ~CASTLE_BK;
    }
    if (st.capturedPiece == B_ROOK && sq_to_row(toSq) == 0 && sq_to_col(toSq) == 0) {
        castling &= ~CASTLE_BQ;
    }

    hash ^= z.castling[castling];

    if (enPassant != -1) {
        hash ^= z.epFile[enPassant];
    }

    stm = other_color(stm);
}

void Board::unmakeMove(Move& move) {
    const Zobrist& z = zobrist();
    
    // Remove from move history
    if (!moveHistory.empty()) {
        moveHistory.pop_back();
    }

    UndoState st = this->undoStack.back();
    this->undoStack.pop_back();
    stm = other_color(stm);

    // Hash out side, current ep, current castling (state after move, before undo)
    hash ^= z.side;
    if (enPassant != -1) hash ^= z.epFile[enPassant];
    hash ^= z.castling[castling];

    const int fromSq = move_from(move);
    const int toSq = move_to(move);

    int pieceOnTo = mailbox[toSq];
    int pieceBase = pieceOnTo;
    if (move_flags(move) >= 8) { // Promotion undo
        pieceBase = make_piece(PAWN, piece_color(pieceOnTo));
    }

    // Remove moved piece from destination
    bb_clear(*this, pieceOnTo, toSq);
    mailbox[toSq] = 0;
    hash ^= z.piece[piece_to_zobrist_index(pieceOnTo)][toSq];

    // Undo castling ROOK move if needed
    if (move_flags(move) == 3 || move_flags(move) == 2) { // Castling move
        if (sq_to_col(toSq) > sq_to_col(fromSq)) {
            int rookFromSq = row_col_to_sq(sq_to_row(toSq), sq_to_col(toSq) - 1);
            int rookToSq = row_col_to_sq(sq_to_row(toSq), sq_to_col(toSq) + 1);
            int rookPiece = stm == WHITE ? W_ROOK : B_ROOK;
            bb_clear(*this, rookPiece, rookFromSq);
            bb_set(*this, rookPiece, rookToSq);
            mailbox[rookFromSq] = 0;
            mailbox[rookToSq] = rookPiece;
            hash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookFromSq];
            hash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookToSq];
        } else {
            int rookFromSq = row_col_to_sq(sq_to_row(toSq), sq_to_col(toSq) + 1);
            int rookToSq = row_col_to_sq(sq_to_row(toSq), sq_to_col(toSq) - 2);
            int rookPiece = stm == WHITE ? W_ROOK : B_ROOK;
            bb_clear(*this, rookPiece, rookFromSq);
            bb_set(*this, rookPiece, rookToSq);
            mailbox[rookFromSq] = 0;
            mailbox[rookToSq] = rookPiece;
            hash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookFromSq];
            hash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookToSq];
        }
    }

    // Restore moving piece to origin
    bb_set(*this, pieceBase, fromSq);
    mailbox[fromSq] = pieceBase;
    hash ^= z.piece[piece_to_zobrist_index(pieceBase)][fromSq];

    // Restore captured piece
    if (move_flags(move) == 5) { // En passant capture
        int capturedPawn = stm == WHITE ? B_PAWN : W_PAWN;
        int captureSq = row_col_to_sq(sq_to_row(fromSq), sq_to_col(toSq));
        bb_set(*this, capturedPawn, captureSq);
        mailbox[captureSq] = capturedPawn;
        hash ^= z.piece[piece_to_zobrist_index(capturedPawn)][captureSq];
    } else if (st.capturedPiece != 0) {
        bb_set(*this, st.capturedPiece, toSq);
        mailbox[toSq] = st.capturedPiece;
        hash ^= z.piece[piece_to_zobrist_index(st.capturedPiece)][toSq];
    }

    castling = st.castling;
    enPassant = st.enPassant;
    halfMoveClock = st.halfMoveClock;
    hash ^= z.castling[castling];

    if (enPassant != -1) hash ^= z.epFile[enPassant];
}

void Board::loadFEN(const std::string& fen) {
    for (int i = 0; i < 6; i++) piece[i] = 0ULL;
    color[WHITE] = 0ULL;
    color[BLACK] = 0ULL;
    for (int i = 0; i < 64; i++) mailbox[i] = 0;
    castling = 0;

    std::istringstream ss(fen);
    std::string position, turn, castling, enPassant;
    ss >> position >> turn >> castling >> enPassant;

    int row = 0, col = 0;
    for (char c : position) {
        if (c == '/') {
            row++;
            col = 0;
        } else if (c >= '1' && c <= '8') {
            col += (c - '0');
        } else {
            int pt = 0;
            switch (std::tolower(static_cast<unsigned char>(c))) {
                case 'p': pt = PAWN; break;
                case 'n': pt = KNIGHT; break;
                case 'b': pt = BISHOP; break;
                case 'r': pt = ROOK; break;
                case 'q': pt = QUEEN; break;
                case 'k': pt = KING; break;
            }
            bool isWhite = std::isupper(static_cast<unsigned char>(c));
            int pieceVal = make_piece(pt, isWhite ? WHITE : BLACK);
            int sq = row_col_to_sq(row, col);
            bb_set(*this, pieceVal, sq);
            mailbox[sq] = pieceVal;
            col++;
        }
    }

    stm = (turn == "w") ? WHITE : BLACK;
    this->castling = (castling.find('K') != std::string::npos) ? (this->castling | CASTLE_WK) : this->castling;
    this->castling = (castling.find('Q') != std::string::npos) ? (this->castling | CASTLE_WQ) : this->castling;
    this->castling = (castling.find('k') != std::string::npos) ? (this->castling | CASTLE_BK) : this->castling;
    this->castling = (castling.find('q') != std::string::npos) ? (this->castling | CASTLE_BQ) : this->castling;

    if (enPassant != "-") {
        this->enPassant = enPassant[0] - 'a';
        int epRow = stm == 0 ? 2 : 5;
        int epSq = row_col_to_sq(epRow, this->enPassant);
        if (!is_pawn_attack_possible(*this, stm == 0 ? false : true, epSq)) {
            this->enPassant = -1;
        }
    } else {
        this->enPassant = -1;
    }

    // Parse halfmove clock (50-move rule) if present
    int halfMoveClockFromFen = 0;
    int fullMoveNumber = 1;
    if (ss >> halfMoveClockFromFen >> fullMoveNumber) {
        halfMoveClock = halfMoveClockFromFen;
    } else {
        halfMoveClock = 0;
    }

    hash = position_key(*this);
}

void printBoard(const Board& board) {
    std::cout << "  +-----------------+" << std::endl;
    for (int r = 0; r < 8; r++) {
        std::cout << 8 - r << " | ";
        for (int c = 0; c < 8; c++) {
            int sq = row_col_to_sq(r, c);
            int p = piece_at_sq(board, sq);
            char cStr = '.';
            if (p != 0) {
                char ch = ' ';
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
                cStr = ch;
            }
            std::cout << cStr << " ";
        }
        std::cout << "|" << std::endl;
    }
    std::cout << "  +-----------------+" << std::endl;
    std::cout << "    a b c d e f g h" << std::endl;
}

Move uci_to_move(const std::string& uci, const Board& board) {
    // (a1=0, h8=63)
    int fromCol = uci[0] - 'a';
    int fromSq = (uci[1] - '1') * 8 + fromCol;
    int fromRow = sq_to_row(fromSq);

    int toCol = uci[2] - 'a';
    int toSq = (uci[3] - '1') * 8 + toCol;
    int toRow = sq_to_row(toSq);

    int flags = 0;
    int movingPiece = board.mailbox[fromSq];
    int targetPiece = board.mailbox[toSq];

    if (uci.length() == 5) {
        // Promotion
        char promoChar = uci[4];
        int promoBase = 0;
        switch (promoChar) {
            case 'n': promoBase = 8; break;  // FLAG_PROMO_KNIGHT
            case 'b': promoBase = 9; break;  // FLAG_PROMO_BISHOP
            case 'r': promoBase = 10; break; // FLAG_PROMO_ROOK
            case 'q': promoBase = 11; break; // FLAG_PROMO_QUEEN
        }
        flags = (targetPiece != 0) ? promoBase + 4 : promoBase;
    } else if (piece_type(movingPiece) == KING && std::abs(fromCol - toCol) == 2) {
        flags = (toCol > fromCol) ? FLAG_CASTLE_KING : FLAG_CASTLE_QUEEN;
    } else if (piece_type(movingPiece) == PAWN) {
        if (std::abs(fromRow - toRow) == 2) {
            flags = FLAG_DOUBLE_PAWN;
        } else if (fromCol != toCol && targetPiece == 0) {
            flags = FLAG_EN_PASSANT;
        } else if (targetPiece != 0) {
            flags = FLAG_CAPTURE;
        }
    } else if (targetPiece != 0) {
        flags = FLAG_CAPTURE;
    }

    return (flags << 12) | (toSq << 6) | fromSq;
}

Zobrist::Zobrist() {
    uint64_t seed = 0xC0FFEE1234ABCDEFULL;
    for (int p = 0; p < 12; p++) {
        for (int sq = 0; sq < 64; sq++) {
            piece[p][sq] = splitmix64(seed);
        }
    }
    for (int i = 0; i < 16; i++) castling[i] = splitmix64(seed);
    for (int i = 0; i < 9; i++) epFile[i] = splitmix64(seed);
    side = splitmix64(seed);
}

uint64_t Zobrist::splitmix64(uint64_t& x) {
    uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

const Zobrist& zobrist() {
    static Zobrist z;
    return z;
}

int piece_to_zobrist_index(int piece) {
    // New encoding: 1-6 = white, 7-12 = black
    // Returns 0-5 for white pieces, 6-11 for black pieces
    if (piece < 1 || piece > 12) return -1;
    return piece - 1;  // piece 1 -> index 0, piece 12 -> index 11
}

uint64_t position_key(const Board& board) {
    const Zobrist& z = zobrist();
    uint64_t h = 0;

    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 6; p++) {
            Bitboard bb = board.piece[p] & board.color[c];
            int base = c * 6 + p;
            while (bb) {
                int sq = lsb(bb);
                bb &= bb - 1;
                h ^= z.piece[base][sq];
            }
        }
    }

    h ^= z.castling[board.castling];

    int ep = (board.enPassant >= 0 && board.enPassant < 8) ? board.enPassant : 8;
    h ^= z.epFile[ep];

    if (board.stm == WHITE) h ^= z.side;
    return h;
}

bool is_threefold_repetition(const std::vector<uint64_t>& positionHistory) {
    if (positionHistory.empty()) return false;
    uint64_t current = positionHistory.back();
    int count = 0;
    for (int i = static_cast<int>(positionHistory.size()) - 1; i >= 0; i--) {
        if (positionHistory[i] == current) {
            count++;
            if (count >= 3) return true;
        }
    }
    return false;
}

TranspositionTable globalTT;

uint64_t TranspositionTable::packData(int score, int depth, TTFlag flag, uint16_t packedMove) {
    uint64_t data = static_cast<uint32_t>(score);
    data |= (static_cast<uint64_t>(depth) & 0xFFULL) << 32;
    data |= (static_cast<uint64_t>(static_cast<int>(flag)) & 0x3ULL) << 40;
    data |= (static_cast<uint64_t>(packedMove) & 0xFFFFULL) << 42;
    return data;
}

void TranspositionTable::unpackData(uint64_t data, int& score, int& depth, TTFlag& flag, uint16_t& packedMove) {
    score = static_cast<int32_t>(static_cast<uint32_t>(data & 0xFFFFFFFFULL));
    depth = static_cast<int>((data >> 32) & 0xFFULL);
    flag = static_cast<TTFlag>((data >> 40) & 0x3ULL);
    packedMove = static_cast<uint16_t>((data >> 42) & 0xFFFFULL);
}

void TranspositionTable::resize(int mbSize) {
    delete[] table;
    table = nullptr;
    size = 0;

    if (mbSize <= 0) return;
    const size_t bytes = static_cast<size_t>(mbSize) * 1024ULL * 1024ULL;
    size_t count = bytes / sizeof(TTAtomicEntry);
    if (count == 0) count = 1;

    table = new TTAtomicEntry[count];
    size = count;
    clear();
}

void TranspositionTable::clear() {
    if (!table || size == 0) return;
    for (size_t i = 0; i < size; i++) {
        table[i].data.store(0, std::memory_order_relaxed);
        table[i].key.store(0, std::memory_order_relaxed);
    }
}

void TranspositionTable::store(uint64_t hash, int score, int depth, TTFlag flag, const Move& bestMove) {
    if (!table || size == 0) return;

    const size_t index = static_cast<size_t>(hash % size);
    TTAtomicEntry& e = table[index];

    const uint64_t existingKey = e.key.load(std::memory_order_relaxed);
    const uint64_t existingData = e.data.load(std::memory_order_relaxed);
    int existingScore = 0;
    int existingDepth = 0;
    TTFlag existingFlag = TTFlag::EXACT;
    uint16_t existingMove = 0;
    unpackData(existingData, existingScore, existingDepth, existingFlag, existingMove);

    if (existingKey == 0 || existingKey != hash || depth >= existingDepth) {
        const uint64_t newData = packData(score, depth, flag, bestMove);
        e.data.store(newData, std::memory_order_relaxed);
        e.key.store(hash, std::memory_order_release);
    }
}

bool TranspositionTable::probe(uint64_t key, int& outScore, int& outDepth, TTFlag& outFlag, Move& outMove) const {
    if (!table || size == 0) return false;

    const size_t index = static_cast<size_t>(key % size);
    const TTAtomicEntry& e = table[index];

    const uint64_t foundKey = e.key.load(std::memory_order_acquire);
    if (foundKey != key) return false;

    const uint64_t data = e.data.load(std::memory_order_relaxed);
    unpackData(data, outScore, outDepth, outFlag, outMove);
    return true;
}

// SEE piece values; keep close to MVV/LVA ordering, not evaluation values
const int see_piece_values[] = {0, 100, 320, 330, 500, 900, 20000};

static Bitboard attackers_to_sq(Bitboard pieces[2][6], int sq, int side, Bitboard occ) {
    Bitboard attackers = 0ULL;

    // Pawns: to find pawns of 'side' that attack 'sq', use pawn_attacks of the opposite side.
    attackers |= ((side == WHITE) ? pawn_attacks[BLACK][sq] : pawn_attacks[WHITE][sq]) & pieces[side][PAWN - 1];
    attackers |= knight_attacks[sq] & pieces[side][KNIGHT - 1];
    attackers |= king_attacks[sq] & pieces[side][KING - 1];
    attackers |= bishop_attacks_on_the_fly(sq, occ) & (pieces[side][BISHOP - 1] | pieces[side][QUEEN - 1]);
    attackers |= rook_attacks_on_the_fly(sq, occ) & (pieces[side][ROOK - 1] | pieces[side][QUEEN - 1]);
    return attackers;
}

inline int moveEstimatedValue(const Board& board, const Move& move){
    int flags = move_flags(move);

    int value = 0;
    if (is_quiet(move)) return 0;
    if (flags >= 8) { // Promotion
        value += see_piece_values[get_promotion_type(move)] - see_piece_values[PAWN]; // A pawn goes, a promoted piece comes
    }
    value += see_piece_values[move_flags(move) == FLAG_EN_PASSANT ? PAWN : piece_type(piece_at_sq(board, move_to(move)))];
    return value;
}

// All attackers to a square (both sides)
static Bitboard all_attackers_to_sq(const Board& board, int sq, Bitboard occ) {
    Bitboard attackers = 0ULL;
    
    // Pawns
    attackers |= pawn_attacks[BLACK][sq] & board.piece[PAWN - 1] & board.color[WHITE];
    attackers |= pawn_attacks[WHITE][sq] & board.piece[PAWN - 1] & board.color[BLACK];
    // Knights
    attackers |= knight_attacks[sq] & board.piece[KNIGHT - 1];
    // Kings
    attackers |= king_attacks[sq] & board.piece[KING - 1];
    // Bishops and Queens (diagonal)
    attackers |= bishop_attacks_on_the_fly(sq, occ) & (board.piece[BISHOP - 1] | board.piece[QUEEN - 1]);
    // Rooks and Queens (orthogonal)
    attackers |= rook_attacks_on_the_fly(sq, occ) & (board.piece[ROOK - 1] | board.piece[QUEEN - 1]);
    
    return attackers;
}

int staticExchangeEvaluation(const Board& board, const Move& move, int threshold) {
    int flags = move_flags(move);
    // Ethereal-style threshold-based SEE
    int from = move_from(move);
    int to = move_to(move);
    
    int balance, nextVictim;
    Bitboard bishops, rooks, occupied, attackers, myAttackers;

    // Determine the piece that will be on 'to' after the move (for recapture value)
    // If promotion, it's the promoted piece; otherwise, it's the moving piece
    nextVictim = (get_promotion_type(move) != -1) 
               ? get_promotion_type(move)
               : piece_type(board.mailbox[from]);

    // Balance is the value of the move minus threshold
    balance = moveEstimatedValue(board, move) - threshold;

    // Best case still fails to beat the threshold
    if (balance < 0) return 0;

    // Worst case is losing the moved piece (or promoted piece)
    balance -= see_piece_values[nextVictim];

    // If the balance is positive even if losing the moved piece,
    // the exchange is guaranteed to beat the threshold.
    if (balance >= 0) return 1;

    // Grab sliders for updating revealed attackers
    bishops = board.piece[BISHOP - 1] | board.piece[QUEEN - 1];
    rooks = board.piece[ROOK - 1] | board.piece[QUEEN - 1];

    // Let occupied suppose that the move was actually made
    occupied = board.color[WHITE] | board.color[BLACK];
    occupied = (occupied ^ (1ULL << from)) | (1ULL << to);

    // Handle en passant: remove the captured pawn
    if (flags == 5) {
        int capRow = sq_to_row(from);
        int capCol = sq_to_col(to);
        int capSq = row_col_to_sq(capRow, capCol);
        occupied ^= (1ULL << capSq);
    }

    // Get all pieces which attack the target square
    attackers = all_attackers_to_sq(board, to, occupied) & occupied;

    // Now opponent's turn to recapture
    int colour = board.stm ^ 1; // Opponent's turn

    while (true) {
        // If we have no more attackers left, we lose
        myAttackers = attackers & board.color[colour];
        if (myAttackers == 0ULL) break;

        // Find our weakest piece to attack with
        for (nextVictim = PAWN; nextVictim <= QUEEN; nextVictim++) {
            if (myAttackers & board.piece[nextVictim - 1])
                break;
        }

        // If only king can attack, check if it's safe
        if (nextVictim > QUEEN) {
            nextVictim = KING;
        }

        // Remove this attacker from occupied
        Bitboard attackerBB = myAttackers & board.piece[nextVictim - 1];
        if (attackerBB == 0ULL) break;
        occupied ^= (1ULL << lsb(attackerBB));

        // A diagonal move may reveal bishop or queen attackers
        if (nextVictim == PAWN || nextVictim == BISHOP || nextVictim == QUEEN)
            attackers |= bishop_attacks_on_the_fly(to, occupied) & bishops;

        // A vertical or horizontal move may reveal rook or queen attackers
        if (nextVictim == ROOK || nextVictim == QUEEN)
            attackers |= rook_attacks_on_the_fly(to, occupied) & rooks;

        // Make sure we did not add any already used attacks
        attackers &= occupied;

        // Swap the turn
        colour ^= 1;

        // Negamax the balance and add the value of the next victim
        balance = -balance - 1 - see_piece_values[nextVictim];

        // If the balance is non-negative after giving away our piece then we win
        if (balance >= 0) {
            // Speed-up for move legality: if our last attacking piece is a king,
            // and opponent still has attackers, then we've lost (move would be illegal)
            if (nextVictim == KING && (attackers & board.color[colour]))
                colour ^= 1;

            break;
        }
    }

    // Side to move after the loop loses
    return (board.stm == WHITE ? WHITE : BLACK) != colour;
}

// Insufficient material detection
bool is_insufficient_material(const Board& board) {
    // If any pawns exist, there's always mating potential through promotion
    if (board.piece[PAWN - 1] != 0) return false;
    
    // If any rooks or queens exist, checkmate is possible
    if (board.piece[ROOK - 1] != 0) return false;
    if (board.piece[QUEEN - 1] != 0) return false;
    
    // Count minor pieces
    int whiteKnights = popcount(board.piece[KNIGHT - 1] & board.color[WHITE]);
    int whiteBishops = popcount(board.piece[BISHOP - 1] & board.color[WHITE]);
    int blackKnights = popcount(board.piece[KNIGHT - 1] & board.color[BLACK]);
    int blackBishops = popcount(board.piece[BISHOP - 1] & board.color[BLACK]);
    
    int whiteMinors = whiteKnights + whiteBishops;
    int blackMinors = blackKnights + blackBishops;
    
    // K vs K
    if (whiteMinors == 0 && blackMinors == 0) return true;
    
    // K+minor vs K (one side has exactly one minor, other has none)
    if (whiteMinors == 1 && blackMinors == 0) return true;
    if (blackMinors == 1 && whiteMinors == 0) return true;

    
    return false;
}
