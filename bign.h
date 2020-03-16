#ifndef HW2_BIGN_H
#define HW2_BIGN_H

/* 128 bits integer */
struct bign128 {
    unsigned long long lower, upper;
};

void add_bign128(struct bign128 *output, struct bign128 *x, struct bign128 *y);

#endif /* HW2_BIGN_H */