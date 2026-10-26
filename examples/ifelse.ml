// ifelse.ml -- relational and logical operators, if / else / else-if
let x = 10;

if (x > 5) {
    print(x);
} else {
    print(0);
}

let score = 72;
if (score >= 90) {
    print(1);
} else if (score >= 70) {
    print(2);
} else {
    print(3);
}

let a = 4;
let b = 9;
let inRange = a > 0 && b < 10;
print(inRange);                 // true
print(a == b || !(a != 4));     // true
print(a >= b);                  // false

// Short-circuit evaluation: b / a is only computed when a != 0.
if (a != 0 && b / a == 2) {
    print(b / a);
}
