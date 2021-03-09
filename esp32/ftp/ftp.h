/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef FTP_H_
#define FTP_H_

extern void stoupper (char *str);

/******************************************************************************
 DECLARE EXPORTED FUNCTIONS
 ******************************************************************************/
extern void ftp_init (void);
extern void ftp_run (void);
extern void ftp_enable (void);
extern void ftp_disable (void);
extern void ftp_reset (void);

#endif /* FTP_H_ */
