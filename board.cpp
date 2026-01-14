#include "board.h"
#include "bitboard.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

char columns[] = "abcdefgh";

namespace {

constexpr Bitboard W_PAWNS   = 0x000000000000FF00ULL;
constexpr Bitboard W_KNIGHTS = 0x0000000000000042ULL;
constexpr Bitboard W_BISHOPS = 0x0000000000000024ULL;
constexpr Bitboard W_ROOKS   = 0x0000000000000081ULL;
constexpr Bitboard W_QUEEN   = 0x0000000000000008ULL;
constexpr Bitboard W_KING    = 0x0000000000000010ULL;

constexpr Bitboard B_PAWNS   = 0x00FF000000000000ULL;
constexpr Bitboard B_KNIGHTS = 0x4200000000000000ULL;
constexpr Bitboard B_BISHOPS = 0x2400000000000000ULL;
constexpr Bitboard B_ROOKS   = 0x8100000000000000ULL;
constexpr Bitboard B_QUEEN   = 0x0800000000000000ULL;
constexpr Bitboard B_KING    = 0x1000000000000000ULL;

inline Bitboard bit_at_sq(int sq) {
    return 1ULL << sq;
}

inline void bb_clear(Board& board, int piece, int sq) {
    if (piece == 0) return;
    int idx = std::abs(piece) - 1;
    int c = (piece > 0) ? WHITE : BLACK;
    Bitboard mask = bit_at_sq(sq);
    board.piece[idx] &= ~mask;
    board.color[c] &= ~mask;
}

inline void bb_set(Board& board, int piece, int sq) {
    if (piece == 0) return;
    int idx = std::abs(piece) - 1;
    int c = (piece > 0) ? WHITE : BLACK;
    Bitboard mask = bit_at_sq(sq);
    board.piece[idx] |= mask;
    board.color[c] |= mask;
}

inline void set_start_position(Board& board) {
    board.piece[pawn - 1]   = W_PAWNS | B_PAWNS;
    board.piece[knight - 1] = W_KNIGHTS | B_KNIGHTS;
    board.piece[bishop - 1] = W_BISHOPS | B_BISHOPS;
    board.piece[rook - 1]   = W_ROOKS | B_ROOKS;
    board.piece[queen - 1]  = W_QUEEN | B_QUEEN;
    board.piece[king - 1]   = W_KING | B_KING;

    board.color[WHITE] = W_PAWNS | W_KNIGHTS | W_BISHOPS | W_ROOKS | W_QUEEN | W_KING;
    board.color[BLACK] = B_PAWNS | B_KNIGHTS | B_BISHOPS | B_ROOKS | B_QUEEN | B_KING;

    for (int i = 0; i < 64; i++) board.mailbox[i] = 0;

    // Populate mailbox from bitboards
    for (int sq = 0; sq < 64; ++sq) {
        Bitboard mask = bit_at_sq(sq);
        if (board.color[WHITE] & mask) {
            if (board.piece[pawn - 1] & mask) board.mailbox[sq] = pawn;
            else if (board.piece[knight - 1] & mask) board.mailbox[sq] = knight;
            else if (board.piece[bishop - 1] & mask) board.mailbox[sq] = bishop;
            else if (board.piece[rook - 1] & mask) board.mailbox[sq] = rook;
            else if (board.piece[queen - 1] & mask) board.mailbox[sq] = queen;
            else if (board.piece[king - 1] & mask) board.mailbox[sq] = king;
        } else if (board.color[BLACK] & mask) {
            if (board.piece[pawn - 1] & mask) board.mailbox[sq] = -pawn;
            else if (board.piece[knight - 1] & mask) board.mailbox[sq] = -knight;
            else if (board.piece[bishop - 1] & mask) board.mailbox[sq] = -bishop;
            else if (board.piece[rook - 1] & mask) board.mailbox[sq] = -rook;
            else if (board.piece[queen - 1] & mask) board.mailbox[sq] = -queen;
            else if (board.piece[king - 1] & mask) board.mailbox[sq] = -king;
        }
    }
}

inline bool is_pawn_attack_possible(const Board& board, bool attackerIsWhite, int epSq) {
    Bitboard pawns = board.piece[pawn - 1] & board.color[attackerIsWhite ? WHITE : BLACK];
    if (attackerIsWhite) {
        return (pawn_attacks[BLACK][epSq] & pawns) != 0;
    }
    return (pawn_attacks[WHITE][epSq] & pawns) != 0;
}
} // namespace

