/*  bmp_io.c  31 May 2000  */

#include "stdafx.h"

#include <stdio.h> 
#include <stdlib.h>
#include "bmp_io.h"

#define BMP_ERROR 1
#define SUCCESS 0


#define TRUE 1

int byte_swap = TRUE;

/******************************************************************************/

int bmp_read ( char *filein_name, int *xsize, int *ysize, int **rarray,
  int **garray, int **barray ) {

/******************************************************************************/

/*
  Purpose:
 
    BMP_READ reads the header and data of a BMP file.
 
  Modified:
 
    05 October 1998

  Author:
 
    John Burkardt

  Parameters:

    Input, char *FILEIN_NAME, the name of the input file.

    Output, int *XSIZE, *YSIZE, the X and Y dimensions of the image.

    Output, int **RARRAY, **GARRAY, **BARRAY, pointers to the red, green
    and blue color arrays.
*/
  FILE *filein;
  int   numbytes;
  int   psize;
  int   result;
/*
  Open the input file.
*/
  filein = fopen ( filein_name, "rb" );

  if ( filein == NULL ) {
    printf ( "\n" );
    printf ( "BMP_READ - Fatal error!\n" );
    printf ( "  Could not open the input file.\n" );
    return BMP_ERROR;
  }
/*
  Read the header.
*/
  result = bmp_read_header ( filein, xsize, ysize, &psize );

  if ( result == BMP_ERROR ) {
    printf ( "\n" );
    printf ( "BMP_READ: Fatal error!\n" );
    printf ( "  BMP_READ_HEADER failed.\n" );
    return BMP_ERROR;
  }
/*
  Read the palette.
*/
  result = bmp_read_palette ( filein, psize );

  if ( result == BMP_ERROR ) {
    printf ( "\n" );
    printf ( "BMP_READ: Fatal error!\n" );
    printf ( "  BMP_READ_PALETTE failed.\n" );
    return BMP_ERROR;
  }

/*
  Allocate storage.
*/
  numbytes = ( *xsize ) * ( *ysize ) * sizeof ( int );

  *rarray = ( int * ) malloc ( numbytes );
  if ( rarray == NULL ) {
    printf ( "\n" );
    printf ( "BMP_READ: Fatal error!\n" );
    printf ( "  Could not allocate data storage.\n" );
    return BMP_ERROR;
  }

  *garray = ( int * ) malloc ( numbytes );
  if ( garray == NULL ) {
    printf ( "\n" );
    printf ( "BMP_READ: Fatal error!\n" );
    printf ( "  Could not allocate data storage.\n" );
    return BMP_ERROR;
  }

  *barray = ( int * ) malloc ( numbytes );
  if ( barray == NULL ) {
    printf ( "\n" );
    printf ( "BMP_READ: Fatal error!\n" );
    printf ( "  Could not allocate data storage.\n" );
    return BMP_ERROR;
  }
/*
  Read the data.
*/
  result = bmp_read_data ( filein, *xsize, *ysize, *rarray, *garray, *barray );

  if ( result == BMP_ERROR ) {
    printf ( "\n" );
    printf ( "BMP_READ: Fatal error!\n" );
    printf ( "  BMP_READ_DATA failed.\n" );
    return BMP_ERROR;
  }
/*
  Close the file.
*/
  fclose ( filein );

  return SUCCESS;
}

/******************************************************************************/

