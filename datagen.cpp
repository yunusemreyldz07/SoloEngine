#include "datagen.h"
#include "evaluation.h"
#include "search.h"
#include <iostream>
#include <fstream>
#include <random>
#include <thread>
#include <cmath>
#include <mutex>
#include <filesystem>

std::atomic<uint64_t> total_fens_generated{0};
std::atomic<uint64_t> games_played_count{0};

std::vector<std::string> book_lines;
std::mutex book_mutex;
bool book_loaded = false;

namespace {

void trim_incomplete_last_line(const std::string& file_path) {
    std::error_code ec;
    if (!std::filesystem::exists(file_path, ec)) {
        return;
    }

    std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        return;
    }

    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    if (size <= 0) {
        return;
    }

    char ch = '\0';
    file.seekg(size - 1);
    file.get(ch);
    if (ch == '\n') {
        return;
    }

    std::streamoff cut_pos = -1;
    for (std::streamoff i = size - 1; i >= 0; --i) {
        file.seekg(i);
        file.get(ch);
        if (ch == '\n') {
            cut_pos = i + 1;
            break;
        }
        if (i == 0) {
            break;
        }
    }

    file.close();
    if (cut_pos < 0) {
        cut_pos = 0;
    }
    std::filesystem::resize_file(file_path, static_cast<uintmax_t>(cut_pos), ec);
}

char piece_to_fen_char(int piece) {
    if (piece == 0) return '1';

    const int pt = piece_type(piece);
    char c = ' ';
    switch (pt) {
        case PAWN: c = 'p'; break;
        case KNIGHT: c = 'n'; break;
        case BISHOP: c = 'b'; break;
        case ROOK: c = 'r'; break;
        case QUEEN: c = 'q'; break;
        case KING: c = 'k'; break;
        default: c = ' '; break;
    }

    if (piece_color(piece) == WHITE) {
        c = static_cast<char>(c - 'a' + 'A');
    }

    return c;
}

std::string get_fen(const Board& board) {
    std::string fen;
    fen.reserve(96);

    for (int row = 0; row < 8; ++row) {
        int empty_count = 0;
        for (int col = 0; col < 8; ++col) {
            const int sq = row_col_to_sq(row, col);
            const int piece = board.mailbox[sq];
            if (piece == EMPTY) {
                ++empty_count;
                continue;
            }

            if (empty_count > 0) {
                fen.push_back(static_cast<char>('0' + empty_count));
                empty_count = 0;
            }

            fen.push_back(piece_to_fen_char(piece));
        }

        if (empty_count > 0) {
            fen.push_back(static_cast<char>('0' + empty_count));
        }

        if (row != 7) {
            fen.push_back('/');
        }
    }

    fen += (board.stm == WHITE) ? " w " : " b ";

    std::string castling;
    if (board.castling & CASTLE_WK) castling.push_back('K');
    if (board.castling & CASTLE_WQ) castling.push_back('Q');
    if (board.castling & CASTLE_BK) castling.push_back('k');
    if (board.castling & CASTLE_BQ) castling.push_back('q');
    if (castling.empty()) castling = "-";

    fen += castling;
    fen.push_back(' ');

    if (board.enPassant == -1) {
        fen += "-";
    } else {
        const char file = static_cast<char>('a' + board.enPassant);
        const int ep_row = (board.stm == WHITE) ? 2 : 5;
        const char rank = static_cast<char>('0' + (8 - ep_row));
        fen.push_back(file);
        fen.push_back(rank);
    }

    fen.push_back(' ');
    fen += std::to_string(board.halfMoveClock);
    fen.push_back(' ');
    const int full_move_number = 1 + static_cast<int>(board.moveHistory.size() / 2);
    fen += std::to_string(full_move_number);

    return fen;
}

bool is_in_check(const Board& pos) {
    int king_sq = -1;
    king_square(pos, pos.stm == WHITE, king_sq);
    if (king_sq == -1) return false;
    return is_square_attacked(pos, king_sq, pos.stm == BLACK);
}

} // namespace

void load_book(const std::string& filename) {
    std::lock_guard<std::mutex> lock(book_mutex);
    if (book_loaded) return;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Book not found: " << filename << ". Using starting position." << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.length() > 5) {
            book_lines.push_back(line);
        }
    }
    book_loaded = true;
    std::cout << book_lines.size() << " FENs loaded from book: " << filename << std::endl;
}

bool filtering(const Board& pos, int score, Move best_move) {
    if (std::abs(score) >= 20000) return true; // Mate scores
    if (is_in_check(pos)) return true;
    if (best_move != 0 && move_flags(best_move) >= FLAG_CAPTURE) return true;
    return false;
}

