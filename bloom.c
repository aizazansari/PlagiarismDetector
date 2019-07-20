/*FINAL 2*/

/***********************************************************
Implementation of bloom filter goes here
**********************************************************/

#include "bloom.h"

/* Constants for bloom filter implementation */
const int H1PRIME = 4189793;
const int H2PRIME = 3296731;
const int BLOOM_HASH_NUM = 10;

/* The hash function used by the bloom filter */
int hash_i(int i, /* which of the BLOOM_HASH_NUM hashes to use */
    long long x /* a long long value to be hashed */)
{
    return ((x % H1PRIME) + i * (x % H2PRIME) + 1 + i * i);
}

/* Initialize a bloom filter by allocating a character array that can pack bsz bits.
(each char represents 8 bits)
Furthermore, clear all bits for the allocated character array.
Hint: use the malloc and bzero library function
Return value is the newly initialized bloom_filter struct.*/
bloom_filter
bloom_init(int bsz /* size of bitmap to allocate in bits*/)
{
    bloom_filter f;
    f.bsz = bsz; /*allocating a size*/

    /* your code here*/
    int size = bsz / 8;
    if (bsz % 8 != 0) {
        size++; /*calculating the number of bytes required for the character array in bloom filter*/
    }
    f.buf = malloc(size); /*dynamic array allocation*/
    bzero(f.buf, size); /*setting all bits of array to zero*/
    return f;
}

/* Add elm into the given bloom filter*/
void bloom_add(bloom_filter f,
    long long elm /* the element to be added (a RK hash value) */)
{
    int i;
    for (i = 0; i < BLOOM_HASH_NUM; i++) {
        int position;
        position = (hash_i(i, elm)) % f.bsz; /*applying BLOOM_HASH_NUM of hashing algorithms to RK hash value and resizing it to fit the bsz bitmap*/
        f.buf[position / 8] = f.buf[position / 8] | (1 << (7 - position % 8)); /*turning on the bit position calculated in the previous line*/
    }
    return;
}

/* Query if elm is probably in the given bloom filter */
int bloom_query(bloom_filter f,
    long long elm /* the query element */)
{
    int i;
    for (i = 0; i < BLOOM_HASH_NUM; i++) {
        int position = (hash_i(i, elm)) % f.bsz; /*calculates position of bit by hashing the query element BLOOM_HASH_NUM of times*/
        unsigned int flag = 1;
        flag = flag << (7 - position % 8);
        if (!((f.buf[position / 8]) & flag)) { /*performs AND bitwise operation with flag and if it returns zero then function returns zero*/
            return 0; 
        }
    }
    return 1; /*means there is a possibility that the query string might exist in the document string*/
}

void bloom_free(bloom_filter* f)
{
    free(f->buf);
    f->buf = f->bsz = 0;
}

/* print out the first count bits in the bloom filter */
void bloom_print(bloom_filter f,
    int count /* number of bits to display*/)
{
    int i;

    assert(count % 8 == 0);

    for (i = 0; i < (f.bsz >> 3) && i < (count >> 3); i++) {
        printf("%02x ", (unsigned char)(f.buf[i]));
    }
    printf("\n");
    return;
}
