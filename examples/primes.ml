// primes.ml -- prime numbers below 50 (trial division)
let n = 2;
while (n < 50) {
    let isPrime = true;
    let d = 2;
    while (d * d <= n && isPrime) {
        if (n % d == 0) {
            isPrime = false;
        }
        d = d + 1;
    }
    if (isPrime) {
        print(n);
    }
    n = n + 1;
}
