#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
    // Default to CLI tool
    if (argc == 1) {
        printf("Starting repl\n");
    } else if (argc > 1) {
        if (strcmp(argv[1], "vis") == 0) {
            printf("starting visualization");
        }
    }
    return 0;
}
