/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef TELNET_H_
#define TELNET_H_

/******************************************************************************
 DECLARE EXPORTED FUNCTIONS
 ******************************************************************************/
extern void telnet_init (void);
extern void telnet_run (void);
extern void telnet_tx_strn (const char *str, int len);
extern bool telnet_rx_any (void);
extern int  telnet_rx_char (void);
extern void telnet_enable (void);
extern void telnet_disable (void);
extern void telnet_reset (void);

#endif /* TELNET_H_ */
