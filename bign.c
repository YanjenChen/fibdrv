#include "bign.h"

void add_bign128(struct bign128 *output, struct bign128 *x, struct bign128 *y)
{
    output->upper = x->upper + y->upper;
    if (y->lower > ~(x->lower))
        output->upper++;
    output->lower = x->lower + y->lower;
}