Move::Move()
    : fromRow(0), fromCol(0), toRow(0), toCol(0), capturedPiece(0), promotion(0),
      prevW_KingSide(false), prevW_QueenSide(false), prevB_KingSide(false), prevB_QueenSide(false),
      prevEnPassantCol(-1), isEnPassant(false), isCastling(false) {}

Board::Board() {
    resetBoard();
}

void Board::resetBoard() {
    set_start_position(*this);

    whiteCanCastleKingSide = true;
    whiteCanCastleQueenSide = true;
    blackCanCastleKingSide = true;
    blackCanCastleQueenSide = true;
    isWhiteTurn = true;
    enPassantCol = -1;
    whiteKingRow = 7; whiteKingCol = 4;
    blackKingRow = 0; blackKingCol = 4;

    currentHash = position_key(*this);
}

void Board::makeMove(Move& move) {
    const Zobrist& z = zobrist();

    move.prevW_KingSide = whiteCanCastleKingSide;
    move.prevW_QueenSide = whiteCanCastleQueenSide;
    move.prevB_KingSide = blackCanCastleKingSide;
    move.prevB_QueenSide = blackCanCastleQueenSide;
    move.prevEnPassantCol = enPassantCol;

    move.isEnPassant = false;
    move.isCastling = false;

    const int fromSq = row_col_to_sq(move.fromRow, move.fromCol);
    const int toSq = row_col_to_sq(move.toRow, move.toCol);
    const int movingPiece = mailbox[fromSq];
    int target_piece = mailbox[toSq];

    move.capturedPiece = target_piece;

    // Hash: remove side-to-move, old ep and old castling
    currentHash ^= z.side;
    if (enPassantCol != -1) currentHash ^= z.epFile[enPassantCol];
    int oldCastling = 0;
    if (whiteCanCastleKingSide) oldCastling |= 1;
    if (whiteCanCastleQueenSide) oldCastling |= 2;
    if (blackCanCastleKingSide) oldCastling |= 4;
    if (blackCanCastleQueenSide) oldCastling |= 8;
    currentHash ^= z.castling[oldCastling];

    // Remove moving piece from origin
    currentHash ^= z.piece[piece_to_zobrist_index(movingPiece)][fromSq];

    bb_clear(*this, movingPiece, fromSq);
    if (target_piece != 0) {
        bb_clear(*this, target_piece, toSq);
        currentHash ^= z.piece[piece_to_zobrist_index(target_piece)][toSq];
    }
    bb_set(*this, movingPiece, toSq);

    mailbox[fromSq] = 0;
    if (target_piece != 0) mailbox[toSq] = 0;

    if (std::abs(movingPiece) == pawn && move.fromCol != move.toCol && move.capturedPiece == 0) {
        move.isEnPassant = true;
        int captureRow = move.fromRow;
        int captureSq = row_col_to_sq(captureRow, move.toCol);
        int capturedPawn = (movingPiece > 0) ? -pawn : pawn;
        bb_clear(*this, capturedPawn, captureSq);
        mailbox[captureSq] = 0;
        currentHash ^= z.piece[piece_to_zobrist_index(capturedPawn)][captureSq];
        move.capturedPiece = capturedPawn;
    }

    if (std::abs(movingPiece) == king && std::abs(move.fromCol - move.toCol) == 2) {
        if (move.toCol > move.fromCol) {
            int rookFromSq = row_col_to_sq(move.toRow, move.toCol + 1);
            int rookToSq = row_col_to_sq(move.toRow, move.toCol - 1);
            int rookPiece = (movingPiece > 0) ? rook : -rook;
            bb_clear(*this, rookPiece, rookFromSq);
            bb_set(*this, rookPiece, rookToSq);
            mailbox[rookToSq] = mailbox[rookFromSq];
            mailbox[rookFromSq] = 0;
            currentHash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookFromSq];
            currentHash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookToSq];
        } else {
            int rookFromSq = row_col_to_sq(move.toRow, move.toCol - 2);
            int rookToSq = row_col_to_sq(move.toRow, move.toCol + 1);
            int rookPiece = (movingPiece > 0) ? rook : -rook;
            bb_clear(*this, rookPiece, rookFromSq);
            bb_set(*this, rookPiece, rookToSq);
            mailbox[rookToSq] = mailbox[rookFromSq];
            mailbox[rookFromSq] = 0;
            currentHash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookFromSq];
            currentHash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookToSq];
        }
        move.isCastling = true;
    }

    int placedPiece = movingPiece;
    if (std::abs(movingPiece) == pawn) {
        int promotionRank = movingPiece > 0 ? 0 : 7;
        if (move.toRow == promotionRank && move.promotion != 0) {
            placedPiece = (movingPiece > 0) ? move.promotion : -move.promotion;
            bb_clear(*this, movingPiece, toSq);
            bb_set(*this, placedPiece, toSq);
        }
    }

    mailbox[toSq] = placedPiece;
    currentHash ^= z.piece[piece_to_zobrist_index(placedPiece)][toSq];

    if (std::abs(movingPiece) == pawn && std::abs(move.fromRow - move.toRow) == 2) {
        const bool opponentWhite = !isWhiteTurn;
        const int epRow = opponentWhite ? 2 : 5;
        const int epSq = row_col_to_sq(epRow, move.toCol);
        if (is_pawn_attack_possible(*this, opponentWhite, epSq)) {
            enPassantCol = move.toCol;
        } else {
            enPassantCol = -1;
        }
    } else {
        enPassantCol = -1;
    }

    isWhiteTurn = !isWhiteTurn;

    if (movingPiece == rook && move.fromRow == 7 && move.fromCol == 7){
        whiteCanCastleKingSide = false;
    }
    else if (movingPiece == rook && move.fromRow == 7 && move.fromCol == 0){
        whiteCanCastleQueenSide = false;
    }
    else if (movingPiece == -rook && move.fromRow == 0 && move.fromCol == 7){
        blackCanCastleKingSide = false;
    }
    else if (movingPiece == -rook && move.fromRow == 0 && move.fromCol == 0){
        blackCanCastleQueenSide = false;
    }

    if (movingPiece == king) {
        whiteCanCastleKingSide = false;
        whiteCanCastleQueenSide = false;
        whiteKingRow = move.toRow;
        whiteKingCol = move.toCol;
    }
    if (movingPiece == -king) {
        blackCanCastleKingSide = false;
        blackCanCastleQueenSide = false;
        blackKingRow = move.toRow;
        blackKingCol = move.toCol;
    }

    if (move.capturedPiece == rook && move.toRow == 7 && move.toCol == 7) {
        whiteCanCastleKingSide = false;
    }
    if (move.capturedPiece == rook && move.toRow == 7 && move.toCol == 0) {
        whiteCanCastleQueenSide = false;
    }
    if (move.capturedPiece == -rook && move.toRow == 0 && move.toCol == 7) {
        blackCanCastleKingSide = false;
    }
    if (move.capturedPiece == -rook && move.toRow == 0 && move.toCol == 0) {
        blackCanCastleQueenSide = false;
    }

    int newCastling = 0;
    if (whiteCanCastleKingSide) newCastling |= 1;
    if (whiteCanCastleQueenSide) newCastling |= 2;
    if (blackCanCastleKingSide) newCastling |= 4;
    if (blackCanCastleQueenSide) newCastling |= 8;
    currentHash ^= z.castling[newCastling];

    if (enPassantCol != -1) {
        currentHash ^= z.epFile[enPassantCol];
    }
}

