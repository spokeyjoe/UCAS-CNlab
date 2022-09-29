#include <stdio.h>

int main() {
    unsigned long long l1 = 5654587888789624569;
    unsigned long long l2 = 5454587888789624569;
    printf("%lld\n", (long long)(l1 - l2));
    printf("%lld\n", (long long)(l2 - l1));
}