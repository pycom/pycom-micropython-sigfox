/*!
 * \file sigfox_types.h
 * \brief Sigfox types definition
 * \author $(SIGFOX_LIB_AUTHOR)
 * \version $(SIGFOX_LIB_VERSION)
 * \date $(SIGFOX_LIB_DATE)
 * \copyright Copyright (c) 2011-2015 SIGFOX, All Rights Reserved. This is unpublished proprietary source code of SIGFOX.
 *
 */

#ifndef SIGFOX_TYPES_H
#define SIGFOX_TYPES_H

/****************************************************/
/*!
 * \defgroup SIGFOX_TYPES Custom types used in library
 *
 *  @{
 */
/* Unsigned Type*/
typedef unsigned char       sfx_u8;
typedef unsigned short      sfx_u16;
typedef unsigned long       sfx_u32;
typedef unsigned char       sfx_bool;
/* Signed Type */
typedef signed char         sfx_s8;
typedef signed short        sfx_s16;
typedef signed long         sfx_s32;
/* Custom Types */
typedef unsigned short      sfx_error_t;

#define SFX_NULL            (void*)0
#define SFX_TRUE            1
#define SFX_FALSE           0

#define SFX_U8_MIN          0U
#define SFX_U8_MAX          0xFFU
#define SFX_U16_MIN         0U
#define SFX_U16_MAX         0xFFFFU
#define SFX_U32_MIN         0UL
#define SFX_U32_MAX         0xFFFFFFFFUL

#define SFX_S8_MIN          0x80
#define SFX_S8_MAX          0x7F
#define SFX_S16_MIN         0x8000
#define SFX_S16_MAX         0x7FFF
#define SFX_S32_MIN         0x80000000L
#define SFX_S32_MAX         0x7FFFFFFFL

/** @}*/
/****************************************************/

#endif /* SIGFOX_TYPES_H */
