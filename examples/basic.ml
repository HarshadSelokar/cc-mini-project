// basic.ml -- variables, assignment and print
let x = 10;
let y = 20;
print(x + y);

let z = x + y * 2;   // precedence: x + (y * 2)
if (z > 30) {
    print(z);
}

x = x + 5;
print(x);

let done = true;
print(done);
