// scopes.ml -- block scoping and shadowing
let x = 1;
{
    let x = true;      // a new variable that shadows the outer x
    print(x);          // true
}
print(x);              // 1

let i = 0;
while (i < 2) {
    let x = i * 10;    // fresh variable on every iteration
    print(x);
    i = i + 1;
}
print(x);              // still 1
