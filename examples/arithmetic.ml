// arithmetic.ml -- operators, precedence and associativity
let a = 17;
let b = 5;

print(a + b);        // 22
print(a - b);        // 12
print(a * b);        // 85
print(a / b);        // 3   (integer division)
print(a % b);        // 2
print(-a + b);       // -12 (unary minus)

print(2 + 3 * 4);    // 14  : * binds tighter than +
print((2 + 3) * 4);  // 20  : parentheses override precedence
print(20 - 5 - 3);   // 12  : left associative (20 - 5) - 3
print(100 / 10 / 2); // 5   : left associative (100 / 10) / 2

let big = a * b + a / b - a % b;
print(big);          // 85 + 3 - 2 = 86