void Board::unmakeMove(Move& move) {
    const Zobrist& z = zobrist();

    isWhiteTurn = !isWhiteTurn;

    // Hash out side, current ep, current castling (state after move, before undo)
    currentHash ^= z.side;
    if (enPassantCol != -1) currentHash ^= z.epFile[enPassantCol];
    int currCastling = 0;
    if (whiteCanCastleKingSide) currCastling |= 1;
    if (whiteCanCastleQueenSide) currCastling |= 2;
    if (blackCanCastleKingSide) currCastling |= 4;
    if (blackCanCastleQueenSide) currCastling |= 8;
    currentHash ^= z.castling[currCastling];

    const int fromSq = row_col_to_sq(move.fromRow, move.fromCol);
    const int toSq = row_col_to_sq(move.toRow, move.toCol);

    int pieceOnTo = mailbox[toSq];
    int pieceBase = pieceOnTo;
    if (move.promotion != 0) {
        pieceBase = (pieceOnTo > 0) ? pawn : -pawn;
    }

    // Remove moved piece from destination
    bb_clear(*this, pieceOnTo, toSq);
    mailbox[toSq] = 0;
    currentHash ^= z.piece[piece_to_zobrist_index(pieceOnTo)][toSq];

    // Undo castling rook move if needed
    if (move.isCastling) {
        if (move.toCol > move.fromCol) {
            int rookFromSq = row_col_to_sq(move.toRow, move.toCol - 1);
            int rookToSq = row_col_to_sq(move.toRow, move.toCol + 1);
            int rookPiece = isWhiteTurn ? rook : -rook;
            bb_clear(*this, rookPiece, rookFromSq);
            bb_set(*this, rookPiece, rookToSq);
            mailbox[rookFromSq] = 0;
            mailbox[rookToSq] = rookPiece;
            currentHash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookFromSq];
            currentHash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookToSq];
        } else {
            int rookFromSq = row_col_to_sq(move.toRow, move.toCol + 1);
            int rookToSq = row_col_to_sq(move.toRow, move.toCol - 2);
            int rookPiece = isWhiteTurn ? rook : -rook;
            bb_clear(*this, rookPiece, rookFromSq);
            bb_set(*this, rookPiece, rookToSq);
            mailbox[rookFromSq] = 0;
            mailbox[rookToSq] = rookPiece;
            currentHash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookFromSq];
            currentHash ^= z.piece[piece_to_zobrist_index(rookPiece)][rookToSq];
        }
    }

    // Restore moving piece to origin
    bb_set(*this, pieceBase, fromSq);
    mailbox[fromSq] = pieceBase;
    currentHash ^= z.piece[piece_to_zobrist_index(pieceBase)][fromSq];

    // Restore captured piece
    if (move.isEnPassant) {
        int capturedPawn = isWhiteTurn ? -pawn : pawn;
        int captureSq = row_col_to_sq(move.fromRow, move.toCol);
        bb_set(*this, capturedPawn, captureSq);
        mailbox[captureSq] = capturedPawn;
        currentHash ^= z.piece[piece_to_zobrist_index(capturedPawn)][captureSq];
    } else if (move.capturedPiece != 0) {
        bb_set(*this, move.capturedPiece, toSq);
        mailbox[toSq] = move.capturedPiece;
        currentHash ^= z.piece[piece_to_zobrist_index(move.capturedPiece)][toSq];
    }

    whiteCanCastleKingSide = move.prevW_KingSide;
    whiteCanCastleQueenSide = move.prevW_QueenSide;
    blackCanCastleKingSide = move.prevB_KingSide;
    blackCanCastleQueenSide = move.prevB_QueenSide;
    enPassantCol = move.prevEnPassantCol;

    if (pieceBase == king) {
        whiteKingRow = move.fromRow;
        whiteKingCol = move.fromCol;
    }
    if (pieceBase == -king) {
        blackKingRow = move.fromRow;
        blackKingCol = move.fromCol;
    }

    int prevCastling = 0;
    if (whiteCanCastleKingSide) prevCastling |= 1;
    if (whiteCanCastleQueenSide) prevCastling |= 2;
    if (blackCanCastleKingSide) prevCastling |= 4;
    if (blackCanCastleQueenSide) prevCastling |= 8;
    currentHash ^= z.castling[prevCastling];

    if (enPassantCol != -1) currentHash ^= z.epFile[enPassantCol];
}

