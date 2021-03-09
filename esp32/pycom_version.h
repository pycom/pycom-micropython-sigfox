/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef VERSION_H_
#define VERSION_H_

#define SW_VERSION_NUMBER                              "1.20.2.r4"

#define LORAWAN_VERSION_NUMBER                         "1.0.2"

#define SIGFOX_VERSION_NUMBER                          "1.0.1"

#if (VARIANT == PYBYTES)
#define PYBYTES_VERSION_NUMBER                         "1.6.1"
#endif

#ifdef PYGATE_ENABLED
#define PYGATE_VERSION_NUMBER                          "1.0.1"
#endif

#endif /* VERSION_H_ */
