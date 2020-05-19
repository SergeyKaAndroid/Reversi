#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <iostream>
#include <chrono>
using namespace std;
using namespace std::chrono;
bool gDebug = (NULL != getenv("DEBUG"));


// Bitmask manipulations, to represent playing field as a uint64_t
static uint64_t col(unsigned n) { return 0x0101010101010101ULL << n; }
static uint64_t row(unsigned n) { return 0xFFULL << (n << 3); }
static const uint64_t edgeL = col(0), edgeR = col(7), edgeT = row(0), edgeB = row(7);
static uint64_t moveL(uint64_t mask) { return (mask & ~edgeL) >> 1; }
static uint64_t moveR(uint64_t mask) { return (mask & ~edgeR) << 1; }
static uint64_t moveU(uint64_t mask) { return (mask & ~edgeT) >> 8; }
static uint64_t moveD(uint64_t mask) { return (mask & ~edgeB) << 8; }
static uint64_t neighbors(uint64_t mask) {
    mask |= moveL(mask) | moveR(mask);
    return mask | moveU(mask) | moveD(mask);
}

static uint64_t fromRC(int r, int c) { return (1ULL << c) << (r << 3); }
static uint64_t fromStr(string str) { // Parses a squre reference from a string
    int r=-1, c=-1;                   // like "e2" into a single bit mask
    for(char ch : str)
        if(ch > '0' && ch < '9')
            r = ch - '1';
        else if(ch >= 'a' && ch <= 'h')
            c = ch - 'a';
    if(r < 0 || c < 0)
        return 0;
    else
        return fromRC(r, c);
}
static string toStr(uint64_t m) { // Converts a mask to a space-separated square
    string s = "";                // reference list like "e2 e4 f7 "
    for(int r=0; r<8; r++)
        for(int c=0; c<8; c++, m>>=1)
            if(m & 1ULL) {
                char buf[4] = {'a'+c, '1'+r, ' ', 0};
                s = s + string(buf);
            }
    return s;
}

// Verification/benchmark, paste anywhere executable:
// for(uint64_t m=0; m<(1ULL<<28); m++)
//    if(bitCountDense(m | (m<<30)) != bitCountSparse(m | (m<<30)))
//        return -1;

// BitCount quickest for args that have only a few 1's
static int bitCountSparse(uint64_t m) {
    int ret = 0;
    for(; m ; m &= (m-1))
        ret++;
    return ret;
}

#ifndef __has_builtin       // Optional of course.
#define __has_builtin(x) 0  // Compatibility with non-clang compilers.
#endif
#if __has_builtin(__builtin_popcountll)
#define bitCountDense(x) __builtin_popcountll(x)
#else
// BitCount quickest for args that may have any number of 1's,
// almost as fast as __builtin_popcountll()
static int _bitCountDense(uint64_t m) {
    uint64_t acc = m & 0x0101010101010101ULL;
    for(int i=1; i<8; i++)
        acc += (0x0101010101010101ULL & (m >> i));
    acc += (acc >> 32);
    acc += (acc >> 16);
    acc += (acc >> 8);
    return (int)(acc & 255ull);
}
#endif

class Board {
public:
    uint64_t x, o;
    Board(uint64_t x, uint64_t o): x(x), o(o) {};
    Board(): x(fromRC(4,3) | fromRC(3,4)),
             o(fromRC(3,3) | fromRC(4,4)) {};

    static uint64_t captures(uint64_t move, uint64_t me, uint64_t he) {
        if( (!move) || (move & (me | he) ) || (move & (move-1)) )
            return 0ULL; // Invalid moves: none, non-empty squares, multiple squares

        uint64_t acc = 0;
        // Follow from <Move> in each direction as long as we flip cells
        for(int dr = -1; dr < 2; dr++)
            for(int dc = -1; dc < 2; dc++) {
                uint64_t i = move, cap = 0;
                do { // Follow the line from <Move> in a given direction
                    switch(dr) {
                        case -1: i = moveU(i); break;
                        case  1: i = moveD(i); break;
                    }
                    switch(dc) {
                        case -1: i = moveL(i); break;
                        case  1: i = moveR(i); break;
                    }
                    if(he & i)
                        cap |= i; // One of his dots, count as flipped
                    else if(me & i)
                        break;    // One of my dots, confirm the flip line
                    else
                        cap = 0;  // Neither his nor mine, discard the flip line
                } while(cap);
                acc |= cap;
            }
        return acc ? (move | acc) : 0;
    }

