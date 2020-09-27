// This file contains the modifications/extensions done to the
// bzlib code by Pycom

#include "bzlib_private.h"
#include "py/obj.h"


/*---------------------------------------------------*/
/*---         Pycom Memory Allocators             ---*/
/*---------------------------------------------------*/
static void* pycom_malloc ( void* opaque, Int32 items, Int32 size )
{
   // Allocating memory from SPI RAM
   void* v = heap_caps_malloc( items * size, MALLOC_CAP_SPIRAM );
   return v;
}

static void pycom_free ( void* opaque, void* addr )
{
   heap_caps_free( addr );
}

// New API

/*
 * Initialize a bz_stream with the provided source buffer.
 * Also calls the DecompressInit which allocates memory
 * for stream stats structure.
 */
int BZ_API(BZ2_bzDecompressStreamInit) 
                           ( void**        strm,
                             char*         source, 
                             unsigned int  sourceLen )
{
   bz_stream *bzstrm;
   int ret;

   if ( strm == NULL || source == NULL )
          return BZ_PARAM_ERROR;

   bzstrm = pycom_malloc(NULL, 1, sizeof(bz_stream));

   bzstrm->bzalloc = pycom_malloc;
   bzstrm->bzfree = pycom_free;
   bzstrm->opaque = NULL;
   ret = BZ2_bzDecompressInit ( bzstrm, 1, 1 );

   bzstrm->next_in = source;
   bzstrm->avail_in = sourceLen;

   if (ret != BZ_OK)
   {
       printf("Init failed...\n");
       pycom_free(NULL, bzstrm);
       return ret;
   }
   *strm = bzstrm;

   return BZ_OK;
}

/*
 * De-inits and frees the bz_stream
 */
int BZ_API(BZ2_bzDecompressStreamEnd) 
                           ( void*        strm )
{
    int ret;
    ret = BZ2_bzDecompressEnd ( (bz_stream*)strm );
    pycom_free(NULL, strm);

    return ret;
}

/*
 * Reads a maximum of destLen number of decompressed bytes
 * from the provided stream. Updates the destLen to the
 * actual bytes read.
 */
int BZ_API(BZ2_bzDecompressRead) 
                           ( void*         strm, 
                             char*         dest,
                             unsigned int* destLen )
{
    bz_stream *bzstrm;
    int ret;

    if(strm == NULL || dest == NULL || destLen == NULL )
       return BZ_PARAM_ERROR;

   bzstrm = (bz_stream*)strm;

   bzstrm->next_out = dest;
   bzstrm->avail_out = *destLen;

   ret = BZ2_bzDecompress ( bzstrm );
   if (ret != BZ_OK && ret != BZ_STREAM_END)
   {
       printf("Decomp...ERR: %d\n", ret);
       return ret;
   }

   /* normal termination */
   *destLen -= bzstrm->avail_out;

   return BZ_OK;
}