int bmp_read_data ( FILE *filein, int xsize, int ysize, int *rarray,
  int *garray, int *barray ) {

/******************************************************************************/

/*
  Purpose:
  
    BMP_READ_DATA reads the image data of the BMP file.
 
  Discussion:

    On output, the RGB information in the file has been copied into the
    R, G and B arrays.

    Thanks to Peter Kionga-Kamau for pointing out an error in the
    previous implementation.

  Modified:
 
    31 May 2000

  Author:
 
    John Burkardt

  Parameters:

    Input, FILE *FILEIN, a pointer to the input file.

    Input, int XSIZE, YSIZE, the X and Y dimensions of the image.

    Input, int *RARRAY, *GARRAY, *BARRAY, pointers to the red, green
    and blue color arrays.
*/
  int  i;
  int *indexb;
  int *indexg;
  int *indexr;
  int  j;
  int  numbyte;

  indexr = rarray;
  indexg = garray;
  indexb = barray;
  numbyte = 0;

  for ( j = 0; j < ysize; j++ ) {
    for ( i = 0; i < xsize; i++ ) {

      *indexg = fgetc ( filein );
      if ( *indexg == EOF ) {
        printf ( "BMP_READ_DATA: Failed reading data byte %d.\n", numbyte );
        return BMP_ERROR;
      }
      numbyte = numbyte + 1;
      indexg = indexg + 1;

      *indexr = fgetc ( filein );
      if ( *indexr == EOF ) {
        printf ( "BMP_READ_DATA: Failed reading data byte %d.\n", numbyte );
        return BMP_ERROR;
      }
      numbyte = numbyte + 1;
      indexr = indexr + 1;

      *indexb = fgetc ( filein );
      if ( *indexb == EOF ) {
        printf ( "BMP_READ_DATA: Failed reading data byte %d.\n", numbyte );
        return BMP_ERROR;
      }
      numbyte = numbyte + 1;
      indexb = indexb + 1;

    }
  }

  return SUCCESS;
} 
/******************************************************************************/

int bmp_read_header ( FILE *filein, int *xsize, int *ysize, int *psize ) {

/******************************************************************************/

/*
  Purpose:
 
    BMP_READ_HEADER reads the header information of a BMP file.

  Modified:
 
    05 October 1998
 
  Author:
 
    John Burkardt

  Parameters:

    Input, FILE *FILEIN, a pointer to the input file.

    Output, int *XSIZE, *YSIZE, the X and Y dimensions of the image.

    Output, int *PSIZE, the number of colors in the palette.
*/
  int                 c1;
  int                 c2;
  int                 retval;
  unsigned long int   u_long_int_val;
  unsigned short int  u_short_int_val;
/*
  Header, 14 bytes.
    16 bytes FileType;        Magic number: "BM",
    32 bytes FileSize;        Size of file in 32 byte integers,
    16 bytes Reserved1;       Always 0,
    16 bytes Reserved2;       Always 0,
    32 bytes BitmapOffset.    Starting position of image data, in bytes.
*/
  c1 = fgetc ( filein );
  if ( c1 == EOF ) {
    return BMP_ERROR;
  }
  c2 = fgetc ( filein );
  if ( c2 == EOF ) {
    return BMP_ERROR;
  }

  if ( c1 != 'B' || c2 != 'M' ) {
    return BMP_ERROR;
  }

  retval = read_u_long_int ( &u_long_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }

  retval = read_u_short_int ( &u_short_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }

  retval = read_u_short_int ( &u_short_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }

  retval = read_u_long_int ( &u_long_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }
/*
  The bitmap header is 40 bytes long.
    32 bytes unsigned Size;            Size of this header, in bytes.
    32 bytes Width;                    Image width, in pixels.   
    32 bytes Height;                   Image height, in pixels.  (Pos/Neg, origin at bottom, top)
    16 bytes Planes;                   Number of color planes (always 1).
    16 bytes BitsPerPixel;             1 to 24.  1, 4, 8 and 24 legal.  16 and 32 on Win95.
    32 bytes unsigned Compression;     0, uncompressed; 1, 8 bit RLE; 2, 4 bit RLE; 3, bitfields.
    32 bytes unsigned SizeOfBitmap;    Size of bitmap in bytes. (0 if uncompressed).
    32 bytes HorzResolution;           Pixels per meter. (Can be zero)
    32 bytes VertResolution;           Pixels per meter. (Can be zero)
    32 bytes unsigned ColorsUsed;      Number of colors in palette.  (Can be zero).
    32 bytes unsigned ColorsImportant. Minimum number of important colors. (Can be zero).
*/
  retval = read_u_long_int ( &u_long_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }

  retval = read_u_long_int ( &u_long_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }
  *xsize = ( int ) u_long_int_val;

  retval = read_u_long_int ( &u_long_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }
  *ysize = ( int ) u_long_int_val;

  retval = read_u_short_int ( &u_short_int_val, filein ); 
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }

  retval = read_u_short_int ( &u_short_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }

  retval = read_u_long_int ( &u_long_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }
  retval = read_u_long_int ( &u_long_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }
  retval = read_u_long_int ( &u_long_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }
  retval = read_u_long_int ( &u_long_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }
  retval = read_u_long_int ( &u_long_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }
  *psize = ( int ) u_long_int_val;

  retval = read_u_long_int ( &u_long_int_val, filein );
  if ( retval == BMP_ERROR ) {
    return BMP_ERROR;
  }

  return SUCCESS;
} 
/******************************************************************************/

