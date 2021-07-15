#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern  "C" {
#endif

// return len of encoded base64 str
size_t b64_encode(const char *src, size_t srclength, char *target, size_t targsize);

// return len of decoded base64 str
size_t b64_decode(const char *src, uint8_t *target, size_t targsize);

size_t b64_output_encode_len(size_t input_length);
size_t b64_output_decode_len(size_t input_length);

#ifdef __cplusplus
}
#endif
