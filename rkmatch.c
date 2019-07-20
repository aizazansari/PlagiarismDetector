
/*FINAL*/

/* Match every k-character snippet of the query_doc document
among a collection of documents doc1, doc2, ....

./rkmatch snippet_size query_doc doc1 [doc2...]

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <assert.h>
#include <time.h>

#include "bloom.h"

enum algotype { SIMPLE = 0,
    RK,
    RKBATCH };

/* a large prime for RK hash (BIG_PRIME*256 does not overflow)*/
long long BIG_PRIME = 5003943032159437;

/* constants used for printing debug information */
const int PRINT_RK_HASH = 5;
const int PRINT_BLOOM_BITS = 160;

/* modulo addition */
long long
madd(long long a, long long b)
{
    return ((a + b) > BIG_PRIME ? (a + b - BIG_PRIME) : (a + b));
}

/* modulo substraction */
long long
mdel(long long a, long long b)
{
    return ((a > b) ? (a - b) : (a + BIG_PRIME - b));
}

/* modulo multiplication*/
long long
mmul(long long a, long long b)
{
    return ((a * b) % BIG_PRIME);
}

/* read the entire content of the file 'fname' into a
character array allocated by this procedure.
Upon return, *doc contains the address of the character array
*doc_len contains the length of the array
*/
void read_file(const char* fname, char** doc, int* doc_len)
{
    struct stat st;
    int fd;
    int n = 0;

    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        perror("read_file: open ");
        exit(1);
    }

    if (fstat(fd, &st) != 0) {
        perror("read_file: fstat ");
        exit(1);
    }

    *doc = (char*)malloc(st.st_size);
    if (!(*doc)) {
        fprintf(stderr, " failed to allocate %d bytes. No memory\n", (int)st.st_size);
        exit(1);
    }

    n = read(fd, *doc, st.st_size);
    if (n < 0) {
        perror("read_file: read ");
        exit(1);
    }
    else if (n != st.st_size) {
        fprintf(stderr, "read_file: short read!\n");
        exit(1);
    }

    close(fd);
    *doc_len = n;
}

/* The normalize procedure examines a character array of size len
in ONE PASS and does the following:
1) turn all upper case letters into lower case ones
2) turn any white-space character into a space character and,
shrink any n>1 consecutive spaces into exactly 1 space only
Hint: use C library function isspace()
You must do the normalization IN PLACE so that when the procedure
returns, the character array buf contains the normalized string and
the return value is the length of the normalized string.
*/

int normalize(char* buf, /* The character array containing the string to be normalized*/
    int len /* the size of the original character array */)
{
    int ir = 0; /*reading counter*/
    int iw = 0; /*writing counter*/
    int beg = 1; /*true if ir at a space in the beginning of document*/
    for (ir = 0; ir < len; ir++) {
        buf[ir] = tolower(buf[ir]); /*convert to lowercase*/
        if (beg && isspace(buf[ir])) { /*skip to next iteration if at space at beginning*/
            continue;
        }
        beg = 0; /*becomes false when first character encountered*/
        if (isspace(buf[ir]) && isspace(buf[ir - 1])) { /*skip to next iteration if a space is encountered after a consecutive space*/
            continue;
        }

        if (isspace(buf[ir])) { /*convert all blank spaces to single space*/
            buf[ir] = ' ';
        }

        buf[iw] = buf[ir]; /* otherwise write using iw if its a character or the first encountered space in a row*/
        iw++;
    }
    if (isspace(buf[iw - 1])) { /*remove spaces at the end*/
        iw--;
    }
    return iw; /*return new length*/
}

/* check if a query string ps (of length k) appears
in ts (of length n) as a substring
If so, return 1. Else return 0
You may want to use the library function strncmp
*/
int simple_match(const char* ps, /* the query string */
    int k, /* the length of the query string */
    const char* ts, /* the document string (Y) */
    int n /* the length of the document Y */)
{
    int i;
    for (i = 0; i < n - k + 1; i++) {
        if (strncmp(ps, ts + i, k) == 0) { /* string function which compares the query string chunk to subsequent substrings of document string Y */
            return 1;
        }
    }
    return 0;
}