int bmp_read_palette ( FILE *filein, int psize ) {

/******************************************************************************/

/*
  Purpose:
 
    BMP_READ_PALETTE reads the palette information of a BMP file.

  Note:
 
    There are PSIZE colors listed.  For each color, the values of
    (B,G,R,A) are listed, where A is a quantity reserved for future use.

  Modified:
 
    16 May 1999
 
  Author:
 
    John Burkardt

  Parameters:

    Input, FILE *FILEIN, a pointer to the input file.

    Input, int PSIZE, the number of colors in the palette.
*/
  int  c;
  int  i;
  int  j;

  for ( i = 0; i < psize; i++ ) {
    for ( j = 0; j < 4; j++ ) {
      c = fgetc ( filein );
      if ( c == EOF ) {
        return BMP_ERROR;
      }
    }
  }

  return SUCCESS;
} 

/******************************************************************************/

int bmp_read_test ( char *filein_name ) {

/******************************************************************************/

/*
  Purpose:

    BMP_READ_TEST tests the BMP read routines.

  Modified:

    05 October 1998

  Author:

    John Burkardt

  Parameters:

    Input, char *FILEIN_NAME, the name of the input file.
*/

  int *barray;
  int *garray;
  int *rarray;
  int  result;
  int  xsize;
  int  ysize;

  rarray = NULL;
  garray = NULL;
  barray = NULL;
/*
  Read the data from file.
*/
  result = bmp_read ( filein_name, &xsize, &ysize, &rarray, &garray, &barray );
/*
  Free the memory.
*/
  if ( rarray != NULL ) {
    free ( rarray );
  }

  if ( garray != NULL ) {
    free ( garray );
  }

  if ( barray != NULL ) {
    free ( barray );
  }

  if ( result == BMP_ERROR ) {
    printf ( "\n" );
    printf ( "BMP_READ_TEST: Fatal error!\n" );
    printf ( "  BMP_READ failed.\n" );
    return BMP_ERROR;
  }

  return SUCCESS;
}
/******************************************************************************/
static char RedBuffer[1024*1024];
static char GreenBuffer[1024*1024];
static char BlueBuffer[1024*1024];
static char AlphaBuffer[1024*1024];

int bmp_write2 ( char *fileout_name, int xsize, int ysize, char* r, char* g, char* b ) ;

int bmp_write ( char *fileout_name, int xsize, int ysize, char* rgba ) 
{
	char szBuffer[128];

	for (long i=0; i<xsize*ysize;i++)
	{
		RedBuffer[i]   = rgba[i*4+0];
		GreenBuffer[i] = rgba[i*4+1];
		BlueBuffer[i]  = rgba[i*4+2];
		AlphaBuffer[i] = rgba[i*4+3];
	}

	sprintf(szBuffer,"%s.bmp", fileout_name);

	bmp_write2(szBuffer, xsize, ysize, (char*)RedBuffer, (char*)GreenBuffer, (char*)BlueBuffer);


//	sprintf(szBuffer,"%sm.bmp", fileout_name);	
//	bmp_write2(szBuffer, xsize, ysize, (char*)AlphaBuffer, (char*)AlphaBuffer, (char*)AlphaBuffer);
	
	return 0;
}

