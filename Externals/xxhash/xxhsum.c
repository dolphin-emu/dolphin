/*
bench.c - Demo program to benchmark open-source algorithm
Copyright (C) Yann Collet 2012-2014

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

You can contact the author at :
- Blog homepage : http://fastcompression.blogspot.com/
- Discussion group : https://groups.google.com/forum/?fromgroups#!forum/lz4c
*/

/**************************************
 * Compiler Options
 *************************************/
/* MS Visual */
#if defined(_MSC_VER) || defined(_WIN32)
#  define _CRT_SECURE_NO_WARNINGS   /* removes visual warnings */
#  define BMK_LEGACY_TIMER 1        /* gettimeofday() not supported by MSVC */
#endif

/* Under Linux at least, pull in the *64 commands */
#define _LARGEFILE64_SOURCE


/**************************************
 * Includes
 *************************************/
#include <stdlib.h>     // malloc
#include <stdio.h>      // fprintf, fopen, ftello64, fread, stdin, stdout; when present : _fileno
#include <string.h>     // strcmp
#include <sys/types.h>  // stat64
#include <sys/stat.h>   // stat64

#include "xxhash.h"


/**************************************
 * OS-Specific Includes
 *************************************/
// Use ftime() if gettimeofday() is not available on your target
#if defined(BMK_LEGACY_TIMER)
#  include <sys/timeb.h>   // timeb, ftime
#else
#  include <sys/time.h>    // gettimeofday
#endif

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>    // _O_BINARY
#  include <io.h>       // _setmode, _isatty
#  ifdef __MINGW32__
   int _fileno(FILE *stream);   // MINGW somehow forgets to include this windows declaration into <stdio.h>
#  endif
#  define SET_BINARY_MODE(file) _setmode(_fileno(file), _O_BINARY)
#  define IS_CONSOLE(stdStream) _isatty(_fileno(stdStream))
#else
#  include <unistd.h>   // isatty, STDIN_FILENO
#  define SET_BINARY_MODE(file)
#  define IS_CONSOLE(stdStream) isatty(STDIN_FILENO)
#endif

#if !defined(S_ISREG)
#  define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif


/**************************************
 * Basic Types
 *************************************/
#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   // C99
# include <stdint.h>
  typedef  uint8_t BYTE;
  typedef uint16_t U16;
  typedef uint32_t U32;
  typedef  int32_t S32;
  typedef uint64_t U64;
#else
  typedef unsigned char       BYTE;
  typedef unsigned short      U16;
  typedef unsigned int        U32;
  typedef   signed int        S32;
  typedef unsigned long long  U64;
#endif


/**************************************
 * Constants
 *************************************/
#define PROGRAM_NAME exename
#define PROGRAM_VERSION ""
#define COMPILED __DATE__
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s %i-bits %s, by %s (%s) ***\n", PROGRAM_NAME, (int)(sizeof(void*)*8), PROGRAM_VERSION, AUTHOR, COMPILED

#define NBLOOPS    3           // Default number of benchmark iterations
#define TIMELOOP   2500        // Minimum timing per iteration
#define PRIME 2654435761U

#define KB *(1<<10)
#define MB *(1<<20)
#define GB *(1U<<30)

#define MAX_MEM    (2 GB - 64 MB)

static const char stdinName[] = "-";


//**************************************
// Display macros
//**************************************
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYRESULT(...)   fprintf(stdout, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=l) DISPLAY(__VA_ARGS__);
static unsigned g_displayLevel = 1;


//**************************************
// Unit variables
//**************************************
static int g_nbIterations = NBLOOPS;
static int g_fn_selection = 1;    // required within main() & usage()


//*********************************************************
// Benchmark Functions
//*********************************************************

#if defined(BMK_LEGACY_TIMER)

static int BMK_GetMilliStart(void)
{
  // Based on Legacy ftime()
  // Rolls over every ~ 12.1 days (0x100000/24/60/60)
  // Use GetMilliSpan to correct for rollover
  struct timeb tb;
  int nCount;
  ftime( &tb );
  nCount = (int) (tb.millitm + (tb.time & 0xfffff) * 1000);
  return nCount;
}

#else

