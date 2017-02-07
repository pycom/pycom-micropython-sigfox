/*******************************************************************************
*  Filename:        hal_types.h
*  Revised:         $Date: 2014-03-27 16:58:25 +0100 (to, 27 mar 2014) $
*  Revision:        $Revision: 12664 $
*
*  Description:     HAL type definitions.
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

#ifndef HAL_TYPES_H
#define HAL_TYPES_H

/*******************************************************************************
* If building with a C++ compiler, make all of the definitions in this header
* have a C binding.
*******************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif


/*******************************************************************************
* TYPEDEFS
*/
typedef signed   char   int8;
typedef unsigned char   uint8;

typedef signed   short  int16;
typedef unsigned short  uint16;

typedef signed   long   int32;
typedef unsigned long   uint32;

typedef void (*ISR_FUNC_PTR)(void);
typedef void (*VFPTR)(void);


/*******************************************************************************
* COMPILER ABSTRACTION
*/

/*******************************************************************************
* IAR MSP430
*/
#ifdef __IAR_SYSTEMS_ICC__

#define _PRAGMA(x) _Pragma(#x)

#if defined __ICC430__

#ifndef CODE
#define CODE
#endif
#ifndef XDATA
#define XDATA
#endif
#define FAR
#define NOP()  asm("NOP")

#define HAL_ISR_FUNC_DECLARATION(f,v)   \
    _PRAGMA(vector=v##_VECTOR) __interrupt void f(void)
#define HAL_ISR_FUNC_PROTOTYPE(f,v)     \
    _PRAGMA(vector=v##_VECTOR) __interrupt void f(void)
#define HAL_ISR_FUNCTION(f,v)           \
    HAL_ISR_FUNC_PROTOTYPE(f,v); HAL_ISR_FUNC_DECLARATION(f,v)


/*******************************************************************************
* IAR 8051
*/
#elif defined __ICC8051__

#ifndef BSP_H
#define CODE   __code
#define XDATA  __xdata
#endif

#define FAR
#define NOP()  asm("NOP")

#define HAL_MCU_LITTLE_ENDIAN()   __LITTLE_ENDIAN__
#define HAL_ISR_FUNC_DECLARATION(f,v)   \
    _PRAGMA(vector=v) __near_func __interrupt void f(void)
#define HAL_ISR_FUNC_PROTOTYPE(f,v)     \
    _PRAGMA(vector=v) __near_func __interrupt void f(void)
#define HAL_ISR_FUNCTION(f,v)           \
    HAL_ISR_FUNC_PROTOTYPE(f,v); HAL_ISR_FUNC_DECLARATION(f,v)


/*******************************************************************************
* IAR ARM
*/
#elif defined __ICCARM__

#ifndef CODE
#define CODE
#endif
#ifndef XDATA
#define XDATA
#endif
#define FAR
#define NOP()  asm("NOP")

#define HAL_ISR_FUNC_DECLARATION(f,v)
#define HAL_ISR_FUNC_PROTOTYPE(f,v)
#define HAL_ISR_FUNCTION(f,v)

#else
#error "IAR: Unsupported architecture"
#endif


/*******************************************************************************
* KEIL 8051
*/
#elif defined __KEIL__
#include <intrins.h>
#define BIG_ENDIAN

#define CODE   code
#define XDATA  xdata
#define FAR
#define NOP()  _nop_()

#define HAL_ISR_FUNC_DECLARATION(f,v)   \
    void f(void) interrupt v
#define HAL_ISR_FUNC_PROTOTYPE(f,v)     \
    void f(void)
#define HAL_ISR_FUNCTION(f,v)           \
    HAL_ISR_FUNC_PROTOTYPE(f,v); HAL_ISR_FUNC_DECLARATION(f,v)

typedef unsigned short istate_t;

// Keil workaround
#define __code  code
#define __xdata xdata


/*******************************************************************************
* WIN32
*/
#elif defined WIN32

#define DESKTOP

#define CODE
#define XDATA
#include "windows.h"
#ifndef FAR
#define FAR far
#endif
#ifdef _MSC_VER
#pragma warning (disable :4761)
#pragma warning (disable :4100)
#endif


/*******************************************************************************
* LINUX
*/
#elif defined __linux

#define DESKTOP
#define LINUX

#define CODE
#define XDATA
#ifndef FAR
#define FAR far
#endif


/*******************************************************************************
* Code Composer Studio
*/
/*
#elif __TI_COMPILER_VERSION__
#define CODE
#define XDATA
#define FAR



*/

/*******************************************************************************
* Other compilers
*/
#else
//#error "Unsupported compiler"
#endif

#define NOP()  asm(" nop")

#define HAL_ISR_FUNC_DECLARATION(f,v)   \
    _PRAGMA(vector=v##_VECTOR) __interrupt void f(void)
#define HAL_ISR_FUNC_PROTOTYPE(f,v)     \
    _PRAGMA(vector=v##_VECTOR) __interrupt void f(void)
#define HAL_ISR_FUNCTION(f,v)           \
    HAL_ISR_FUNC_PROTOTYPE(f,v); HAL_ISR_FUNC_DECLARATION(f,v)
typedef unsigned short istate_t;
/*******************************************************************************
* Mark the end of the C bindings section for C++ compilers.
*******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif // #ifndef HAL_TYPES_H
