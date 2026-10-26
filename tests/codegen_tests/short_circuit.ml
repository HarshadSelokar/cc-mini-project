// The right operand must not run when the left decides the result:
// otherwise 10 / d would divide by zero.
let d = 0;
let n = 0;
while (n < 2) {
    if (d != 0 && 10 / d > 1) {
        print(1);
    } else {
        print(2);
    }
    if (d == 0 || 10 / d > 1) {
        print(3);
    }
    n = n + 1;
}
