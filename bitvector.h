#ifndef _BIT_VECTOR_H
#define _BIT_VECTOR_H

#include <limits.h>

typedef int chunk;
#define BITS_IN_CHUNK   (sizeof(chunk)*CHAR_BIT)


typedef struct BitVector
{
  int    numbits;         /* total number of bits in "bits" */
  int    next_free;       /* index of the next free bit */
  int    is_full;
  chunk *bits;            /* the actual bitmap */
} BitVector;

/* prototypes */
int  SetBV(BitVector *bv, int which);
int  UnSetBV(BitVector *bv, int which);
int  UnSetRangeBV(BitVector *bv, int lo, int hi);
int  IsSetBV(BitVector *bv, int which);
int  GetFreeRangeOfBits(BitVector *bv, int len, int *biggest_free_chunk);

#endif /* _BIT_VECTOR_H */
