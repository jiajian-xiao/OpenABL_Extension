typedef double abl_float;

typedef struct {
    int x;
    abl_float y;
} flo2;

static inline flo2 flo2_create(abl_float x, abl_float y) {
    return (flo2) { x, y };
}
static inline flo2 flo2_fill(abl_float x) {
    return (flo2) { x, x };
}
static inline flo2 flo2_add(flo2 a, flo2 b) {
    return (flo2) { a.x + b.x, a.y + b.y };
}
static inline flo2 flo2_sub(flo2 a, flo2 b) {
    return (flo2) { a.x - b.x, a.y - b.y };
}
static inline flo2 flo2_mul_scalar(flo2 a, abl_float s) {
    return (flo2) { a.x * s, a.y * s };
}
static inline flo2 flo2_div_scalar(flo2 a, abl_float s) {
    return (flo2) { a.x / s, a.y / s };
}
static inline bool flo2_equals(flo2 a, flo2 b) {
    return a.x == b.x && a.y == b.y;
}
static inline bool flo2_not_equals(flo2 a, flo2 b) {
    return a.x != b.x || a.y != b.y;
}

typedef struct {
    abl_float x;
    abl_float y;
    abl_float z;
} flo3;

static inline flo3 flo3_create(abl_float x, abl_float y, abl_float z) {
    return (flo3) { x, y, z };
}
static inline flo3 flo3_fill(abl_float x) {
    return (flo3) { x, x, x };
}
static inline flo3 flo3_add(flo3 a, flo3 b) {
    return (flo3) { a.x + b.x, a.y + b.y, a.z + b.z };
}
static inline flo3 flo3_sub(flo3 a, flo3 b) {
    return (flo3) { a.x - b.x, a.y - b.y, a.z - b.z };
}
static inline flo3 flo3_mul_scalar(flo3 a, abl_float s) {
    return (flo3) { a.x * s, a.y * s, a.z * s };
}
static inline flo3 flo3_div_scalar(flo3 a, abl_float s) {
    return (flo3) { a.x / s, a.y / s, a.z / s };
}
static inline bool flo3_equals(flo3 a, flo3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
static inline bool flo3_not_equals(flo3 a, flo3 b) {
    return a.x != b.x || a.y != b.y || a.z != b.z;
}

/*
 * Lengths and distances
 */

static inline abl_float dot_flo2(flo2 a, flo2 b) {
    return a.x * b.x + a.y * b.y;
}
static inline abl_float dot_flo3(flo3 a, flo3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline abl_float length_flo2(flo2 v) {
    return sqrt(v.x * v.x + v.y * v.y);
}
static inline abl_float length_flo3(flo3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline abl_float dist_flo2(flo2 a, flo2 b) {
    return length_flo2(flo2_sub(a, b));
}
static inline abl_float dist_flo3(flo3 a, flo3 b) {
    return length_flo3(flo3_sub(a, b));
}

static inline flo2 normalize_flo2(flo2 v) {
    return flo2_div_scalar(v, length_flo2(v));
}
static inline flo3 normalize_flo3(flo3 v) {
    return flo3_div_scalar(v, length_flo3(v));
}
ulong xorshift128plus() {
    ulong xorshift_state[2] = { 0xdeadbeef, 0xbeefdead };
    ulong x = xorshift_state[0];
    ulong const y = xorshift_state[1];
    xorshift_state[0] = y;
    x ^= x << 23; // a
    xorshift_state[1] = x ^ y ^ (x >> 17) ^ (y >> 26); // b, c
    return xorshift_state[1] + y;
}
abl_float MWC64X(__global uint2 *state)
{
    enum { A=4294883355U};
    uint x=(*state).x, c=(*state).y;  // Unpack the state
    uint res=x^c;                     // Calculate the result
    uint hi=mul_hi(x,A);              // Step the RNG
    x=x*A+c;
    c=hi+(x<c);
    *state=(uint2)(x,c);              // Pack the state back up
    return (abl_float)res / UINT_MAX;     // Return the next result
}
abl_float random_float(abl_float min, abl_float max) {
    ulong x = xorshift128plus();
    abl_float rx = MWC64X(&x);
    // This is a horrible way of generating a random float.
    // It will do for now.
    return min + (abl_float) rx / (abl_float) ( (max - min));
}

int random_int(int min, int max) {
    unsigned n = max-min+1;
    if ((n & (n-1)) == 0) {
        return xorshift128plus() & (n - 1);
    }

    // Not the fastest way to do this
    unsigned r = UINT_MAX % n;
    unsigned x;
    do {
        x = xorshift128plus();
    } while (x >= UINT_MAX - r);
    return min + x % n;
}

