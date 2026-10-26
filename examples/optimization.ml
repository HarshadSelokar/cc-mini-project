// optimization.ml -- shows every optimization of the AST optimizer.
// Compare:  minilang examples/optimization.ml --optimized-ast

// Constant folding: 10 + 20 * 2 is computed at compile time.
let x = 10 + 20 * 2;
print(x);                       // 50

// Constant propagation: a is known to be 10, so b = 15.
let a = 10;
let b = a + 5;
print(b);                       // 15

// Algebraic simplification on a value unknown at compile time.
let n = 0;
while (n < 3) {
    let y = n * 1 + 0;          // => n
    let z = (y - 0) * 0;        // => 0
    print(y + z);
    n = n + 1;
}

// Dead code elimination: the condition is always false.
if (false) {
    print(999);
}

// The condition folds to true, so only the 'then' branch remains.
let debug = 2 > 1;
if (debug && true) {
    print(x * 2);               // 100
} else {
    print(-1);
}

// A loop that never runs is removed.
while (a > 100) {
    print(a);
}