    static uint64_t possibleMoves(uint64_t me, uint64_t he) {
        uint64_t consider = neighbors(he);
        for(uint64_t i = 1; i; i <<= 1)
            if(consider & i)
                if(!captures(i, me, he))
                    consider &= ~i;
        return consider;
    }
    uint64_t possibleMoves(bool xMove) const {
        return possibleMoves( xMove ? x : o,
                              xMove ? o : x );
    }

    Board move(bool xMove, uint64_t m) const {
        Board ret(*this);
        uint64_t &me = xMove ? ret.x : ret.o;
        uint64_t &he = xMove ? ret.o : ret.x;
        uint64_t cap = captures(m, me, he);
        me |= cap;
        he &= ~cap;
        return ret;
    }
    Board move(bool xMove, int r, int c) const {
        return move(xMove, fromRC(r, c));
    }
    Board move(bool xMove, const string str) const {
        return move(xMove, fromStr(str));
    }
};

std::ostream& operator<< (ostream &out, const Board &board) {
    const char charSet[] = {'.', 'X', 'O', '?'};
    char buf[20] = "  a b c d e f g h\n";
    out << buf;
    uint64_t x = board.x, o = board.o;
    for(int r=0; r<8; r++) {
        buf[0] = '1' + r;
        for(int c=0; c<8; c++) {
            buf[2*(c+1)] = charSet[ (x&1) + 2*(o&1) ];
            x >>= 1; o >>= 1;
        }
        out << buf;
    }
    return out << "  a b c d e f g h\n";
}

class Player {
public:
    bool isX;
    Player(bool x): isX(x) {};
    virtual uint64_t think(const Board &b, uint64_t ops) = 0;
    virtual void printStats() {};
};


// Only random AI for now
class RandomAI : Player {
public:
    RandomAI(bool x): Player(x) { srandomdev(); };
    virtual uint64_t think(const Board &b, uint64_t ops);
};

// Human
class Human : Player {
public:
    Human(bool x): Player(x) {};
    virtual uint64_t think(const Board &b, uint64_t ops);
};

class AlphaBeta : Player {
    int IQ = 20; // Aim to evaluate up to 2^IQ board configs
    uint64_t edges, corners;
    int maxEval=0, maxMsec=0;
    unsigned incidence;
    int histo[65];
    int abMin(Board b, int depth, int alpha, int beta);
    int abMax(Board b, int depth, int alpha, int beta);
    int abScore(const Board &b);
public:
    AlphaBeta(bool x): Player(x), maxEval(0), maxMsec(0), IQ(20) {
        edges = row(0) | row(7) | col(0) | col(7);
        corners = (row(0) | row(7)) & (col(0) | col(7));
    };
    virtual uint64_t think(const Board &b, uint64_t ops);
    virtual void printStats() {
        cout << "Evaluated max of " << maxEval << " boards, "
             << maxMsec/1000000. << "sec\n";
    }

};

int main(int argc, char **argv) {
    Board b;
    char *env = getenv("AI"); // Execute: AI=XY ./a.out
    bool xMove = true;
    Player
        *playerX = (env && strchr(env,'X'))
        ? (Player*)new AlphaBeta(true)
        : (Player*)new Human(true),
        *playerO = (env && strchr(env,'O'))
        ? (Player*)new AlphaBeta(false)
        : (Player*)new Human(false);

    for(bool done=false, xMove=true; ; xMove = !xMove) {
        uint64_t ops = b.possibleMoves(xMove);
        cout << Board(b.x | ops, b.o | ops)
             << (xMove?"X: ":"O: ") << toStr(ops);
        if(!ops) {
            cout << (xMove?"X can't move\n":"O can't move\n");
            if(done)
                break;
            done = true;
            continue;
        } else if(0 == (ops & (ops-1)))
            cout << "Only one move - automatic\n";
        else
            ops = (xMove ? playerX : playerO)->think(b, ops);
        cout << ":" << toStr(ops) << "\n";
        b = b.move(xMove, ops);
        done = false;
    }
    int x=0, o=0;
    for(uint64_t m=1; m; m<<=1) {
        if(b.x & m) x++;
        if(b.o & m) o++;
    }
    cout << b << "Game over. X:" << x << " O:" << o << "\n";
    if(gDebug) {
        playerX->printStats();
        playerO->printStats();
    }
    return 0;
}

