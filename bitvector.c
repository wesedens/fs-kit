/*
  This file contains routines that operate on a bit vector to set
  and clear bits.  Currently the free block map and the inode map
  use this code to managed their respective entities.
  
  THIS CODE COPYRIGHT DOMINIC GIAMPAOLO.  NO WARRANTY IS EXPRESSED 
  OR IMPLIED.  YOU MAY USE THIS CODE AND FREELY DISTRIBUTE IT FOR
  NON-COMMERCIAL USE AS LONG AS THIS NOTICE REMAINS ATTACHED.

  FOR COMMERCIAL USE, CONTACT DOMINIC GIAMPAOLO (dbg@be.com).

  Dominic Giampaolo
  dbg@be.com
*/
#include "compat.h"
#include "bitvector.h"


/* Set a bit in a BitVector.
 *
 *   Inputs : bv is a valid bit vector pointer
 *            which indicates which bit specifically to set in bv
 *
 *   returns : TRUE if bit was set, FALSE otherwise... 
 */
int
SetBV(BitVector *bv, int which)
{
    int i,j;
    
    if (which >= bv->numbits || which < 0)
        return FALSE;
    
    i = which / BITS_IN_CHUNK;       /* i == index to bv->bits         */
    j = which % BITS_IN_CHUNK;       /* j == bit inside of bv->bits[i] */
    
    bv->bits[i] |= (1 << j);
    
    return TRUE;
}


/* UNSet a bit in a BitVector.
 *
 *   Inputs : bv is a valid bit vector pointer
 *            which indicates which bit specifically to unset in bv
 *
 *   returns : TRUE if bit was unset, FALSE otherwise... 
 */
int
UnSetBV(BitVector *bv, int which)
{
    int i,j;
    
    if (which >= bv->numbits || which < 0)
        return FALSE;
    
    i = which / BITS_IN_CHUNK;       /* i == index to bv->bits         */
    j = which % BITS_IN_CHUNK;       /* j == bit inside of bv->bits[i] */
    
    bv->bits[i]   &= ~(1 << j);
    bv->next_free  = which;
    
    return TRUE;
}


int
UnSetRangeBV(BitVector *bv, int start, int len)
{
    int i,j,k;
    
    if (start < 0 || len < 0 || start+len > bv->numbits)
        return FALSE;
    
    bv->next_free = start;
    
    for(k=start; k < start+len; k++) {
        i = k / BITS_IN_CHUNK; /* i == index to bv->bits         */
        j = k % BITS_IN_CHUNK; /* j == bit inside of bv->bits[i] */
     
        bv->bits[i] &= ~(1 << j);
    }     
    
    return TRUE;
}



/* Test if a bit is set in a bit vector... 
 *
 *  Inputs : bv, a valid bit vector pointer
 *           which, which bit to test 
 *
 *  Returns : true if the bit was set, false otherwise.
 */
int
IsSetBV(BitVector *bv, int which)
{
    int i,j;
    
    if (which >= bv->numbits || which < 0)
        return FALSE;
    
    i = which / BITS_IN_CHUNK;
    j = which % BITS_IN_CHUNK;
    
    return (int)(bv->bits[i] & (1 << j));
}


/* Get the first range of free bits that is len bits long
 *
 *   Inputs : bm is a valid bitmap pointer
 *            len is the number of bits needed in the range 
 *
 *   returns: the number of the first free bit or -1 on failure
 *
 */
int
GetFreeRangeOfBits(BitVector *bv, int len, int *biggest_free_chunk)
{
    int max_free = -1;
    int i,j,k,n, begin, base, real_count;
    int count, start, bit, non_zero_bits=0;
  
    if (biggest_free_chunk)
        *biggest_free_chunk = -1;

    if (len <= 0 || len > bv->numbits || bv->is_full)
        return -1;
    
    count = bv->numbits / BITS_IN_CHUNK;
    if ((bv->numbits % BITS_IN_CHUNK) != 0)
        count++;
    
    base = bv->next_free / BITS_IN_CHUNK;
  
    for(i=0, real_count=0; i < count; i++, real_count++) {
        if (real_count > count)
            panic("bitmap loop! count %d, real_count %d, i %d bv @ 0x%lx\n",
                  count, real_count, i, (ulong)bv);

        begin = (base + i) % count;

        if (bv->bits[begin] == (chunk)(~0)) {
            non_zero_bits += sizeof(chunk) * CHAR_BIT;
            continue;
        }
     
        for(j=0; j < BITS_IN_CHUNK; j++) {
            if ((bv->bits[begin] & (1 << j)) != 0) {
                non_zero_bits++;
            } else {                                   /* aha! a free bit */
                start = (begin * BITS_IN_CHUNK) + j; 
                bit = j;  n = begin;
       
                /*
                 * Here we start to see if we can get a range of bits,
                 * being careful if we cross chunk boundaries.
                 */
                for(k=0; k < len && (n*BITS_IN_CHUNK+bit) < bv->numbits; k++,bit++) {
                    if (((bv->bits[n]) & (1 << bit)) != 0)
                        break;
          
                    if ((bit+1) == BITS_IN_CHUNK) {
                        n++;
                        bit = -1;  /* gets incremented on loop continuation */
                    }
                }

                if (k > max_free) {
                    max_free = k;
                }

                if ((n*BITS_IN_CHUNK+bit) >= bv->numbits && k < len) {
                    /* i += (n - begin - 1); */
                    if (n > begin)
                        i += (n - begin);
                    else
                        i += 1;
                    break;
                }

                if (k < len) { /* didn't find a big enough range here */
                    i += (n - begin); /* advance over what we just checked */
                    begin = n;
                    j = bit;
                    continue;
                }

                if ((begin*BITS_IN_CHUNK+j + len) > bv->numbits) 
                    break;      /* too big for the # of bits */

                /*
                 * Now go back through and set all the bits
                 */
                bit = j; n = begin;
                for(k=0; k < len; k++, bit++) {
                    bv->bits[n] |= (1 << bit);
          
                    if ((bit+1) == BITS_IN_CHUNK) {
                        n++;
                        bit = -1;  /* gets incremented on loop continuation */
                    }
                }

                if (n < count) {
                    if ((bv->bits[n] & (1 << bit)) == 0)
                        bv->next_free = start + len;
                }

                return (start);
            }  /* end of if (...) aha! a free bit */
            
        }  /* end of for(j=0; ...) (loop on each bit) */
        
    }  /* end of for(i=0; ...) (loop on each chunk) */


    if (non_zero_bits >= bv->numbits)  /* then mark this guy as full */
        bv->is_full = 1;

    if (biggest_free_chunk)
        *biggest_free_chunk = max_free;


    /* if we get here, we didn't find nuthin. */
    return -1;
}

