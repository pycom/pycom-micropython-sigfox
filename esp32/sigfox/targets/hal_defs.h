/*******************************************************************************
*  Filename:        hal_defs.h
*  Revised:         $Date: 2014-03-27 16:58:25 +0100 (to, 27 mar 2014) $
*  Revision:        $Revision: 12664 $
*
*  Description:     HAL defines.
*
*  Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
*
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*    Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*******************************************************************************/

#ifndef HAL_DEFS_H
#define HAL_DEFS_H
/*******************************************************************************
* CONSTANTS AND DEFINES
*/

#ifndef TRUE
#define TRUE 1
#else
#ifdef __IAR_SYSTEMS_ICC__
#warning "Macro TRUE already defined"
#endif
#endif

#ifndef FALSE
#define FALSE 0
#else
#ifdef __IAR_SYSTEMS_ICC__
#warning "Macro FALSE already defined"
#endif
#endif

#ifndef NULL
#define NULL (void *)0
#else
#ifdef __IAR_SYSTEMS_ICC__
#warning "Macro NULL already defined"
#endif
#endif

#ifndef SUCCESS
#define SUCCESS 0
#else
#warning "Macro SUCCESS already defined"
#endif

#ifndef FAILED
#ifndef WIN32
#define FAILED  1
#endif
#else
#ifdef __IAR_SYSTEMS_ICC__
#warning "Macro FAILED already defined"
#endif
#endif


/*******************************************************************************
* MACROS
*/
#ifndef BV
#define BV(n)      (1 << (n))
#endif

#ifndef BM
#define BM(n)      (1 << (n))
#endif

#ifndef BF
#define BF(x,b,s)  (((x) & (b)) >> (s))
#endif

#ifndef MIN
#define MIN(n,m)   (((n) < (m)) ? (n) : (m))
#endif

#ifndef MAX
#define MAX(n,m)   (((n) < (m)) ? (m) : (n))
#endif

#ifndef ABS
#define ABS(n)     (((n) < 0) ? -(n) : (n))
#endif

// ANSI C types are used for CC2538 and ARM processors
#if defined(__ICCARM__)
/* uint32 processing */
#define BREAK_UINT32( var, ByteNum ) \
    (unsigned char)((unsigned long)(((var) >>((ByteNum) * 8)) & 0x00FF))

#define BUILD_UINT32(Byte0, Byte1, Byte2, Byte3) \
    ((unsigned long)((unsigned long)((Byte0) & 0x00FF) \
        + ((unsigned long)((Byte1) & 0x00FF) << 8) \
            + ((unsigned long)((Byte2) & 0x00FF) << 16) \
                + ((unsigned long)((Byte3) & 0x00FF) << 24)))

#define HI_UINT32(a) ((unsigned short) (((unsigned long)(a)) >> 16))
#define LO_UINT32(a) ((unsigned short) ((unsigned long)(a)))

/* uint16 processing */
#define BUILD_UINT16(loByte, hiByte) \
    ((unsigned short)(((loByte) & 0x00FF) + (((hiByte) & 0x00FF) << 8)))
#define HI_UINT16(a) (((unsigned short)(a) >> 8) & 0xFF)
#define LO_UINT16(a) ((unsigned short)(a) & 0xFF)

/* uint16 processing */
#define BUILD_UINT8(hiByte, loByte) \
    ((unsigned char)(((loByte) & 0x0F) + (((hiByte) & 0x0F) << 4)))

#define HI_UINT8(a) (((unsigned char)(a) >> 4) & 0x0F)
#define LO_UINT8(a) ((unsigned char)(a) & 0x0F)

#else

/* uint32 processing */
#define BREAK_UINT32( var, ByteNum ) \
    (uint8)((uint32)(((var) >>((ByteNum) * 8)) & 0x00FF))

#define BUILD_UINT32(Byte0, Byte1, Byte2, Byte3) \
    ((uint32)((uint32)((Byte0) & 0x00FF) \
        + ((uint32)((Byte1) & 0x00FF) << 8) \
            + ((uint32)((Byte2) & 0x00FF) << 16) \
                + ((uint32)((Byte3) & 0x00FF) << 24)))

#define HI_UINT32(a) ((uint16) (((uint32)(a)) >> 16))
#define LO_UINT32(a) ((uint16) ((uint32)(a)))


/* uint16 processing */
#define BUILD_UINT16(loByte, hiByte) \
    ((uint16)(((loByte) & 0x00FF) + (((hiByte) & 0x00FF) << 8)))

#define HI_UINT16(a) (((uint16)(a) >> 8) & 0xFF)
#define LO_UINT16(a) ((uint16)(a) & 0xFF)


/* uint16 processing */
#define BUILD_UINT8(hiByte, loByte) \
    ((uint8)(((loByte) & 0x0F) + (((hiByte) & 0x0F) << 4)))

#define HI_UINT8(a) (((uint8)(a) >> 4) & 0x0F)
#define LO_UINT8(a) ((uint8)(a) & 0x0F)

#endif /* ifdef __ICCARM__ */

/*******************************************************************************
* HOST TO NETWORK BYTE ORDER MACROS
*/
#ifdef BIG_ENDIAN
#if defined(ewarm) || defined(__ICCARM__)
#define UINT16_HTON(x)  st( utilReverseBuf((unsigned char*)&x, sizeof(unsigned short)); )
#define UINT16_NTOH(x)  st( utilReverseBuf((unsigned char*)&x, sizeof(unsigned short)); )

#define UINT32_HTON(x)  st( utilReverseBuf((unsigned char*)&x, sizeof(unsigned long)); )
#define UINT32_NTOH(x)  st( utilReverseBuf((unsigned char*)&x, sizeof(unsigned long)); )
#else
#define UINT16_HTON(x)  st( utilReverseBuf((unsigned char*)&x, sizeof(unsigned short)); )
#define UINT16_NTOH(x)  st( utilReverseBuf((unsigned char*)&x, sizeof(unsigned short)); )

#define UINT32_HTON(x)  st( utilReverseBuf((unsigned char*)&x, sizeof(unsigned long)); )
#define UINT32_NTOH(x)  st( utilReverseBuf((unsigned char*)&x, sizeof(unsigned long)); )
#endif /* ifdef ewarm */
#else
#define UINT16_HTON(x)
#define UINT16_NTOH(x)

#define UINT32_HTON(x)
#define UINT32_NTOH(x)
#endif


/*
*  This macro is for use by other macros to form a fully valid C statement.
*  Without this, the if/else conditionals could show unexpected behavior.
*
*  For example, use...
*    #define SET_REGS()  st( ioreg1 = 0; ioreg2 = 0; )
*  instead of ...
*    #define SET_REGS()  { ioreg1 = 0; ioreg2 = 0; }
*  or
*    #define  SET_REGS()    ioreg1 = 0; ioreg2 = 0;
*  The last macro would not behave as expected in the if/else construct.
*  The second to last macro will cause a compiler error in certain uses
*  of if/else construct
*
*  It is not necessary, or recommended, to use this macro where there is
*  already a valid C statement.  For example, the following is redundant...
*    #define CALL_FUNC()   st(  func();  )
*  This should simply be...
*    #define CALL_FUNC()   func()
*
* (The while condition below evaluates false without generating a
*  constant-controlling-loop type of warning on most compilers.)
*/
#define st(x)      do { x } while (__LINE__ == -1)


#endif // #ifndef HAL_DEFS_H