void Board::loadFromFEN(const std::string& fen) {
    for (int i = 0; i < 6; i++) piece[i] = 0ULL;
    color[WHITE] = 0ULL;
    color[BLACK] = 0ULL;
    for (int i = 0; i < 64; i++) mailbox[i] = 0;

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
            int pieceType = 0;
            switch (std::tolower(static_cast<unsigned char>(c))) {
                case 'p': pieceType = pawn; break;
                case 'n': pieceType = knight; break;
                case 'b': pieceType = bishop; break;
                case 'r': pieceType = rook; break;
                case 'q': pieceType = queen; break;
                case 'k': pieceType = king; break;
            }
            int signedPiece = std::isupper(static_cast<unsigned char>(c)) ? pieceType : -pieceType;
            int sq = row_col_to_sq(row, col);
            bb_set(*this, signedPiece, sq);
            mailbox[sq] = signedPiece;

            if (pieceType == king) {
                if (std::isupper(static_cast<unsigned char>(c))) {
                    whiteKingRow = row;
                    whiteKingCol = col;
                } else {
                    blackKingRow = row;
                    blackKingCol = col;
                }
            }
            col++;
        }
    }

    isWhiteTurn = (turn == "w");
    whiteCanCastleKingSide = (castling.find('K') != std::string::npos);
    whiteCanCastleQueenSide = (castling.find('Q') != std::string::npos);
    blackCanCastleKingSide = (castling.find('k') != std::string::npos);
    blackCanCastleQueenSide = (castling.find('q') != std::string::npos);

    if (enPassant != "-") {
        enPassantCol = enPassant[0] - 'a';
        int epRow = isWhiteTurn ? 2 : 5;
        int epSq = row_col_to_sq(epRow, enPassantCol);
        if (!is_pawn_attack_possible(*this, isWhiteTurn, epSq)) {
            enPassantCol = -1;
        }
    } else {
        enPassantCol = -1;
    }

    currentHash = position_key(*this);
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
                int ap = std::abs(p);
                switch (ap) {
                    case pawn: ch = 'p'; break;
                    case knight: ch = 'n'; break;
                    case bishop: ch = 'b'; break;
                    case rook: ch = 'r'; break;
                    case queen: ch = 'q'; break;
                    case king: ch = 'k'; break;
                }
                if (p > 0) ch = static_cast<char>(ch - 'a' + 'A');
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
    Move move;
    move.fromCol = uci[0] - 'a';
    move.fromRow = 8 - (uci[1] - '0');
    move.toCol = uci[2] - 'a';
    move.toRow = 8 - (uci[3] - '0');
    if (uci.length() == 5) {
        char promoChar = uci[4];
        switch (promoChar) {
            case 'q': move.promotion = queen; break;
            case 'r': move.promotion = rook; break;
            case 'b': move.promotion = bishop; break;
            case 'n': move.promotion = knight; break;
        }
    }
    return move;
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
    int absP = std::abs(piece);
    if (absP < 1 || absP > 6) return -1;
    int base = (piece > 0) ? 0 : 6;
    return base + (absP - 1);
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

    int castle = 0;
    if (board.whiteCanCastleKingSide) castle |= 1;
    if (board.whiteCanCastleQueenSide) castle |= 2;
    if (board.blackCanCastleKingSide) castle |= 4;
    if (board.blackCanCastleQueenSide) castle |= 8;
    h ^= z.castling[castle];

    int ep = (board.enPassantCol >= 0 && board.enPassantCol < 8) ? board.enPassantCol : 8;
    h ^= z.epFile[ep];

    if (board.isWhiteTurn) h ^= z.side;
    return h;
}