// Each position is stored with its FEN and the white-relative cp score from search.
struct FenRecord {
    std::string fen;
    int16_t score; // centipawns, white's perspective (positive = white better)
};

int play_selfgen_game(std::ofstream& out_file, int soft_nodes, bool use_book, std::mt19937_64& gen) {
    Board pos;
    pos.reset();

    if (use_book && !book_lines.empty()) {
        std::uniform_int_distribution<size_t> dist(0, book_lines.size() - 1);
        pos.loadFEN(book_lines[dist(gen)]);
    }

    // Play 8 random moves from the starting/book position
    for (int i = 0; i < 8; ++i) {
        Move legal_moves[256];
        int move_count = 0;
        get_all_moves(pos, legal_moves, move_count);
        if (move_count == 0) break;
        std::uniform_int_distribution<int> dist(0, move_count - 1);
        pos.makeMove(legal_moves[dist(gen)]);
    }

    std::vector<FenRecord> fen_list;
    fen_list.reserve(MAX_GAME_PLYS);

    double result = 0.5;
    int win_adj_count = 0;
    int draw_adj_count = 0;
    int game_ply = 0;

    while ((int)fen_list.size() < MAX_GAME_PLYS) {
        Move legal_moves[256];
        int move_count = 0;
        get_all_moves(pos, legal_moves, move_count);

        if (move_count == 0) {
            result = is_in_check(pos) ? (pos.stm == WHITE ? 0.0 : 1.0) : 0.5;
            break;
        }

        if (pos.halfMoveClock >= 100) {
            result = 0.5;
            break;
        }

        std::string current_fen = get_fen(pos);

        setSoftNodeLimit(soft_nodes);
        int16_t raw_score = 0;
        Move best_move = getBestMove(pos, 128, -1, {}, 0, true, raw_score);
        setSoftNodeLimit(-1);

        int16_t white_score = (pos.stm == WHITE) ? raw_score : static_cast<int16_t>(-raw_score);

        bool skip_save = filtering(pos, raw_score, best_move);
        if (!skip_save) {
            fen_list.push_back({current_fen, white_score});
        }

        // Win adjudication
        if (std::abs(raw_score) > 2000) win_adj_count++;
        else win_adj_count = 0;

        if (win_adj_count >= 5) {
            result = (raw_score > 0)
                ? (pos.stm == WHITE ? 1.0 : 0.0)
                : (pos.stm == WHITE ? 0.0 : 1.0);
            break;
        }

        // Draw adjudication
        if (game_ply >= 64) {
            if (std::abs(raw_score) < 15) draw_adj_count++;
            else draw_adj_count = 0;

            if (draw_adj_count >= 6) {
                result = 0.5;
                break;
            }
        } else {
            draw_adj_count = 0;
        }

        if (best_move == 0) break;
        pos.makeMove(best_move);
        game_ply++;
    }

    // Write positions in bullet trainer format: fen | score | wdl
    for (const auto& record : fen_list) {
        out_file << record.fen << " | " << record.score << " | " << result << "\n";
    }

    return static_cast<int>(fen_list.size());
}

void datagen_worker(int thread_id, uint64_t target_games, int soft_nodes, bool use_book, uint64_t seed) {
    // Each thread gets a unique seed derived from the base seed + thread_id
    std::mt19937_64 gen(seed + static_cast<uint64_t>(thread_id));

    std::string out_filename = "datagen_" + std::to_string(thread_id) + ".txt";
    trim_incomplete_last_line(out_filename);
    std::ofstream out_file(out_filename, std::ios::app);

    while (true) {
        uint64_t current_game = games_played_count.load();
        if (target_games > 0 && current_game >= target_games) break;

        games_played_count++;

        int new_fens = play_selfgen_game(out_file, soft_nodes, use_book, gen);
        total_fens_generated += new_fens;

        if (games_played_count % 100 == 0) {
            std::cout << "Games played: " << games_played_count
                      << " | FENs generated: " << total_fens_generated << std::endl;
        }
    }
}

void start_datagen(int num_threads, uint64_t target_games, int soft_nodes, bool use_book, uint64_t seed) {
    if (use_book) {
        load_book("UHO_Lichess_4852_v1.epd");
    }

    std::cout << "Starting datagen: threads=" << num_threads
              << " games=" << target_games
              << " soft_nodes=" << soft_nodes
              << " seed=" << seed << std::endl;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(datagen_worker, i, target_games, soft_nodes, use_book, seed);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Datagen finished! Total FENs: " << total_fens_generated << std::endl;
}
