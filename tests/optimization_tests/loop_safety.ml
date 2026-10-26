// Variables changed inside a loop must not be treated as constants.
let i = 0;
let total = 0;
let step = 2;
while (i < 5) {
    total = total + step;
    i = i + 1;
}
print(total);      // 10
print(i);          // 5
print(step);       // step never changes => propagated as 2
