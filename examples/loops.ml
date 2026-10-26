// loops.ml -- while loops
let x = 3;

while (x > 0) {
    print(x);
    x = x - 1;
}

// Sum of 1..10
let i = 1;
let sum = 0;
while (i <= 10) {
    sum = sum + i;
    i = i + 1;
}
print(sum);            // 55

// Nested loops: count pairs (r, c) with 1 <= c <= r <= 4
let r = 1;
let pairs = 0;
while (r <= 4) {
    let c = 1;
    while (c <= r) {
        pairs = pairs + 1;
        c = c + 1;
    }
    r = r + 1;
}
print(pairs);          // 10