static int BMK_GetMilliStart(void)
{
  // Based on newer gettimeofday()
  // Use GetMilliSpan to correct for rollover
  struct timeval tv;
  int nCount;
  gettimeofday(&tv, NULL);
  nCount = (int) (tv.tv_usec/1000 + (tv.tv_sec & 0xfffff) * 1000);
  return nCount;
}

#endif

static int BMK_GetMilliSpan( int nTimeStart )
{
    int nSpan = BMK_GetMilliStart() - nTimeStart;
    if ( nSpan < 0 )
        nSpan += 0x100000 * 1000;
    return nSpan;
}


static size_t BMK_findMaxMem(U64 requestedMem)
{
    size_t step = (64 MB);
    size_t allocatedMemory;
    BYTE* testmem=NULL;

    requestedMem += 3*step;
    requestedMem -= (size_t)requestedMem & (step-1);
    if (requestedMem > MAX_MEM) requestedMem = MAX_MEM;
    allocatedMemory = (size_t)requestedMem;

    while (!testmem)
    {
        allocatedMemory -= step;
        testmem = (BYTE*) malloc((size_t)allocatedMemory);
    }
    free (testmem);

    return (size_t) (allocatedMemory - step);
}


static U64 BMK_GetFileSize(char* infilename)
{
    int r;
#if defined(_MSC_VER)
    struct _stat64 statbuf;
    r = _stat64(infilename, &statbuf);
#else
    struct stat statbuf;
    r = stat(infilename, &statbuf);
#endif
    if (r || !S_ISREG(statbuf.st_mode)) return 0;   // No good...
    return (U64)statbuf.st_size;
}