/* Check if a query string ps (of length k) appears
in ts (of length n) as a substring using the rabin-karp algorithm
If so, return 1. Else return 0
In addition, print the first 'PRINT_RK_HASH' hash values of ts
Example:
$ ./rkmatch -t 1 -k 20 X Y
605818861882592 812687061542252 1113263531943837 1168659952685767 4992125708617222
0.01 matched: 1 out of 148
*/

long long rtopower(int power) /*power function used for hashing of long long values in RK match*/
{
    int a;
    if (power == 0) {
        return 1;
    }
    if (power == 1) {
        return 256;
    }
    long long number = 1;
    for (a = 1; a <= power; a++) {
        number = mmul(number, 256);
    }
    return number;
}

int rabin_karp_match(const char* ps, /* the query string */
    int k, /* the length of the query string */
    const char* ts, /* the document string (Y) */
    int n /* the length of the document Y */)
{
    long long hashq = 0; /*query string hash*/
    long long hashd = 0; /*document string hash*/
    
    /*compute hash for query string and the first substring of document Y*/
    int i;
    for (i = 0; i <= k - 1; i++) {
        hashq = madd(hashq, mmul(rtopower(k - 1 - i), (long long)ps[i]));
        hashd = madd(hashd, mmul(rtopower(k - 1 - i), (long long)ts[i]));
    }

    printf("%lld ", hashd); /*print first document hash with space at the end to distinguish from other hashes*/
    if (hashq == hashd) { 
        if (simple_match(ps, k, ts, k)) {
            printf("\n"); /*if hash of document substring and query string equal and they are also equal in terms of characters then print line break and return*/
            return 1;
        }
    }

    /*compute hash for subsequent substrings of document Y*/
    int j;
    for (j = 1; j < n - k + 1; j++) {
        /*computing new hash of document substring using rolling hash formula and then checking against query string again by first hashing and then comparing character values*/
        hashd = madd(mmul(mdel(hashd, mmul(rtopower(k - 1), (long long)ts[j - 1])), 256), ts[j + k - 1]);

        if (j < PRINT_RK_HASH) {
            printf("%lld ", hashd);
        }
        if (hashq == hashd) {
            if (simple_match(ps, k, ts + j, k)) {
                printf("\n");
                return 1;
            }
        }
    }
    printf("\n"); /*printing line break before every return to distinguish a set of hash values (max value of which is PRINT_RK_HASH) 
    from each other */
    return 0;
}

/* Initialize the bitmap for the bloom filter using bloom_init().
Insert all m/k RK hashes of qs into the bloom filter using bloom_add().
Then, compute each of the n-k+1 RK hashes of ts and check if it's in the filter using bloom_query().
Use the given procedure, hash_i(i, p), to compute the i-th bloom filter hash value for the RK value p.

Return the number of matched chunks.
Additionally, print out the first PRINT_BLOOM_BITS of the bloom filter using the given bloom_print
after inserting m/k substrings from qs.
*/
int rabin_karp_batchmatch(int bsz, /* size of bitmap (in bits) to be used */
    int k, /* chunk length to be matched */
    const char* qs, /* query docoument (X)*/
    int m, /* query document length */
    const char* ts, /* to-be-matched document (Y) */
    int n /* to-be-matched document length*/)
{
    bloom_filter bf = bloom_init(bsz); /*initializing the bloom filter*/
    int i, j;
    long long hashq, hashd;

    for (i = 0; i < m / k; i++) {
        hashq = 0;
        for (j = 0; j < k; j++) {
            hashq = madd(mmul(rtopower(k - j - 1), (long long)(qs[(i * k) + j])), hashq);
        }
        bloom_add(bf, hashq); /*hash value of every chunk of query document computed and added to the bitmap using bloom_add*/
    }

    bloom_print(bf, PRINT_BLOOM_BITS);

    hashd = 0;
    for (i = 0; i < k; i++) {
        hashd = madd(mmul(rtopower(k - i - 1), (long long)ts[i]), hashd); /*first hash value of document Y computed*/
    }

    int match_count = 0;
    for (i = 0; i < n - k + 1; i++) {
        if (bloom_query(bf, hashd)) { 
            for (j = 0; j < m / k; j++) {
                if (simple_match(&qs[j * k], k, &ts[i], k)) {
                    match_count++; /*checking the possibility of the substring of Y existing in X and if bloom_query returns true for this 
                    existence then running simple match to compare Y's substring to chunks in X*/
                }
            }
        }

        hashd = madd(mmul(256, mdel(hashd, mmul(rtopower(k - 1), (long long)ts[i]))), ts[i + k]); /*computing the next hash value as a rolling hash*/
    }

    return match_count; /*returning the number of matches*/
}

