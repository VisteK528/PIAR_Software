

#ifndef NTAG213_H
#define NTAG213_H

#include <stdint.h>

#define NTAG213_UID_LENGTH_BYTES    7
#define NTAG213_MAX_PAGES           39
#define NTAG213_BYES_PER_PAGE       4

typedef struct {
    uint8_t uid[NTAG213_UID_LENGTH_BYTES];
    uint8_t data[NTAG213_BYES_PER_PAGE * NTAG213_MAX_PAGES];
} Ntag213_t;


#endif //NTAG213_H