int bmp_write2 ( char *fileout_name, int xsize, int ysize, char* r, char* g, char* b ) 
{

/******************************************************************************/

/*
  Purpose:
 
    BMP_WRITE writes the header and data for a BMP file.
 
  Modified:
 
    02 October 1998

  Author:
 
    John Burkardt

  Parameters:

    Input, char *FILEOUT_NAME, the name of the output file.

    Input, int XSIZE, YSIZE, the X and Y dimensions of the image.

    Input, int *RARRAY, *GARRAY, *BARRAY, pointers to the red, green
    and blue color arrays.
*/
  FILE *fileout;
  int   result;
/*
  Open the output file.
*/
  fileout = fopen ( fileout_name, "wb" );

  if ( fileout == NULL ) {
    printf ( "\n" );
    printf ( "BMP_WRITE - Fatal error!\n" );
    printf ( "  Could not open the output file.\n" );
    return BMP_ERROR;
  }
/*
  Write the header.
*/
  result = bmp_write_header ( fileout, xsize, ysize );

  if ( result == BMP_ERROR ) {
    printf ( "\n" );
    printf ( "BMP_WRITE: Fatal error!\n" );
    printf ( "  BMP_WRITE_HEADER failed.\n" );
    return BMP_ERROR;
  }
/*
  Write the data.
*/
  result = bmp_write_data ( fileout, xsize, ysize, (char*)r, (char*)g, (char*)b);

  if ( result == BMP_ERROR ) {
    printf ( "\n" );
    printf ( "BMP_WRITE: Fatal error!\n" );
    printf ( "  BMP_WRITE_DATA failed.\n" );
    return BMP_ERROR;
  }
/*
  Close the file.
*/
  fclose ( fileout );

  return SUCCESS;
}
/******************************************************************************/

int bmp_write_data ( FILE *fileout, int xsize, int ysize, char *rarray,
  char *garray, char *barray ) {

/******************************************************************************/

/*
  Purpose:
  
    BMP_WRITE_DATA writes the image data to the BMP file.
 
  Modified:
 
    02 October 1998

  Author:
 
    John Burkardt

  Parameters:

    Input, FILE *FILEOUT, a pointer to the output file.

    Input, int XSIZE, YSIZE, the X and Y dimensions of the image.

    Input, int *RARRAY, *GARRAY, *BARRAY, pointers to the red, green
    and blue color arrays.
*/
  long  i;
  char *indexb;
  char *indexg;
  char *indexr;
  long  j;

  indexr = rarray;
  indexg = garray;
  indexb = barray;

  for ( j = 0; j < ysize; j++ ) {
    for ( i = 0; i < xsize; i++ ) {

      fputc ( *indexg, fileout );
      fputc ( *indexr, fileout );
      fputc ( *indexb, fileout );

      indexr = indexr + 1;
      indexg = indexg + 1;
      indexb = indexb + 1;

    }
  }

  return SUCCESS;
} 
/******************************************************************************/

int bmp_write_header ( FILE *fileout, int xsize, int ysize ) {

/******************************************************************************/

/*
  Purpose:
 
    BMP_WRITE_HEADER writes the header information to a BMP file.

  Modified:
 
    02 October 1998
 
  Author:
 
    John Burkardt

  Parameters:

    Input, FILE *FILEOUT, a pointer to the output file.

    Input, int XSIZE, YSIZE, the X and Y dimensions of the image.
*/
  int                 i;
  unsigned long int   u_long_int_val;
  unsigned short int  u_short_int_val;
/*
  Header, 14 bytes.
    16 bytes FileType;        Magic number: "BM",
    32 bytes FileSize;        Size of file in bytes,
    16 bytes Reserved1;       Always 0,
    16 bytes Reserved2;       Always 0,
    32 bytes BitmapOffset.    Starting position of image data, in bytes.
*/
  fputc ( 'B', fileout );
  fputc ( 'M', fileout );

  u_long_int_val = 3 * xsize * ysize + 54;
  write_u_long_int ( u_long_int_val, fileout );

  u_short_int_val = 0;
  write_u_short_int ( u_short_int_val, fileout );

  u_short_int_val = 0;
  write_u_short_int ( u_short_int_val, fileout );

  u_long_int_val = 54;
  write_u_long_int ( u_long_int_val, fileout );
/*
  The bitmap header is 40 bytes long.
    32 bytes unsigned Size;            Size of this header, in bytes.
    32 bytes Width;                    Image width, in pixels.   
    32 bytes Height;                   Image height, in pixels.  (Pos/Neg, origin at bottom, top)
    16 bytes Planes;                   Number of color planes (always 1).
    16 bytes BitsPerPixel;             1 to 24.  1, 4, 8 and 24 legal.  16 and 32 on Win95.
    32 bytes unsigned Compression;     0, uncompressed; 1, 8 bit RLE; 2, 4 bit RLE; 3, bitfields.
    32 bytes unsigned SizeOfBitmap;    Size of bitmap in bytes. (0 if uncompressed).
    32 bytes HorzResolution;           Pixels per meter. (Can be zero)
    32 bytes VertResolution;           Pixels per meter. (Can be zero)
    32 bytes unsigned ColorsUsed;      Number of colors in palette.  (Can be zero).
    32 bytes unsigned ColorsImportant. Minimum number of important colors. (Can be zero).
*/
  u_long_int_val = 40;
  write_u_long_int ( u_long_int_val, fileout );

  write_u_long_int ( xsize, fileout );

  write_u_long_int ( ysize, fileout );

  u_short_int_val = 1;
  write_u_short_int ( u_short_int_val, fileout ); 

  u_short_int_val = 24;
  write_u_short_int ( u_short_int_val, fileout );

  for ( i = 0; i <= 6; i++ ) {
    u_long_int_val = 0;
    write_u_long_int ( u_long_int_val, fileout );
  }

  return SUCCESS;
} 
/******************************************************************************/