uint64_t RandomAI::think(const Board &b, uint64_t ops) {
    uint64_t choices[64]; int n = 0;
    for(uint64_t i=1; i; i<<=1)
        if(ops & i)
            choices[n++] = i;
    return choices[random()%n];
}

uint64_t Human::think(const Board &b, uint64_t ops) {
    cout << ": ";
    for(;;) {
        string str;
        cin >> str;
        uint64_t m = fromStr(str);
        if(m & ops)
            return m;
        else
            cout << "Invalid move\n";
    }
}

int AlphaBeta::abScore(const Board &b) {
    uint64_t x = b.x, o = b.o;
    incidence++;
    int xCount=bitCountDense(x),
        oCount=bitCountDense(o);
    histo[xCount+oCount]++;
    if( xCount+oCount == 64 ) { // Board is full
        if(xCount == oCount) return 0;
        return xCount > oCount ? INT_MAX : INT_MIN;
    }

    int gamePhaseFactor = (80-oCount-xCount)/16,
        xSides = bitCountSparse(x & edges),
        oSides = bitCountSparse(o & edges),
        xCorners = bitCountSparse(x & corners),
        oCorners = bitCountSparse(o & corners);
        
    return xCount - oCount +
        gamePhaseFactor * (xSides - oSides +
                           gamePhaseFactor * (xCorners - oCorners));
}

int AlphaBeta::abMin(Board b, int depth, int alpha, int beta) {
    if(depth==0)
        return abScore(b);
    uint64_t ops = b.possibleMoves(false);
    if(!ops)
        return abMax(b, b.possibleMoves(true)?depth:0, alpha, beta);
    int vMin = INT_MAX;
    depth /= bitCountSparse(ops);
    for(uint64_t m=ops; m; ops = m) {
        m &= m-1;
        uint64_t move = ops & ~m;
        int v = abMax(b.move(false, move), depth, alpha, beta);
        if(v < vMin) {
            vMin = v;
            if(vMin < beta)
                beta = vMin;
            if(alpha >= beta)
                break;
        }
    }
    return vMin;
}
int AlphaBeta::abMax(Board b, int depth, int alpha, int beta) {
    if(depth==0)
        return abScore(b);
    uint64_t ops = b.possibleMoves(true);
    if(!ops)
        return abMin(b, b.possibleMoves(false)?depth:0, alpha, beta);
    int vMax = INT_MIN;
    depth /= bitCountSparse(ops);
    for(uint64_t m=ops; m; ops = m) {
        m &= m-1;
        uint64_t move = ops & ~m;
        int v = abMin(b.move(true, move), depth, alpha, beta);
        if(v > vMax) {
            vMax = v;
            if(vMax > alpha)
                alpha = vMax;
            if(alpha >= beta)
                break;
        }
    }
    return vMax;
}

uint64_t AlphaBeta::think(const Board &b, uint64_t ops) {
    int nMoves = bitCountSparse(ops);
        int depth = (1<<IQ) / nMoves;
    uint64_t bestMove = 0;
    auto start = high_resolution_clock::now();
    incidence=0;
    memset(histo, 0, sizeof(histo));
    if(isX) { // Maximize
        int alpha = INT_MIN;
        for(uint64_t move = (1ULL << 63); move; move>>=1)
            if(ops & move) {
                int v = abMin(b.move(true, move), depth, alpha, INT_MAX);
                if(v > alpha) {
                    bestMove = move;
                    alpha = v;
                }
            }
        if(ops && (alpha == INT_MAX) && gDebug)
            cout << "\nX is gonna win\n";
    } else { // Minimize
        int beta = INT_MAX;
        for(uint64_t move = (1ULL << 63); move; move>>=1)
            if(ops & move) {
                int v = abMax(b.move(false, move), depth, INT_MIN, beta);
                if(v < beta) {
                    bestMove = move;
                    beta = v;
                }
            }
        if(ops && (beta == INT_MIN) && gDebug)
            cout << "\nO is gonna win\n";
    }
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    int mc = duration.count();
    if(mc > 300000) IQ--;
    else if(mc < 150000) IQ++;
    if(mc > maxMsec) {
        maxMsec = mc;
        maxEval = incidence;
    }
    int base = bitCountDense(b.x | b.o);
    cout << "\nEvaluated " << incidence << " boards, "
         << mc/1000000. << "sec elapsed\n";
    if(gDebug) {
        cout << "Analysis depth:\n";
        for(int i=0; i<65; i++)
            if(histo[i])
                cout << i-base << ": " << histo[i] << "\n";
    }
    return bestMove;
}
