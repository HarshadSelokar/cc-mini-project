// Propagation must merge information from both branches of an if.
let n = 0;
while (n < 2) {
    let a = 1;
    let b = 1;
    if (n == 0) {
        a = 2;
        b = 3;
    } else {
        a = 2;
        b = 4;
    }
    print(a);      // a == 2 on both paths => constant
    print(b);      // differs => must not be propagated
    n = n + 1;
}
