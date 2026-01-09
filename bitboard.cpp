#include "bitboard.h"
#include "types.h"

#include <iostream>
#include <fstream>
#include <string.h>

// Constants and enums
#define no_sq 64

// Color definitions
#define WHITE 0
#define BLACK 1
#define BOTH 2

// FEN dedug positions
#define empty_board "8/8/8/8/8/8/8/8 w - - "
#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define tricky_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 "
#define killer_position "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define cmk_position "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NPP/PPP1NPB1/R1BQ1RK1 b - - 0 9 "

// Castling bits representation
/*
bin     dec
0000    0   no castling rights
0001    1   white can castle kingside
0010    2   white can castle queenside
0100    4   black can castle kingside
1000    8   black can castle queenside

1111    15  all castling rights
1001    9   white king side & black king side
*/
enum { wk = 1, wq = 2, bk = 4, bq = 8 }; 
// Square enumeration
enum {
    a1, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8
};

// Encode pieces
enum { P, N, B, R, Q, K, p, n, b, r, q, k };

// Bit manipulation macros
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define pop_bit(bitboard, square) (get_bit(bitboard, square) ? (bitboard) ^= (1ULL << (square)) : 0)
#define count_bits(bitboard) __builtin_popcountll(bitboard)
#define lsb(bitboard) __builtin_ctzll(bitboard)

// Globals and lookup tables
U64 bitboards[12];
U64 occupancies[3];
int side;
int enPassantSquare = no_sq;
int castle;

const char ascii_pieces[] = "PNBRQKpnbrqk";
int char_pieces[128];

const char *square_to_coordinates[] = {
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
};

// Attack masks
const U64 not_a_file = 18374403900871474942ULL;
const U64 not_h_file = 9187201950435737471ULL;
const U64 not_gh_file = 4557430888798830399ULL;
const U64 not_ab_file = 18229723555195321596ULL;

// Attack tables
U64 pawn_attacks[2][64];
U64 knight_attacks[64];
U64 king_attacks[64];
U64 bishop_masks[64];
U64 rook_masks[64];
U64 bishop_attacks[64][512];
U64 rook_attacks[64][4096];

// Relevant occupancy bits
const int bishop_relevant_bits[64] = {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6
};

const int rook_relevant_bits[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12
};

// Magic numbers
U64 bishop_magic_numbers[64] = {
    18018831494946945ULL,1134767471886336ULL,2308095375972630592ULL,27308574661148680ULL,9404081239914275072ULL,
    4683886618770800641ULL,216245358743802048ULL,9571253153235970ULL,27092002521253381ULL,1742811846410792ULL,
    8830470070272ULL,9235202921558442240ULL,1756410529322199040ULL,1127005325142032ULL,1152928124311179269ULL,
    2377913937382869017ULL,2314850493043704320ULL,4684324174200832257ULL,77688339246880000ULL,74309421802472544ULL,
    8649444578941734912ULL,4758897525753456914ULL,18168888584831744ULL,2463750540959940880ULL,9227893366251856128ULL,
    145276341141897348ULL,292821938185734161ULL,5190965918678714400ULL,2419567834477633538ULL,2308272929927873024ULL,
    18173279030480900ULL,612771170333492228ULL,4611976426970161409ULL,2270508834359424ULL,9223442681551127040ULL,
    144117389281722496ULL,1262208579542270208ULL,13988180992906560530ULL,4649975687305298176ULL,9809420809726464128ULL,
    1153222256471056394ULL,2901448468860109312ULL,40690797321924624ULL,4504295814726656ULL,299204874469892ULL,
    594838215186186752ULL,7210408796106130432ULL,144405467744964672ULL,145390656058359810ULL,1153203537948246016ULL,
    102002796048417802ULL,9243919728426124800ULL,2455024885924167748ULL,72066815467061280ULL,325424741529814049ULL,
    1175584649085829253ULL,18720594346444812ULL,584352516473913920ULL,1441151883179198496ULL,4919056693802862608ULL,
    1161950831810052608ULL,2464735771073020416ULL,54610562058947072ULL,580611413180448ULL
};

