#include <stdlib.h>
#include <stdint.h>
#include <iostream>
using namespace std;

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
static uint64_t fromStr(string str) {
    int r=-1, c=-1;
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

string toStr(uint64_t m) {
    string s = "";
    for(int r=0; r<8; r++)
        for(int c=0; c<8; c++, m>>=1)
            if(m & 1ULL) {
                char buf[4] = {'a'+c, '1'+r, ' ', 0};
                s = s + string(buf);
            }
    return s;
}


class Board {
public:
    uint64_t x, o;
    //Board(const Board &src): x(src.x), o(src.o) {};
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
    uint64_t possibleMoves(bool xMove) {
        return possibleMoves( xMove ? x : o,
                              xMove ? o : x );
    }

    Board move(bool xMove, uint64_t m) {
        Board ret(*this);
        uint64_t &me = xMove ? ret.x : ret.o;
        uint64_t &he = xMove ? ret.o : ret.x;
        uint64_t cap = captures(m, me, he);
        me |= cap;
        he &= ~cap;
        return ret;
    }
    Board move(bool xMove, int r, int c) {
        return move(xMove, fromRC(r, c));
    }
    Board move(bool xMove, const string str) {
        return move(xMove, fromStr(str));
    }
};

// Random for now
uint64_t ai(Board b, bool xMove) {
    uint64_t ops = b.possibleMoves(xMove);
    uint64_t choices[64]; int n = 0;
    for(uint64_t i=1; i; i<<=1)
        if(ops & i)
            choices[n++] = i;
    return choices[random()%n];
}

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

int main() {
    Board b;
    char *env = getenv("AI"); // Execute: AI=XY ./a.out
    bool xMove = true,
        aiX = env && strchr(env,'X'),
        aiO = env && strchr(env,'O');
    srandomdev();

    for(bool done=false, xMove=true; ; xMove = !xMove) {
        uint64_t ops = b.possibleMoves(xMove);
        if(ops)
            for(;;) {
                cout << Board(b.x | ops, b.o | ops)
                     << (xMove?"X: ":"O: ")/* << toStr(ops) << ": "*/;
                uint64_t m;
                if(0 == (ops & (ops-1))) {
                    cout << "Forced move\n";
                    m = ops;
                } else if(xMove ? aiX : aiO) {
                    m = ai(b, xMove);
                    cout << toStr(m) << "\n";
                } else {
                    string str;
                    cin >> str;
                    m = fromStr(str);
                }
                if(m & ops) {
                    done = false;
                    b = b.move(xMove, m);
                    break;
                }
            }
        else {
            cout << (xMove?"X can't move\n":"O can't move\n");
            if(done)  break;
            else      done = true;
        }
    }
    int x=0, o=0;
    for(uint64_t m=1; m; m<<=1) {
        if(b.x & m) x++;
        if(b.o & m) o++;
    }
    cout << b << "Game over. X:" << x << " O:" << o << "\n";
    return 0;
}
