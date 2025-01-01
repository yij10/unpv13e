#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    char *c = "hi\n";
    printf("%s", c);
    char *a = strtok(c, " ");
    printf("%s", a);
}