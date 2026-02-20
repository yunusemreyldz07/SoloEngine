#include "board.h"
#include "bitboard.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>

char columns[] = "abcdefgh";

namespace {

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
    const int idx = piece_type(piece) - 1;
    const int c = piece_color(piece);
    const Bitboard mask = bit_at_sq(sq);
    board.piece[idx] &= ~mask;
    board.color[c] &= ~mask;
}

inline void bb_set(Board& board, int piece, int sq) {
    if (piece == 0) return;
    const int idx = piece_type(piece) - 1;
    const int c = piece_color(piece);
    const Bitboard mask = bit_at_sq(sq);
    board.piece[idx] |= mask;
    board.color[c] |= mask;
}

inline void clear_board(Board& board) {
    for (int i = 0; i < 6; ++i) board.piece[i] = 0ULL;
    board.color[WHITE] = 0ULL;
    board.color[BLACK] = 0ULL;
    for (int i = 0; i < 64; ++i) board.mailbox[i] = 0;
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

    for (int i = 0; i < 64; ++i) board.mailbox[i] = 0;

    for (int sq = 0; sq < 64; ++sq) {
        const Bitboard mask = bit_at_sq(sq);
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

inline bool is_pawn_attack_possible(const Board& board, int attackerColor, int epSq) {
    const Bitboard pawns = board.piece[PAWN - 1] & board.color[attackerColor];
    if (attackerColor == WHITE) {
        return (pawn_attacks[BLACK][epSq] & pawns) != 0;
    }
    return (pawn_attacks[WHITE][epSq] & pawns) != 0;
}

inline bool is_rook_start_square(int piece, int sq) {
    return (piece == W_ROOK && (sq == 0 || sq == 7)) || (piece == B_ROOK && (sq == 56 || sq == 63));
}

inline void remove_castling_rights_for_rook(Board& board, int piece, int sq) {
    if (!is_rook_start_square(piece, sq)) return;
    if (piece == W_ROOK) {
        if (sq == 7) board.castling &= ~CASTLE_WHITE_K;
        if (sq == 0) board.castling &= ~CASTLE_WHITE_Q;
    } else {
        if (sq == 63) board.castling &= ~CASTLE_BLACK_K;
        if (sq == 56) board.castling &= ~CASTLE_BLACK_Q;
    }
}

} // namespace

Board::Board() {
    reset();
}

void Board::reset() {
    set_start_position(*this);
    castling = CASTLE_WHITE_K | CASTLE_WHITE_Q | CASTLE_BLACK_K | CASTLE_BLACK_Q;
    enPassant = -1;
    stm = WHITE;
    halfMoveClock = 0;
    undoStack.clear();
    hash = position_key(*this);
}

void Board::loadFEN(const std::string& fen) {
    clear_board(*this);

    std::istringstream ss(fen);
    std::string boardPart;
    std::string turnPart;
    std::string castlingPart;
    std::string epPart;
    int halfmove = 0;
    int fullmove = 1;

    ss >> boardPart >> turnPart >> castlingPart >> epPart;
    if (!(ss >> halfmove)) halfmove = 0;
    if (!(ss >> fullmove)) fullmove = 1;

    int row = 0;
    int col = 0;
    for (char c : boardPart) {
        if (c == '/') {
            ++row;
            col = 0;
            continue;
        }
        if (c >= '1' && c <= '8') {
            col += (c - '0');
            continue;
        }

        int pt = 0;
        switch (static_cast<char>(std::tolower(static_cast<unsigned char>(c)))) {
            case 'p': pt = PAWN; break;
            case 'n': pt = KNIGHT; break;
            case 'b': pt = BISHOP; break;
            case 'r': pt = ROOK; break;
            case 'q': pt = QUEEN; break;
            case 'k': pt = KING; break;
            default: break;
        }
        if (pt == 0) continue;

        const int colorOfPiece = std::isupper(static_cast<unsigned char>(c)) ? WHITE : BLACK;
        const int p = make_piece(pt, colorOfPiece);
        const int sq = row_col_to_sq(row, col);
        bb_set(*this, p, sq);
        mailbox[sq] = static_cast<int8_t>(p);
        ++col;
    }

    stm = (turnPart == "b") ? BLACK : WHITE;

    castling = 0;
    if (castlingPart.find('K') != std::string::npos) castling |= CASTLE_WHITE_K;
    if (castlingPart.find('Q') != std::string::npos) castling |= CASTLE_WHITE_Q;
    if (castlingPart.find('k') != std::string::npos) castling |= CASTLE_BLACK_K;
    if (castlingPart.find('q') != std::string::npos) castling |= CASTLE_BLACK_Q;

    enPassant = -1;
    if (epPart.size() == 2 && epPart[0] >= 'a' && epPart[0] <= 'h' && epPart[1] >= '1' && epPart[1] <= '8') {
        const int epCol = epPart[0] - 'a';
        const int epRow = 8 - (epPart[1] - '0');
        const int epSq = row_col_to_sq(epRow, epCol);
        if (is_pawn_attack_possible(*this, stm, epSq)) {
            enPassant = static_cast<int8_t>(epSq);
        }
    }

    halfMoveClock = static_cast<int8_t>(std::clamp(halfmove, 0, 127));
    undoStack.clear();
    hash = position_key(*this);
}

void Board::makeMove(Move move) {
    const int from = from_sq(move);
    const int to = to_sq(move);
    const int movingPiece = mailbox[from];
    if (movingPiece == EMPTY) return;

    const int movingColor = piece_color(movingPiece);
    const bool promotion = is_promotion(*this, move);
    const int promoPieceType = promotion_piece(*this, move);
    const bool castlingMove = is_castling(move) || (piece_type(movingPiece) == KING && std::abs(from - to) == 2);

    int captureSq = to;
    bool enPassantMove = false;
    if (piece_type(movingPiece) == PAWN && sq_to_col(from) != sq_to_col(to) && mailbox[to] == EMPTY) {
        enPassantMove = true;
        captureSq = (movingColor == WHITE) ? (to - 8) : (to + 8);
    } else if (is_en_passant(move)) {
        enPassantMove = true;
        captureSq = (movingColor == WHITE) ? (to - 8) : (to + 8);
    }
    const int capturedPiece = (captureSq >= 0 && captureSq < 64) ? mailbox[captureSq] : EMPTY;

    UndoState st;
    st.move = move;
    st.movedPiece = static_cast<int8_t>(movingPiece);
    st.capturedPiece = static_cast<int8_t>(capturedPiece);
    st.captureSquare = static_cast<int8_t>(captureSq);
    st.wasEnPassant = enPassantMove;
    st.wasCastling = castlingMove;
    st.prevCastling = castling;
    st.prevEnPassant = enPassant;
    st.prevHalfMoveClock = halfMoveClock;
    st.prevHash = hash;

    halfMoveClock = (piece_type(movingPiece) == PAWN || capturedPiece != EMPTY) ? 0 : static_cast<int8_t>(halfMoveClock + 1);
    enPassant = -1;

    bb_clear(*this, movingPiece, from);
    mailbox[from] = EMPTY;

    if (capturedPiece != EMPTY) {
        bb_clear(*this, capturedPiece, captureSq);
        mailbox[captureSq] = EMPTY;
    }

    int placedPiece = movingPiece;
    if (promotion && promoPieceType != 0) {
        placedPiece = make_piece(promoPieceType, movingColor);
    }

    bb_set(*this, placedPiece, to);
    mailbox[to] = static_cast<int8_t>(placedPiece);

    if (castlingMove) {
        int rookFrom = -1;
        int rookTo = -1;
        if (to > from) {
            rookFrom = from + 3;
            rookTo = from + 1;
        } else {
            rookFrom = from - 4;
            rookTo = from - 1;
        }
        st.rookFrom = static_cast<int8_t>(rookFrom);
        st.rookTo = static_cast<int8_t>(rookTo);

        const int rookPiece = make_piece(ROOK, movingColor);
        bb_clear(*this, rookPiece, rookFrom);
        bb_set(*this, rookPiece, rookTo);
        mailbox[rookFrom] = EMPTY;
        mailbox[rookTo] = static_cast<int8_t>(rookPiece);
    }

    if (piece_type(movingPiece) == KING) {
        if (movingColor == WHITE) castling &= static_cast<uint8_t>(~(CASTLE_WHITE_K | CASTLE_WHITE_Q));
        else castling &= static_cast<uint8_t>(~(CASTLE_BLACK_K | CASTLE_BLACK_Q));
    }

    if (piece_type(movingPiece) == ROOK) {
        remove_castling_rights_for_rook(*this, movingPiece, from);
    }

    if (capturedPiece != EMPTY && piece_type(capturedPiece) == ROOK) {
        remove_castling_rights_for_rook(*this, capturedPiece, captureSq);
    }

    if (piece_type(movingPiece) == PAWN && std::abs(from - to) == 16) {
        enPassant = static_cast<int8_t>((from + to) / 2);
    }

    undoStack.push_back(st);
    stm ^= 1;
    hash = position_key(*this);
}

void Board::unmakeMove(Move /*move*/) {
    if (undoStack.empty()) return;
    const UndoState st = undoStack.back();
    undoStack.pop_back();

    stm ^= 1;

    const int from = from_sq(st.move);
    const int to = to_sq(st.move);
    const int movedPiece = st.movedPiece;
    const int capturedPiece = st.capturedPiece;

    const int pieceOnTo = mailbox[to];
    if (pieceOnTo != EMPTY) {
        bb_clear(*this, pieceOnTo, to);
    }
    mailbox[to] = EMPTY;

    bb_set(*this, movedPiece, from);
    mailbox[from] = static_cast<int8_t>(movedPiece);

    if (st.wasCastling && st.rookFrom >= 0 && st.rookTo >= 0) {
        const int rookPiece = make_piece(ROOK, stm);
        bb_clear(*this, rookPiece, st.rookTo);
        bb_set(*this, rookPiece, st.rookFrom);
        mailbox[st.rookTo] = EMPTY;
        mailbox[st.rookFrom] = static_cast<int8_t>(rookPiece);
    }

    if (capturedPiece != EMPTY && st.captureSquare >= 0) {
        bb_set(*this, capturedPiece, st.captureSquare);
        mailbox[st.captureSquare] = static_cast<int8_t>(capturedPiece);
    }

    castling = st.prevCastling;
    enPassant = st.prevEnPassant;
    halfMoveClock = st.prevHalfMoveClock;
    hash = st.prevHash;
}

void printBoard(const Board& board) {
    std::cout << "  +-----------------+" << std::endl;
    for (int r = 0; r < 8; r++) {
        std::cout << 8 - r << " | ";
        for (int c = 0; c < 8; c++) {
            const int sq = row_col_to_sq(r, c);
            const int p = piece_at_sq(board, sq);
            char cStr = '.';
            if (p != 0) {
                char ch = ' ';
                const int pt = piece_type(p);
                switch (pt) {
                    case PAWN: ch = 'p'; break;
                    case KNIGHT: ch = 'n'; break;
                    case BISHOP: ch = 'b'; break;
                    case ROOK: ch = 'r'; break;
                    case QUEEN: ch = 'q'; break;
                    case KING: ch = 'k'; break;
                    default: break;
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

Move uci_to_move(const std::string& uci) {
    if (uci.size() < 4) return 0;
    const int fromCol = uci[0] - 'a';
    const int fromRow = 8 - (uci[1] - '0');
    const int toCol = uci[2] - 'a';
    const int toRow = 8 - (uci[3] - '0');
    const int from = row_col_to_sq(fromRow, fromCol);
    const int to = row_col_to_sq(toRow, toCol);

    int promo = 0;
    if (uci.size() >= 5) {
        switch (static_cast<char>(std::tolower(static_cast<unsigned char>(uci[4])))) {
            case 'q': promo = QUEEN; break;
            case 'r': promo = ROOK; break;
            case 'b': promo = BISHOP; break;
            case 'n': promo = KNIGHT; break;
            default: promo = 0; break;
        }
    }
    return make_move(from, to, promo, MOVE_FLAG_NORMAL);
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
    if (piece < 1 || piece > 12) return -1;
    return piece - 1;
}

uint64_t position_key(const Board& board) {
    const Zobrist& z = zobrist();
    uint64_t h = 0;

    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 6; p++) {
            Bitboard bb = board.piece[p] & board.color[c];
            const int base = c * 6 + p;
            while (bb) {
                const int sq = lsb(bb);
                bb &= bb - 1;
                h ^= z.piece[base][sq];
            }
        }
    }

    h ^= z.castling[board.castling & 0xF];
    const int ep = (board.enPassant >= 0 && board.enPassant < 64) ? (board.enPassant % 8) : 8;
    h ^= z.epFile[ep];
    if (board.stm == WHITE) h ^= z.side;
    return h;
}

bool is_threefold_repetition(const std::vector<uint64_t>& positionHistory) {
    if (positionHistory.empty()) return false;
    const uint64_t current = positionHistory.back();
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

uint16_t TranspositionTable::packMove(const Move& m) {
    return m;
}

Move TranspositionTable::unpackMove(uint16_t packed) {
    return packed;
}

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
        const uint16_t pm = packMove(bestMove);
        const uint64_t newData = packData(score, depth, flag, pm);
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
    uint16_t packedMove = 0;
    unpackData(data, outScore, outDepth, outFlag, packedMove);
    outMove = unpackMove(packedMove);
    return true;
}

const int see_piece_values[] = {0, 100, 320, 330, 500, 900, 20000};

static Bitboard all_attackers_to_sq(const Board& board, int sq, Bitboard occ) {
    Bitboard attackers = 0ULL;
    attackers |= pawn_attacks[BLACK][sq] & board.piece[PAWN - 1] & board.color[WHITE];
    attackers |= pawn_attacks[WHITE][sq] & board.piece[PAWN - 1] & board.color[BLACK];
    attackers |= knight_attacks[sq] & board.piece[KNIGHT - 1];
    attackers |= king_attacks[sq] & board.piece[KING - 1];
    attackers |= bishop_attacks_on_the_fly(sq, occ) & (board.piece[BISHOP - 1] | board.piece[QUEEN - 1]);
    attackers |= rook_attacks_on_the_fly(sq, occ) & (board.piece[ROOK - 1] | board.piece[QUEEN - 1]);
    return attackers;
}

inline int captured_piece_for_move(const Board& board, const Move move) {
    const int from = from_sq(move);
    const int to = to_sq(move);
    const int movingPiece = board.mailbox[from];
    if (movingPiece == EMPTY) return EMPTY;
    if (is_en_passant(move) || (piece_type(movingPiece) == PAWN && sq_to_col(from) != sq_to_col(to) && board.mailbox[to] == EMPTY)) {
        return make_piece(PAWN, other_color(piece_color(movingPiece)));
    }
    return board.mailbox[to];
}

inline int moveEstimatedValue(const Board& board, const Move move) {
    int value = 0;
    if (is_promotion(board, move)) {
        value += see_piece_values[promotion_piece(board, move)] - see_piece_values[PAWN];
    }
    const int captured = captured_piece_for_move(board, move);
    value += see_piece_values[piece_type(captured)];
    return value;
}

int staticExchangeEvaluation(const Board& board, const Move& move, int threshold) {
    const int from = from_sq(move);
    const int to = to_sq(move);
    const int movingPiece = board.mailbox[from];
    if (movingPiece == EMPTY) return 0;

    int balance;
    int nextVictim;
    Bitboard bishops, rooks, occupied, attackers, myAttackers;

    nextVictim = is_promotion(board, move) ? promotion_piece(board, move) : piece_type(movingPiece);
    balance = moveEstimatedValue(board, move) - threshold;
    if (balance < 0) return 0;

    balance -= see_piece_values[nextVictim];
    if (balance >= 0) return 1;

    bishops = board.piece[BISHOP - 1] | board.piece[QUEEN - 1];
    rooks = board.piece[ROOK - 1] | board.piece[QUEEN - 1];

    occupied = board.color[WHITE] | board.color[BLACK];
    occupied = (occupied ^ (1ULL << from)) | (1ULL << to);

    if (is_en_passant(move) || (piece_type(movingPiece) == PAWN && sq_to_col(from) != sq_to_col(to) && board.mailbox[to] == EMPTY)) {
        const int capSq = (piece_color(movingPiece) == WHITE) ? (to - 8) : (to + 8);
        occupied ^= (1ULL << capSq);
    }

    attackers = all_attackers_to_sq(board, to, occupied) & occupied;
    int colour = board.stm ^ 1;

    while (true) {
        myAttackers = attackers & board.color[colour];
        if (myAttackers == 0ULL) break;

        for (nextVictim = PAWN; nextVictim <= QUEEN; nextVictim++) {
            if (myAttackers & board.piece[nextVictim - 1]) break;
        }
        if (nextVictim > QUEEN) nextVictim = KING;

        Bitboard attackerBB = myAttackers & board.piece[nextVictim - 1];
        if (attackerBB == 0ULL) break;
        occupied ^= (1ULL << lsb(attackerBB));

        if (nextVictim == PAWN || nextVictim == BISHOP || nextVictim == QUEEN) {
            attackers |= bishop_attacks_on_the_fly(to, occupied) & bishops;
        }
        if (nextVictim == ROOK || nextVictim == QUEEN) {
            attackers |= rook_attacks_on_the_fly(to, occupied) & rooks;
        }

        attackers &= occupied;
        colour ^= 1;
        balance = -balance - 1 - see_piece_values[nextVictim];

        if (balance >= 0) {
            if (nextVictim == KING && (attackers & board.color[colour])) colour ^= 1;
            break;
        }
    }

    return board.stm != colour;
}

int see_exchange(const Board& board, const Move& move) {
    return staticExchangeEvaluation(board, move, 0);
}

bool is_insufficient_material(const Board& board) {
    if (board.piece[PAWN - 1] != 0) return false;
    if (board.piece[ROOK - 1] != 0) return false;
    if (board.piece[QUEEN - 1] != 0) return false;

    const int whiteKnights = popcount(board.piece[KNIGHT - 1] & board.color[WHITE]);
    const int whiteBishops = popcount(board.piece[BISHOP - 1] & board.color[WHITE]);
    const int blackKnights = popcount(board.piece[KNIGHT - 1] & board.color[BLACK]);
    const int blackBishops = popcount(board.piece[BISHOP - 1] & board.color[BLACK]);

    const int whiteMinors = whiteKnights + whiteBishops;
    const int blackMinors = blackKnights + blackBishops;

    if (whiteMinors == 0 && blackMinors == 0) return true;
    if (whiteMinors == 1 && blackMinors == 0) return true;
    if (blackMinors == 1 && whiteMinors == 0) return true;
    return false;
}
