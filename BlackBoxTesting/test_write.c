#include <stdio.h>

int main() {
    FILE *fp = fopen("test_output.txt", "w");
    if (fp == NULL) {
        perror("Failed to open file");
        return 1;
    }

    int num = fprintf(fp, "Hello, world!\n");
    if (num < 0) {
        perror("Failed to write to file");
    } else {
        printf("Wrote %d characters\n", num);
    }

    fclose(fp);
    return 0;
}