U64 rook_magic_numbers[64] = {
    11565248328107303040ULL,12123725398701785089ULL,900733188335206529ULL,72066458867205152ULL,144117387368072224ULL,216203568472981512ULL,9547631759814820096ULL,2341881152152807680ULL,
    140740040605696ULL,2316046545841029184ULL,72198468973629440ULL,81205565149155328ULL,146508277415412736ULL,703833479054336ULL,2450098939073003648ULL,576742228899270912ULL,
    36033470048378880ULL,72198881818984448ULL,1301692025185255936ULL,90217678106527746ULL,324684134750365696ULL,9265030608319430912ULL,4616194016369772546ULL,2199165886724ULL,
    72127964931719168ULL,2323857549994496000ULL,9323886521876609ULL,9024793588793472ULL,562992905192464ULL,2201179128832ULL,36038160048718082ULL,36029097666947201ULL,4629700967774814240ULL,
    306244980821723137ULL,1161084564161792ULL,110340390163316992ULL,5770254227613696ULL,2341876206435041792ULL,82199497949581313ULL,144120019947619460ULL,324329544062894112ULL,1152994210081882112ULL,
    13545987550281792ULL,17592739758089ULL,2306414759556218884ULL,144678687852232706ULL,9009398345171200ULL,2326183975409811457ULL,72339215047754240ULL,18155273440989312ULL,4613959945983951104ULL,
    145812974690501120ULL,281543763820800ULL,147495088967385216ULL,2969386217113789440ULL,19215066297569792ULL,180144054896435457ULL,2377928092116066437ULL,9277424307650174977ULL,4621827982418248737ULL,
    563158798583922ULL,5066618438763522ULL,144221860300195844ULL,281752018887682ULL
};

// Forward declarations
void init_leapers_attack();
void init_slider_attacks(int bishop);
void init_magic_numbers();
U64 mask_pawn_attacks(int square, int side);
U64 mask_knight_attacks(int square);
U64 mask_king_attacks(int square);
U64 mask_bishop_attacks(int square);
U64 mask_rook_attacks(int square);
U64 bishop_attacks_otf(int square, U64 block);
U64 rook_attacks_otf(int square, U64 block);
U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask);
U64 find_magic_number(int square, int relevant_bits, int bishop);

// Random and helpers
unsigned int state = 1804289383;

unsigned int get_random_U32_number() {
    unsigned int number = state;
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;
    state = number;
    return number;
}