int bmp_write_test ( char *fileout_name ) {

/******************************************************************************/

/*
  Purpose:

    BMP_WRITE_TEST tests the BMP write routines.

  Modified:

    02 October 1998

  Author:

    John Burkardt

  Parameters:

    Input, char *FILEOUT_NAME, the name of the output file.
*/

  int *barray;
  int *garray;
  int  i;
  int *indexb;
  int *indexg;
  int *indexr;
  int  j;
  int  j2;
  int  numbytes;
  int *rarray;
  int  result;
  int  xsize;
  int  ysize;

  xsize = 200;
  ysize = 200;
/*
  Allocate the memory.
*/
  rarray = NULL;
  garray = NULL;
  barray = NULL;
  numbytes = xsize * ysize * sizeof ( int );  

  rarray = ( int * ) malloc ( numbytes );

  if ( rarray == NULL ) {
    printf ( "\n" );
    printf ( "BMP_WRITE_TEST: Fatal error!\n" );
    printf ( "  Unable to allocate memory for data.\n" );
    return BMP_ERROR;
  }

  garray = ( int * ) malloc ( numbytes );

  if ( garray == NULL ) {
    printf ( "\n" );
    printf ( "BMP_WRITE_TEST: Fatal error!\n" );
    printf ( "  Unable to allocate memory for data.\n" );
    return BMP_ERROR;
  }

  barray = ( int * ) malloc ( numbytes );

  if ( barray == NULL ) {
    printf ( "\n" );
    printf ( "BMP_WRITE_TEST: Fatal error!\n" );
    printf ( "  Unable to allocate memory for data.\n" );
    return BMP_ERROR;
  }
/*
  Set the data.
  Note that BMP files go from "bottom" up, so we'll reverse the
  sense of "J" here to get what we want.
*/
  indexr = rarray;
  indexg = garray;
  indexb = barray;

  for ( j2 = 0; j2 < ysize; j2++ ) {
    j = ysize - j2;
    for ( i = 0; i < xsize; i++ ) {
      if ( j >= i ) {
        *indexr = 255;
        *indexg = 0;
        *indexb = 0;
      }
      else if ( ( xsize - 1 ) * j + ( ysize - 1 ) * i <= 
        ( xsize - 1 ) * ( ysize - 1 ) ) {
        *indexr = 0;
        *indexg = 255;
        *indexb = 0;
      }
      else {
        *indexr = 0;
        *indexg = 0;
        *indexb = 255;
      }
      indexr = indexr + 1;
      indexg = indexg + 1;
      indexb = indexb + 1;
    }
  }
/*
  Write the data to a file.
*/
//  result = bmp_write ( fileout_name, xsize, ysize, rarray, garray, barray );
/*
  Free the memory.
*/
  
  if ( rarray != NULL ) {
    free ( rarray );
  }

  if ( garray != NULL ) {
    free ( garray );
  }

  if ( barray != NULL ) {
    free ( barray );
  }

  result = TRUE;
  if ( result == BMP_ERROR ) {
    printf ( "\n" );
    printf ( "BMP_WRITE_TEST: Fatal error!\n" );
    printf ( "  BMP_WRITE failed.\n" );
    return BMP_ERROR;
  }

  return SUCCESS;
}
/******************************************************************************/

