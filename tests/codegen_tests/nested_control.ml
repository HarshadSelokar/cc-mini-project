let i = 1;
while (i <= 3) {
    let j = 1;
    while (j <= 3) {
        if (i == j) {
            print(i * 10 + j);
        } else if (i < j) {
            print(0);
        } else {
            print(-1);
        }
        j = j + 1;
    }
    i = i + 1;
}
