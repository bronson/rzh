/* scan.h
 * Scott Bronson
 *
 * Watches for a zmodem start request.
 */

// called when scan notices that a transfer has been started.
// implemented in another file.
extern void receive_file();

int scan_for_zrqinit();