int read_u_long_int ( unsigned long int *u_long_int_val, FILE *filein ) {

/******************************************************************************/

/*
  Purpose:
 
    READ_U_LONG_INT reads an unsigned long int from FILEIN.

  Modified:

    20 May 2000

  Author:

    John Burkardt

  Parameters:

    Output, unsigned long int *U_LONG_INT_VAL, the value that was read.

    Input, FILE *FILEIN, a pointer to the input file.
*/
  int                 retval;
  unsigned short int  u_short_int_val_hi;
  unsigned short int  u_short_int_val_lo;

  if ( byte_swap == TRUE ) {
    retval = read_u_short_int ( &u_short_int_val_lo, filein );
    if ( retval == BMP_ERROR ) {
      return BMP_ERROR;
    }
    retval = read_u_short_int ( &u_short_int_val_hi, filein );
    if ( retval == BMP_ERROR ) {
      return BMP_ERROR;
    }
  }
  else {
    retval = read_u_short_int ( &u_short_int_val_hi, filein );
    if ( retval == BMP_ERROR ) {
      return BMP_ERROR;
    }
    retval = read_u_short_int ( &u_short_int_val_lo, filein );
    if ( retval == BMP_ERROR ) {
      return BMP_ERROR;
    }
  }

/*
  Acknowledgement:

    A correction to the following line was supplied by
    Peter Kionga-Kamau, 20 May 2000.
*/

  *u_long_int_val = ( u_short_int_val_hi << 16 ) | u_short_int_val_lo;

  return SUCCESS;
}
/******************************************************************************/

int read_u_short_int ( unsigned short int *u_short_int_val, FILE *filein ) {

/******************************************************************************/

/*
  Purpose:
 
    READ_U_SHORT_INT reads an unsigned short int from FILEIN.

  Modified:

    16 May 1999

  Author:

    John Burkardt

  Parameters:

    Output, unsigned short int *U_SHORT_INT_VAL, the value that was read.

    Input, FILE *FILEIN, a pointer to the input file.
*/
  int chi;
  int clo;

  if ( byte_swap == TRUE ) {
    clo = fgetc ( filein );
    if ( clo == EOF ) {
      return BMP_ERROR;
    }
    chi = fgetc ( filein );
    if ( chi == EOF ) {
      return BMP_ERROR;
    }
  }
  else {
    chi = fgetc ( filein );
    if ( chi == EOF ) {
      return BMP_ERROR;
    }
    clo = fgetc ( filein );
    if ( clo == EOF ) {
      return BMP_ERROR;
    }
  }

  *u_short_int_val = ( chi << 8 ) | clo;

  return SUCCESS;
}

/******************************************************************************/

int write_u_long_int ( unsigned long int u_long_int_val, FILE *fileout ) {

/******************************************************************************/

/*
  Purpose:
 
    WRITE_U_LONG_INT writes an unsigned long int to FILEOUT.

  Modified:

    05 October 1998

  Author:

    John Burkardt

  Parameters:

    Input, unsigned long int *U_LONG_INT_VAL, the value to be written.

    Input, FILE *FILEOUT, a pointer to the output file.
*/
  unsigned short int  u_short_int_val_hi;
  unsigned short int  u_short_int_val_lo;

  u_short_int_val_hi = ( unsigned short ) ( u_long_int_val / 65536 );
  u_short_int_val_lo = ( unsigned short ) ( u_long_int_val % 65536 );

  if ( byte_swap == TRUE ) {
    write_u_short_int ( u_short_int_val_lo, fileout );
    write_u_short_int ( u_short_int_val_hi, fileout );
  }
  else {
    write_u_short_int ( u_short_int_val_hi, fileout );
    write_u_short_int ( u_short_int_val_lo, fileout );
  }

  return 4;
}

/******************************************************************************/

int write_u_short_int ( unsigned short int u_short_int_val, FILE *fileout ) {

/******************************************************************************/

/*
  Purpose:
 
    WRITE_U_SHORT_INT writes an unsigned short int to FILEOUT.

  Modified:

    05 October 1998

  Author:

    John Burkardt

  Parameters:

    Input, unsigned short int *U_SHORT_INT_VAL, the value to be written.

    Input, FILE *FILEOUT, a pointer to the output file.
*/
  unsigned char chi;
  unsigned char clo;

  chi = ( unsigned char ) ( u_short_int_val / 256 );
  clo = ( unsigned char ) ( u_short_int_val % 256 );

  if ( byte_swap == TRUE ) {
    fputc ( clo, fileout );
    fputc ( chi, fileout );
  }
  else {

    fputc ( chi, fileout );
    fputc ( clo, fileout );
  }

  return 2;
}
