// factorial.ml -- iterative factorial of 1..10
let n = 1;
let fact = 1;
while (n <= 10) {
    fact = fact * n;
    print(fact);
    n = n + 1;
}