int main(int argc, char** argv)
{
    int k = 100; /* default match size is 100*/
    int which_algo = SIMPLE; /* default match algorithm is simple */

    char *qdoc, *doc;
    int qdoc_len, doc_len;
    int i;
    int num_matched = 0;
    int to_be_matched;
    int c;

    /* Refuse to run on platform with a different size for long long*/
    assert(sizeof(long long) == 8);

    /*getopt is a C library function to parse command line options */
    while ((c = getopt(argc, argv, "t:k:q:")) != -1) {
        switch (c) {
        case 't':
            /*optarg is a global variable set by getopt()
it now points to the text following the '-t' */
            which_algo = atoi(optarg);
            break;
        case 'k':
            k = atoi(optarg);
            break;
        case 'q':
            BIG_PRIME = atoi(optarg);
            break;
        default:
            fprintf(stderr,
                "Valid options are: -t <algo type> -k <match size> -q <prime modulus>\n");
            exit(1);
        }
    }

    /* optind is a global variable set by getopt()
it now contains the index of the first argv-element
that is not an option*/
    if (argc - optind < 1) {
        printf("Usage: ./rkmatch query_doc doc\n");
        exit(1);
    }

    /* argv[optind] contains the query_doc argument */
    read_file(argv[optind], &qdoc, &qdoc_len);
    qdoc_len = normalize(qdoc, qdoc_len);

    /* argv[optind+1] contains the doc argument */
    read_file(argv[optind + 1], &doc, &doc_len);
    doc_len = normalize(doc, doc_len);

    switch (which_algo) {
    case SIMPLE:
        /* for each of the qdoc_len/k chunks of qdoc,
check if it appears in doc as a substring*/
        for (i = 0; (i + k) <= qdoc_len; i += k) {
            if (simple_match(qdoc + i, k, doc, doc_len)) {
                num_matched++;
            }
        }
        break;
    case RK:
        /* for each of the qdoc_len/k chunks of qdoc,
check if it appears in doc as a substring using
the rabin-karp substring matching algorithm */
        for (i = 0; (i + k) <= qdoc_len; i += k) {
            if (rabin_karp_match(qdoc + i, k, doc, doc_len)) {
                num_matched++;
            }
        }
        break;
    case RKBATCH:
        /* match all qdoc_len/k chunks simultaneously (in batch) by using a bloom filter*/
        num_matched = rabin_karp_batchmatch(((qdoc_len * 10 / k) >> 3) << 3, k, qdoc, qdoc_len, doc, doc_len);
        break;
    default:
        fprintf(stderr, "Wrong algorithm type, choose from 0 1 2\n");
        exit(1);
    }

    to_be_matched = qdoc_len / k;
    printf("%.2f matched: %d out of %d\n", (double)num_matched / to_be_matched,
        num_matched, to_be_matched);

    free(qdoc);
    free(doc);

    return 0;
}

