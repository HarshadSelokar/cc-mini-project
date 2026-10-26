// fibonacci.ml -- first 15 Fibonacci numbers
let a = 0;
let b = 1;
let count = 0;
while (count < 15) {
    print(a);
    let next = a + b;
    a = b;
    b = next;
    count = count + 1;
}
