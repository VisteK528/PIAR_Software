

#ifndef API_REQUESTS_H
#define API_REQUESTS_H
#include <stdint.h>

void make_api_request(void);

void post_tag_record(uint8_t* tag, int milliliters);

#endif //API_REQUESTS_H