static int BMK_benchFile(char** fileNamesTable, int nbFiles)
{
    int fileIdx=0;
    U32 hashResult=0;

    U64 totals = 0;
    double totalc = 0.;


    // Loop for each file
    while (fileIdx<nbFiles)
    {
        FILE*  inFile;
        char*  inFileName;
        U64    inFileSize;
        size_t benchedSize;
        size_t readSize;
        char*  buffer;
        char*  alignedBuffer;

        // Check file existence
        inFileName = fileNamesTable[fileIdx++];
        inFile = fopen( inFileName, "rb" );
        if (inFile==NULL)
        {
            DISPLAY( "Pb opening %s\n", inFileName);
            return 11;
        }

        // Memory allocation & restrictions
        inFileSize = BMK_GetFileSize(inFileName);
        benchedSize = (size_t) BMK_findMaxMem(inFileSize);
        if ((U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
        if (benchedSize < inFileSize)
        {
            DISPLAY("Not enough memory for '%s' full size; testing %i MB only...\n", inFileName, (int)(benchedSize>>20));
        }

        buffer = (char*)malloc((size_t )benchedSize+16);
        if(!buffer)
        {
            DISPLAY("\nError: not enough memory!\n");
            fclose(inFile);
            return 12;
        }
        alignedBuffer = (buffer+15) - (((size_t)(buffer+15)) & 0xF);   // align on next 16 bytes boundaries

        // Fill input buffer
        DISPLAY("\rLoading %s...        \n", inFileName);
        readSize = fread(alignedBuffer, 1, benchedSize, inFile);
        fclose(inFile);

        if(readSize != benchedSize)
        {
            DISPLAY("\nError: problem reading file '%s' !!    \n", inFileName);
            free(buffer);
            return 13;
        }


        // Bench XXH32
        {
            int interationNb;
            double fastestC = 100000000.;

            DISPLAY("\r%79s\r", "");       // Clean display line
            for (interationNb = 1; interationNb <= g_nbIterations; interationNb++)
            {
                int nbHashes = 0;
                int milliTime;

                DISPLAY("%1i-%-14.14s : %10i ->\r", interationNb, "XXH32", (int)benchedSize);

                // Hash loop
                milliTime = BMK_GetMilliStart();
                while(BMK_GetMilliStart() == milliTime);
                milliTime = BMK_GetMilliStart();
                while(BMK_GetMilliSpan(milliTime) < TIMELOOP)
                {
                    int i;
                    for (i=0; i<100; i++)
                    {
                        hashResult = XXH32(alignedBuffer, benchedSize, 0);
                        nbHashes++;
                    }
                }
                milliTime = BMK_GetMilliSpan(milliTime);
                if ((double)milliTime < fastestC*nbHashes) fastestC = (double)milliTime/nbHashes;
                DISPLAY("%1i-%-14.14s : %10i -> %7.1f MB/s\r", interationNb, "XXH32", (int)benchedSize, (double)benchedSize / fastestC / 1000.);
            }
            DISPLAY("%-16.16s : %10i -> %7.1f MB/s   0x%08X\n", "XXH32", (int)benchedSize, (double)benchedSize / fastestC / 1000., hashResult);

            totals += benchedSize;
            totalc += fastestC;
        }

        // Bench Unaligned XXH32
        {
            int interationNb;
            double fastestC = 100000000.;

            DISPLAY("\r%79s\r", "");       // Clean display line
            for (interationNb = 1; (interationNb <= g_nbIterations) && ((benchedSize>1)); interationNb++)
            {
                int nbHashes = 0;
                int milliTime;

                DISPLAY("%1i-%-14.14s : %10i ->\r", interationNb, "(unaligned)", (int)benchedSize);
                // Hash loop
                milliTime = BMK_GetMilliStart();
                while(BMK_GetMilliStart() == milliTime);
                milliTime = BMK_GetMilliStart();
                while(BMK_GetMilliSpan(milliTime) < TIMELOOP)
                {
                    int i;
                    for (i=0; i<100; i++)
                    {
                        hashResult = XXH32(alignedBuffer+1, benchedSize-1, 0);
                        nbHashes++;
                    }
                }
                milliTime = BMK_GetMilliSpan(milliTime);
                if ((double)milliTime < fastestC*nbHashes) fastestC = (double)milliTime/nbHashes;
                DISPLAY("%1i-%-14.14s : %10i -> %7.1f MB/s\r", interationNb, "XXH32 (unaligned)", (int)(benchedSize-1), (double)(benchedSize-1) / fastestC / 1000.);
            }
            DISPLAY("%-16.16s : %10i -> %7.1f MB/s \n", "XXH32 (unaligned)", (int)benchedSize-1, (double)(benchedSize-1) / fastestC / 1000.);
        }

        // Bench XXH64
        {
            int interationNb;
            double fastestC = 100000000.;
            unsigned long long h64 = 0;

            DISPLAY("\r%79s\r", "");       // Clean display line
            for (interationNb = 1; interationNb <= g_nbIterations; interationNb++)
            {
                int nbHashes = 0;
                int milliTime;

                DISPLAY("%1i-%-14.14s : %10i ->\r", interationNb, "XXH64", (int)benchedSize);

                // Hash loop
                milliTime = BMK_GetMilliStart();
                while(BMK_GetMilliStart() == milliTime);
                milliTime = BMK_GetMilliStart();
                while(BMK_GetMilliSpan(milliTime) < TIMELOOP)
                {
                    int i;
                    for (i=0; i<100; i++)
                    {
                        h64 = XXH64(alignedBuffer, benchedSize, 0);
                        nbHashes++;
                    }
                }
                milliTime = BMK_GetMilliSpan(milliTime);
                if ((double)milliTime < fastestC*nbHashes) fastestC = (double)milliTime/nbHashes;
                DISPLAY("%1i-%-14.14s : %10i -> %7.1f MB/s\r", interationNb, "XXH64", (int)benchedSize, (double)benchedSize / fastestC / 1000.);
            }
            DISPLAY("%-16.16s : %10i -> %7.1f MB/s   0x%08X%08X\n", "XXH64", (int)benchedSize, (double)benchedSize / fastestC / 1000., (U32)(h64>>32), (U32)(h64));

            totals += benchedSize;
            totalc += fastestC;
        }

        free(buffer);
    }

    if (nbFiles > 1)
        printf("%-16.16s :%11llu -> %7.1f MB/s\n", "  TOTAL", (long long unsigned int)totals, (double)totals/totalc/1000.);

    return 0;
}



static void BMK_checkResult(U32 r1, U32 r2)
{
    static int nbTests = 1;

    if (r1==r2) DISPLAY("\rTest%3i : %08X == %08X   ok   ", nbTests, r1, r2);
    else
    {
        DISPLAY("\rERROR : Test%3i : %08X <> %08X   !!!!!   \n", nbTests, r1, r2);
        exit(1);
    }
    nbTests++;
}


static void BMK_checkResult64(U64 r1, U64 r2)
{
    static int nbTests = 1;

    if (r1!=r2)
    {
        DISPLAY("\rERROR : Test%3i : 64-bits values non equals   !!!!!   \n", nbTests);
        DISPLAY("\r %08X%08X != %08X%08X \n", (U32)(r1>>32), (U32)r1, (U32)(r2<<32), (U32)r2);
        exit(1);
    }
    nbTests++;
}


static void BMK_testSequence64(void* sentence, int len, U64 seed, U64 Nresult)
{
    U64 Dresult;
    XXH64_state_t state;
    int index;

    Dresult = XXH64(sentence, len, seed);
    BMK_checkResult64(Dresult, Nresult);

    XXH64_reset(&state, seed);
    XXH64_update(&state, sentence, len);
    Dresult = XXH64_digest(&state);
    BMK_checkResult64(Dresult, Nresult);

    XXH64_reset(&state, seed);
    for (index=0; index<len; index++) XXH64_update(&state, ((char*)sentence)+index, 1);
    Dresult = XXH64_digest(&state);
    BMK_checkResult64(Dresult, Nresult);
}


static void BMK_testSequence(void* sentence, int len, U32 seed, U32 Nresult)
{
    U32 Dresult;
    XXH32_state_t state;
    int index;

    Dresult = XXH32(sentence, len, seed);
    BMK_checkResult(Dresult, Nresult);

    XXH32_reset(&state, seed);
    XXH32_update(&state, sentence, len);
    Dresult = XXH32_digest(&state);
    BMK_checkResult(Dresult, Nresult);

    XXH32_reset(&state, seed);
    for (index=0; index<len; index++) XXH32_update(&state, ((char*)sentence)+index, 1);
    Dresult = XXH32_digest(&state);
    BMK_checkResult(Dresult, Nresult);
}


#define SANITY_BUFFER_SIZE 101
static void BMK_sanityCheck(void)
{
    BYTE sanityBuffer[SANITY_BUFFER_SIZE];
    int i;
    U32 prime = PRIME;

    for (i=0; i<SANITY_BUFFER_SIZE; i++)
    {
        sanityBuffer[i] = (BYTE)(prime>>24);
        prime *= prime;
    }

    BMK_testSequence(NULL,          0, 0,     0x02CC5D05);
    BMK_testSequence(NULL,          0, PRIME, 0x36B78AE7);
    BMK_testSequence(sanityBuffer,  1, 0,     0xB85CBEE5);
    BMK_testSequence(sanityBuffer,  1, PRIME, 0xD5845D64);
    BMK_testSequence(sanityBuffer, 14, 0,     0xE5AA0AB4);
    BMK_testSequence(sanityBuffer, 14, PRIME, 0x4481951D);
    BMK_testSequence(sanityBuffer, SANITY_BUFFER_SIZE, 0,     0x1F1AA412);
    BMK_testSequence(sanityBuffer, SANITY_BUFFER_SIZE, PRIME, 0x498EC8E2);

    BMK_testSequence64(NULL        ,  0, 0,     0xEF46DB3751D8E999ULL);
    BMK_testSequence64(NULL        ,  0, PRIME, 0xAC75FDA2929B17EFULL);
    BMK_testSequence64(sanityBuffer,  1, 0,     0x4FCE394CC88952D8ULL);
    BMK_testSequence64(sanityBuffer,  1, PRIME, 0x739840CB819FA723ULL);
    BMK_testSequence64(sanityBuffer, 14, 0,     0xCFFA8DB881BC3A3DULL);
    BMK_testSequence64(sanityBuffer, 14, PRIME, 0x5B9611585EFCC9CBULL);
    BMK_testSequence64(sanityBuffer, SANITY_BUFFER_SIZE, 0,     0x0EAB543384F878ADULL);
    BMK_testSequence64(sanityBuffer, SANITY_BUFFER_SIZE, PRIME, 0xCAA65939306F1E21ULL);

    DISPLAY("\r%79s\r", "");       // Clean display line
    DISPLAYLEVEL(2, "Sanity check -- all tests ok\n");
}


static int BMK_hash(const char* fileName, U32 hashNb)
{
    FILE*  inFile;
    size_t const blockSize = 64 KB;
    size_t readSize;
    char*  buffer;
    XXH64_state_t state;

    // Check file existence
    if (fileName == stdinName)
    {
        inFile = stdin;
        SET_BINARY_MODE(stdin);
    }
    else
        inFile = fopen( fileName, "rb" );
    if (inFile==NULL)
    {
        DISPLAY( "Pb opening %s\n", fileName);
        return 11;
    }

    // Memory allocation & restrictions
    buffer = (char*)malloc(blockSize);
    if(!buffer)
    {
        DISPLAY("\nError: not enough memory!\n");
        fclose(inFile);
        return 12;
    }

    // Init
    switch(hashNb)
    {
    case 0:
        XXH32_reset((XXH32_state_t*)&state, 0);
        break;
    case 1:
        XXH64_reset(&state, 0);
        break;
    default:
        DISPLAY("Error : bad hash algorithm ID\n");
        fclose(inFile);
        free(buffer);
        return -1;
    }


    // Load file & update hash
    DISPLAY("\rLoading %s...        \r", fileName);
    readSize = 1;
    while (readSize)
    {
        readSize = fread(buffer, 1, blockSize, inFile);
        switch(hashNb)
        {
        case 0:
            XXH32_update((XXH32_state_t*)&state, buffer, readSize);
            break;
        case 1:
            XXH64_update(&state, buffer, readSize);
            break;
        default:
            break;
        }
    }
    fclose(inFile);
    free(buffer);

    // display Hash
    switch(hashNb)
    {
    case 0:
        {
            U32 h32 = XXH32_digest((XXH32_state_t*)&state);
            DISPLAYRESULT("%08x   %s           \n", h32, fileName);
            break;
        }
    case 1:
        {
            U64 h64 = XXH64_digest(&state);
            DISPLAYRESULT("%08x%08x   %s     \n", (U32)(h64>>32), (U32)(h64), fileName);
            break;
        }
    default:
            break;
    }

    return 0;
}


//*********************************************************
//  Main
//*********************************************************

static int usage(const char* exename)
{
    DISPLAY( WELCOME_MESSAGE );
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [arg] [filename]\n", exename);
    DISPLAY( "When no filename provided, or - provided : use stdin as input\n");
    DISPLAY( "Arguments :\n");
    DISPLAY( " -H# : hash selection : 0=32bits, 1=64bits (default %i)\n", g_fn_selection);
    DISPLAY( " -b  : benchmark mode \n");
    DISPLAY( " -i# : number of iterations (benchmark mode; default %i)\n", g_nbIterations);
    DISPLAY( " -h  : help (this text)\n");
    return 0;
}


static int badusage(const char* exename)
{
    DISPLAY("Wrong parameters\n");
    usage(exename);
    return 1;
}


int main(int argc, char** argv)
{
    int i, filenamesStart=0;
    const char* input_filename = (char*)stdinName;
    const char* exename = argv[0];
    U32 benchmarkMode = 0;

    // xxh32sum default to 32 bits checksum
    if (strstr(exename, "xxh32sum")!=NULL) g_fn_selection=0;

    for(i=1; i<argc; i++)
    {
        char* argument = argv[i];

        if(!argument) continue;   // Protection if argument empty

        if (*argument!='-')
        {
            input_filename=argument;
            if (filenamesStart==0) filenamesStart=i;
            continue;
        }

        // Select command
        // note : *argument=='-'
        argument++;

        while (*argument!=0)
        {
            switch(*argument)
            {
            // Display help on usage
            case 'h':
                return usage(exename);

            // select hash algorithm
            case 'H':
                g_fn_selection = argument[1] - '0';
                argument+=2;
                break;

            // Trigger benchmark mode
            case 'b':
                argument++;
                benchmarkMode=1;
                break;

            // Modify Nb Iterations (benchmark only)
            case 'i':
                g_nbIterations = argument[1] - '0';
                argument+=2;
                break;

            default:
                return badusage(exename);
            }
        }
    }

    // Check if input is defined as console; trigger an error in this case
    if ((input_filename == stdinName) && IS_CONSOLE(stdin) ) return badusage(exename);

    // Check results are good
    if (benchmarkMode)
    {
        if (filenamesStart==0) return badusage(exename);
        DISPLAY( WELCOME_MESSAGE );
        BMK_sanityCheck();
        return BMK_benchFile(argv+filenamesStart, argc-filenamesStart);
    }

    if(g_fn_selection < 0 || g_fn_selection > 1) return badusage(exename);

    return BMK_hash(input_filename, g_fn_selection);
}