U64 get_random_U64_number() {
    U64 n1, n2, n3, n4;
    n1 = (U64)(get_random_U32_number()) & 0xFFFF;
    n2 = (U64)(get_random_U32_number()) & 0xFFFF;
    n3 = (U64)(get_random_U32_number()) & 0xFFFF;
    n4 = (U64)(get_random_U32_number()) & 0xFFFF;
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

U64 generate_magic_number() {
    return get_random_U64_number() & get_random_U64_number() & get_random_U64_number();
}

// Initialization
static void init_char_pieces() {
    for (int i = 0; i < 128; ++i) {
        char_pieces[i] = -1;
    }
    char_pieces['P'] = P; char_pieces['N'] = N; char_pieces['B'] = B;
    char_pieces['R'] = R; char_pieces['Q'] = Q; char_pieces['K'] = K;
    char_pieces['p'] = p; char_pieces['n'] = n; char_pieces['b'] = b;
    char_pieces['r'] = r; char_pieces['q'] = q; char_pieces['k'] = k;
}

void init_leapers_attack() {
    for (int square = 0; square < 64; square++) {
        pawn_attacks[WHITE][square] = mask_pawn_attacks(square, WHITE);
        pawn_attacks[BLACK][square] = mask_pawn_attacks(square, BLACK);
        knight_attacks[square] = mask_knight_attacks(square);
        king_attacks[square] = mask_king_attacks(square);
    }
}

void init_magic_numbers() {
    for (int square = 0; square < 64; square++) {
        bishop_magic_numbers[square] = find_magic_number(square, bishop_relevant_bits[square], BISHOP);
        rook_magic_numbers[square] = find_magic_number(square, rook_relevant_bits[square], ROOK);
    }
}

void init_slider_attacks(int bishop) {
    for (int square = 0; square < 64; square++) {
        bishop_masks[square] = mask_bishop_attacks(square);
        rook_masks[square] = mask_rook_attacks(square);

        U64 attack_mask = bishop ? bishop_masks[square] : rook_masks[square];
        int relevant_bits_count = count_bits(attack_mask);
        int occupancy_indices = 1 << relevant_bits_count;

        for (int index = 0; index < occupancy_indices; index++) {
            U64 occupancy = set_occupancy(index, relevant_bits_count, attack_mask);
            int magic_index;
            if (bishop) {
                U64 occupancy = set_occupancy(index, relevant_bits_count, attack_mask);
                magic_index = (int)((occupancy * bishop_magic_numbers[square]) >> (64 - bishop_relevant_bits[square]));
                bishop_attacks[square][magic_index] = bishop_attacks_otf(square, occupancy);
            } else {
                magic_index = (int)((occupancy * rook_magic_numbers[square]) >> (64 - rook_relevant_bits[square]));
                rook_attacks[square][magic_index] = rook_attacks_otf(square, occupancy);
            }
        }
    }
}

void init_all() {
    init_leapers_attack();
    init_slider_attacks(BISHOP);
    init_slider_attacks(ROOK);
    init_char_pieces();
    // init_magic_numbers();
}

// Printing
void print_bitboard(U64 bitboard) {
    std::cout << "\n";
    for (int rank = 0; rank < 8; rank++) {
        std::cout << " " << (8 - rank) << "  ";
        for (int file = 0; file < 8; file++) {
            int square = (7 - rank) * 8 + file;
            std::cout << " " << ((get_bit(bitboard, square)) ? "1" : "0") << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n     a  b  c  d  e  f  g  h\n\n";
    std::cout << "     Bitboard: " << bitboard << "\n\n";
}

void print_board(){
    std::cout << "\n";
    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            int square = (7 - rank) * 8 + file;
            
            if (!file) std::cout << " " << (8 - rank) << " "; // Print rank numbers
            int piece = -1;
            // loop over 12 bitboards to find which piece is on this square
            for (int bb_piece = P; bb_piece <= k; bb_piece++) {
                if (get_bit(bitboards[bb_piece], square)) {
                    piece = bb_piece;
                    break;
                }
            }

            piece = piece == -1 ? '.' : ascii_pieces[piece];
            std::cout << (char)(piece) << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n   a b c d e f g h\n\n";

    std::cout << "Side to move: " << (side == WHITE ? "White" : "Black") << "\n";

    std::cout << "En Passant Square: ";
    if (enPassantSquare != no_sq) {
        std::cout << square_to_coordinates[enPassantSquare] << "\n";
    } else {
        std::cout << "None\n";
    }

    std::cout << "Castling Rights: "<< (castle & wk ? "K" : "-") 
                                    << (castle & wq ? "Q" : "-")
                                    << (castle & bk ? "k" : "-") 
                                    << (castle & bq ? "q" : "-") << "\n\n";
}

// parse FEN string
void parse_fen(const char *fen)
{
    // reset board position (bitboards)
    memset(bitboards, 0ULL, sizeof(bitboards));
    
    // reset occupancies (bitboards)
    memset(occupancies, 0ULL, sizeof(occupancies));
    
    // reset game state variables
    side = WHITE;
    enPassantSquare = no_sq;
    castle = 0;
    
    // loop over board ranks
    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            // init current square
            int square = (7 - rank) * 8 + file;
            
            // match ascii pieces within FEN string
            if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z'))
            {
                // init piece type
                int piece = char_pieces[*fen];
                
                // set piece on corresponding bitboard
                set_bit(bitboards[piece], square);
                
                // increment pointer to FEN string
                fen++;
            }
            
            // match empty square numbers within FEN string
            if (*fen >= '0' && *fen <= '9')
            {
                // init offset (convert char 0 to int 0)
                int offset = *fen - '0';
                
                // define piece variable
                int piece = -1;
                
                // loop over all piece bitboards
                for (int bb_piece = P; bb_piece <= k; bb_piece++)
                {
                    // if there is a piece on current square
                    if (get_bit(bitboards[bb_piece], square))
                        // get piece code
                        piece = bb_piece;
                }
                
                // on empty current square
                if (piece == -1)
                    // decrement file
                    file--;
                
                // adjust file counter
                file += offset;
                
                // increment pointer to FEN string
                fen++;
            }
            
            // match rank separator
            if (*fen == '/')
                // increment pointer to FEN string
                fen++;
        }
    }
    
    // got to parsing side to move (increment pointer to FEN string)
    fen++;
    
    // parse side to move
    (*fen == 'w') ? (side = WHITE) : (side = BLACK);
    
    // go to parsing castling rights
    fen += 2;
    
    // parse castling rights
    while (*fen != ' ')
    {
        switch (*fen)
        {
            case 'K': castle |= wk; break;
            case 'Q': castle |= wq; break;
            case 'k': castle |= bk; break;
            case 'q': castle |= bq; break;
            case '-': break;
        }

        // increment pointer to FEN string
        fen++;
    }
    
    // got to parsing enpassant square (increment pointer to FEN string)
    fen++;
    
    // parse enpassant square
    if (*fen != '-')
    {
        // parse enpassant file & rank
        int file = fen[0] - 'a';
        int rank = fen[1] - '1';
        
        // init enpassant square
        enPassantSquare = rank * 8 + file;
    }
    
    // no enpassant square
    else
        enPassantSquare = no_sq;
    
    // loop over white pieces bitboards
    for (int piece = P; piece <= K; piece++)
        occupancies[WHITE] |= bitboards[piece];
    
    // loop over black pieces bitboards
    for (int piece = p; piece <= k; piece++)
        occupancies[BLACK] |= bitboards[piece];
    
    // init all occupancies
    occupancies[BOTH] |= occupancies[WHITE];
    occupancies[BOTH] |= occupancies[BLACK];
}

// Attack masks
U64 mask_pawn_attacks(int square, int side) {
    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;

    set_bit(bitboard, square);

    if (side == WHITE) {
        if ((bitboard << 9) & not_a_file) attacks |= (bitboard << 9);
        if ((bitboard << 7) & not_h_file) attacks |= (bitboard << 7);
    } else {
        if ((bitboard >> 9) & not_h_file) attacks |= (bitboard >> 9);
        if ((bitboard >> 7) & not_a_file) attacks |= (bitboard >> 7);
    }
    return attacks;
}

U64 mask_knight_attacks(int square) {
    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);

    if ((bitboard << 17) & not_a_file) attacks |= (bitboard << 17);
    if ((bitboard << 15) & not_h_file) attacks |= (bitboard << 15);
    if ((bitboard << 10) & not_ab_file) attacks |= (bitboard << 10);
    if ((bitboard << 6) & not_gh_file) attacks |= (bitboard << 6);
    if ((bitboard >> 17) & not_h_file) attacks |= (bitboard >> 17);
    if ((bitboard >> 15) & not_a_file) attacks |= (bitboard >> 15);
    if ((bitboard >> 10) & not_gh_file) attacks |= (bitboard >> 10);
    if ((bitboard >> 6) & not_ab_file) attacks |= (bitboard >> 6);
    return attacks;
}