bool is_threefold_repetition(const std::vector<uint64_t>& history) {
    if (history.empty()) return false;
    uint64_t current = history.back();
    int count = 0;
    for (int i = static_cast<int>(history.size()) - 1; i >= 0; i--) {
        if (history[i] == current) {
            count++;
            if (count >= 3) return true;
        }
    }
    return false;
}

TranspositionTable globalTT;

uint16_t TranspositionTable::packMove(const Move& m) {
    int from = row_col_to_sq(m.fromRow, m.fromCol);
    int to = row_col_to_sq(m.toRow, m.toCol);

    if (from < 0 || from >= 64) from = 0;
    if (to < 0 || to >= 64) to = 0;

    int promo = std::abs(m.promotion);
    if (promo < 0) promo = 0;
    if (promo > 7) promo = 7;

    return static_cast<uint16_t>((from & 63) | ((to & 63) << 6) | ((promo & 7) << 12));
}

Move TranspositionTable::unpackMove(uint16_t packed) {
    Move m;
    const int from = packed & 63;
    const int to = (packed >> 6) & 63;
    const int promo = (packed >> 12) & 7;

    m.fromRow = sq_to_row(from);
    m.fromCol = sq_to_col(from);
    m.toRow = sq_to_row(to);
    m.toCol = sq_to_col(to);
    m.promotion = promo;
    return m;
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

// SEE piece values; keep close to MVV/LVA ordering, not evaluation values
const int see_piece_values[] = {0, 100, 320, 330, 500, 900, 20000};

static Bitboard attackers_to_sq(Bitboard pieces[2][6], int sq, int side, Bitboard occ) {
    Bitboard attackers = 0ULL;

    // Pawns: to find pawns of 'side' that attack 'sq', use pawn_attacks of the opposite side.
    attackers |= ((side == WHITE) ? pawn_attacks[BLACK][sq] : pawn_attacks[WHITE][sq]) & pieces[side][pawn - 1];
    attackers |= knight_attacks[sq] & pieces[side][knight - 1];
    attackers |= king_attacks[sq] & pieces[side][king - 1];
    attackers |= bishop_attacks_on_the_fly(sq, occ) & (pieces[side][bishop - 1] | pieces[side][queen - 1]);
    attackers |= rook_attacks_on_the_fly(sq, occ) & (pieces[side][rook - 1] | pieces[side][queen - 1]);
    return attackers;
}

int see_exchange(const Board& board, const Move& move) {
    if (move.capturedPiece == 0 && !move.isEnPassant) return 0;

    int fromSq = row_col_to_sq(move.fromRow, move.fromCol);
    int toSq = row_col_to_sq(move.toRow, move.toCol);

    int movingPiece = piece_at_sq(board, fromSq);
    int captured = move.isEnPassant ? pawn : std::abs(move.capturedPiece);
    
    int victimValue = see_piece_values[captured];

    int gain[32];
    int depth = 0;

    gain[depth++] = victimValue;

    int side = board.isWhiteTurn ? WHITE : BLACK;
    int opp = side ^ 1;
    Bitboard occ = board.color[WHITE] | board.color[BLACK];
    Bitboard pieces[2][6];
    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 6; p++) {
            pieces[c][p] = board.piece[p] & board.color[c];
        }
    }

    if (!move.isEnPassant && move.capturedPiece != 0) {
        occ &= ~bit_at_sq(toSq);
        pieces[opp][std::abs(move.capturedPiece) - 1] &= ~bit_at_sq(toSq);
    }
    if (move.isEnPassant) {
        int capRow = move.fromRow;
        int capSq = row_col_to_sq(capRow, move.toCol);
        occ &= ~bit_at_sq(capSq);
        pieces[opp][pawn - 1] &= ~bit_at_sq(capSq);
    }
    occ &= ~bit_at_sq(fromSq);
    pieces[side][std::abs(movingPiece) - 1] &= ~bit_at_sq(fromSq);
    int promoteType = move.promotion != 0 ? std::abs(move.promotion) : std::abs(movingPiece);
    pieces[side][promoteType - 1] |= bit_at_sq(toSq);
    occ |= bit_at_sq(toSq);

    victimValue = see_piece_values[promoteType];
    
    int attackerSide = opp;

    while (true) {
        int attackerSq = -1;
        int nextVictimPiece = 0;

        Bitboard allAttackers = attackers_to_sq(pieces, toSq, attackerSide, occ);

        if ((pieces[attackerSide][pawn - 1] & allAttackers)) {
            attackerSq = lsb(pieces[attackerSide][pawn - 1] & allAttackers);
            nextVictimPiece = pawn;
        } else if ((pieces[attackerSide][knight - 1] & allAttackers)) {
            attackerSq = lsb(pieces[attackerSide][knight - 1] & allAttackers);
            nextVictimPiece = knight;
        } else if ((pieces[attackerSide][bishop - 1] & allAttackers)) {
            attackerSq = lsb(pieces[attackerSide][bishop - 1] & allAttackers);
            nextVictimPiece = bishop;
        } else if ((pieces[attackerSide][rook - 1] & allAttackers)) {
            attackerSq = lsb(pieces[attackerSide][rook - 1] & allAttackers);
            nextVictimPiece = rook;
        } else if ((pieces[attackerSide][queen - 1] & allAttackers)) {
            attackerSq = lsb(pieces[attackerSide][queen - 1] & allAttackers);
            nextVictimPiece = queen;
        } else if ((pieces[attackerSide][king - 1] & allAttackers)) {
            attackerSq = lsb(pieces[attackerSide][king - 1] & allAttackers);
            nextVictimPiece = king;
        } else {
            break;
        }

        gain[depth] = victimValue - gain[depth - 1];

        if (std::max(-gain[depth - 1], gain[depth]) < 0) break;

        occ &= ~bit_at_sq(attackerSq);
        pieces[attackerSide][nextVictimPiece - 1] &= ~bit_at_sq(attackerSq);

        attackerSide ^= 1;
        depth++;

        victimValue = see_piece_values[nextVictimPiece];
        
        if (depth >= 31) break;
    }

    while (--depth) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
    }

    return gain[0];
}
