/* scan.h
 * 15 Jan 2004
 * Scott Bronson
 *
 * Watches for a zmodem start request.
 *
 * This file is released under the MIT license.  This is basically the
 * same as public domain, but absolves the author of liability.
 */

// called when scan notices that a transfer has been started.
// implemented in another file.
extern void receive_file();

int scan_for_zrqinit();