U64 mask_king_attacks(int square) {
    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);

    if ((bitboard << 8)) attacks |= (bitboard << 8);
    if ((bitboard << 9) & not_a_file) attacks |= (bitboard << 9);
    if ((bitboard << 7) & not_h_file) attacks |= (bitboard << 7);
    if ((bitboard << 1) & not_a_file) attacks |= (bitboard << 1);
    if ((bitboard >> 8)) attacks |= (bitboard >> 8);
    if ((bitboard >> 9) & not_h_file) attacks |= (bitboard >> 9);
    if ((bitboard >> 7) & not_a_file) attacks |= (bitboard >> 7);
    if ((bitboard >> 1) & not_h_file) attacks |= (bitboard >> 1);
    return attacks;
}

U64 mask_bishop_attacks(int square) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++)
        attacks |= (1ULL << (r * 8 + f));
    for (r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--)
        attacks |= (1ULL << (r * 8 + f));
    for (r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++)
        attacks |= (1ULL << (r * 8 + f));
    for (r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--)
        attacks |= (1ULL << (r * 8 + f));

    return attacks;
}

U64 mask_rook_attacks(int square) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1; r <= 6; r++)
        attacks |= (1ULL << (r * 8 + tf));
    for (r = tr - 1; r >= 1; r--)
        attacks |= (1ULL << (r * 8 + tf));
    for (f = tf + 1; f <= 6; f++)
        attacks |= (1ULL << (tr * 8 + f));
    for (f = tf - 1; f >= 1; f--)
        attacks |= (1ULL << (tr * 8 + f));

    return attacks;
}

// Attack generation (on the fly)
U64 bishop_attacks_otf(int square, U64 block) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block) break;
    }

    for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block) break;
    }
    for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block) break;
    }
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block) break;
    }

    return attacks;
}

U64 rook_attacks_otf(int square, U64 block) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1; r <= 7; r++) {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block) break;
    }
    for (r = tr - 1; r >= 0; r--) {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block) break;
    }
    for (f = tf + 1; f <= 7; f++) {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block) break;
    }
    for (f = tf - 1; f >= 0; f--) {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block) break;
    }

    return attacks;
}

