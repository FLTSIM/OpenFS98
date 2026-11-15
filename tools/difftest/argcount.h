/* A scenario count must be a positive number: "N=0" or "N=2OOOO" must not run nothing and pass. */
#ifndef ARGCOUNT_H
#define ARGCOUNT_H
#include <stdio.h>
#include <stdlib.h>
static unsigned long arg_count(const char *text)
{
    char *end;
    unsigned long n = strtoul(text, &end, 0);
    if (n == 0 || *end) {
        fprintf(stderr, "'%s' is not a positive count\n", text);
        exit(2);
    }
    return n;
}
#endif
