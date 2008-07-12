/*  bmp_io.h  16 May 1999  */

#ifndef _BMP_IO_H
#define _BMP_IO_H

int bmp_read          ( char *filein_name, int *xsize, int *ysize, int **rarray,
                        int **garray, int **barray );
int bmp_read_data     ( FILE *filein, int xsize, int ysize, int *rarray,
                        int *garray, int *barray );
int bmp_read_header   ( FILE *filein, int *xsize, int *ysize, int *psize );
int bmp_read_palette  ( FILE *filein, int psize );
int bmp_read_test     ( char *filein_name );

int bmp_write ( char *fileout_name, int xsize, int ysize, char* rgba );
int bmp_write_data ( FILE *fileout, int xsize, int ysize, char *rarray, char *garray, char *barray );

int bmp_write_header  ( FILE *fileout, int xsize, int ysize );
int bmp_write_test    ( char *fileout_name );

int read_u_long_int   ( unsigned long int *u_long_int_val, FILE *filein );
int read_u_short_int  ( unsigned short int *u_short_int_val, FILE *filein );

int write_u_long_int  ( unsigned long int u_long_int_val, FILE *fileout );
int write_u_short_int ( unsigned short int u_short_int_val, FILE *fileout );


#endif