// Occupancy helpers
U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask) {
    U64 occupancy = 0ULL;
    for (int count = 0; count < bits_in_mask; count++) {
        int square = lsb(attack_mask);
        pop_bit(attack_mask, square);
        if (index & (1 << count)) {
            occupancy |= (1ULL << square);
        }
    }
    return occupancy;
}

// Magics
U64 find_magic_number(int square, int relevant_bits, int bishop) {
    U64 occupancies[4096];
    U64 attacks[4096];
    U64 used_attacks[4096];

    U64 attack_mask = bishop ? mask_bishop_attacks(square) : mask_rook_attacks(square);
    int occupancy_indices = 1 << relevant_bits;

    for (int index = 0; index < occupancy_indices; index++) {
        occupancies[index] = set_occupancy(index, relevant_bits, attack_mask);
        attacks[index] = bishop ? bishop_attacks_otf(square, occupancies[index])
                                : rook_attacks_otf(square, occupancies[index]);
    }

    for (int random_count; random_count < 100000000; random_count++) {
        U64 magic_number = generate_magic_number();
        if (count_bits((attack_mask * magic_number) & 0xFF00000000000000ULL) < 6) continue;

        memset(used_attacks, 0ULL, sizeof(used_attacks));

        int index, fail;
        for (index = 0, fail = 0; !fail && index < occupancy_indices; index++) {
            int magic_index = (int)((occupancies[index] * magic_number) >> (64 - relevant_bits));
            if (used_attacks[magic_index] == 0ULL) {
                used_attacks[magic_index] = attacks[index];
            } else if (used_attacks[magic_index] != attacks[index]) {
                fail = 1;
            }
        }

        if (!fail) {
            return magic_number;
        }
    }
    std::cout << "Failed to find magic number for square " << square << "\n";
    return 0ULL;
}

// Public attack lookups
U64 get_bishop_attacks(int square, U64 occupancy) {
    occupancy &= bishop_masks[square];
    occupancy *= bishop_magic_numbers[square];
    occupancy >>= (64 - bishop_relevant_bits[square]);
    return bishop_attacks[square][occupancy];
}

U64 get_rook_attacks(int square, U64 occupancy) {
    occupancy &= rook_masks[square];
    occupancy *= rook_magic_numbers[square];
    occupancy >>= (64 - rook_relevant_bits[square]);
    return rook_attacks[square][occupancy];
}

U64 get_queen_attacks(int square, U64 occupancy) {
    return get_bishop_attacks(square, occupancy) | get_rook_attacks(square, occupancy);
}

static inline int is_square_attacked(int square, int by_side) {
    if (by_side == WHITE) {
        if (pawn_attacks[BLACK][square] & bitboards[P]) return 1;
        if (knight_attacks[square] & bitboards[N]) return 1;
        if (king_attacks[square] & bitboards[K]) return 1;
        if (get_bishop_attacks(square, occupancies[BOTH]) & (bitboards[B] | bitboards[Q])) return 1;
        if (get_rook_attacks(square, occupancies[BOTH]) & (bitboards[R] | bitboards[Q])) return 1;
        return 0;
    }

    if (pawn_attacks[WHITE][square] & bitboards[p]) return 1;
    if (knight_attacks[square] & bitboards[n]) return 1;
    if (king_attacks[square] & bitboards[k]) return 1;
    if (get_bishop_attacks(square, occupancies[BOTH]) & (bitboards[b] | bitboards[q])) return 1;
    if (get_rook_attacks(square, occupancies[BOTH]) & (bitboards[r] | bitboards[q])) return 1;
    return 0;
}

void print_attacked_squares(int by_side) {
    std::cout << "\n";
    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            int square = (7 - rank) * 8 + file;
            if (!file) std::cout << " " << (8 - rank) << " ";
            std::cout << " " << (is_square_attacked(square, by_side) ? '1' : '0');
        }
        std::cout << "\n";
    }
    std::cout << "\n     a b c d e f g h\n\n";
}

// Demo entry
#ifdef BITBOARD_DEMO
int main() {
    init_all();
    parse_fen(cmk_position);
    side = WHITE;
    enPassantSquare = e6;
    castle |= wk | wq | bk | bq; // all castling rights
    print_bitboard(get_queen_attacks(d4, occupancies[BOTH]));
    print_attacked_squares(BLACK);
    print_bitboard(bitboards[P]);
    print_board();
    return 0;
}
#endif
