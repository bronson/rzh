/* r2scan.h
 * Scott Bronson
 * 27 Dec 2004
 *
 * This part of support code to make writing re2c scanners much easier.
 */

/** @file r2scan.h
 *
 * This is the central file for the readers.  They provide data
 * for scanners.
 *
 * NOTES
 *
 * All charptrs in the scanstate structure are declared const to help
 * ensure that you don't
 * accidentally end up modifying the buffer as it's being scanned.
 * This means that your read routine must cast them to be mutable
 * (char*) before read them.
 *
 * TERMINOLOGY
 *
 * allocate: scanstates can be dynamically (dynamicscan_create()) or
 * statically (scanstate_init()) allocated.  The buffers they use to
 * hold data may also be either dynamic or static.  Of course, any
 * time you allocate something dynamically, you must call the
 * corresponding free routine when you're done with it.
 *
 * attach: when the scanner is first initialized (scanstate_init())
 * or allocated (dynamicscan_create()), it is blank.  Trying to
 * pass it to a scanner would result in an assert or a crash.
 * You need to first attach a READER to provide data.
 *
 * initialize: prepare an already-allocated scanner for use.
 * After initializing it, you must ATTACH the scanner to a
 * READER.
 *
 * reader: reads data into the scanstate for the scanner.
 * Examples are readmem.c (read from a contiguous block in
 * memory), readfp.c (read from a FILE*), readfd.c (read
 * from a Unix file descriptor), etc.
 *
 * scanner: the function that actually performs the scanning.
 * It may or may not be written with the assistance of re2c.
 * It accepts a scanstate data structure and returns the next
 * token in the stream.
 *
 * scanstate: the data structure that retains complete state for the
 * scanner.  Scanners are thread safe: they never, ever use global
 * state.
 */


#ifndef R2SCAN_H
#define R2SCAN_H


// for re2c...
#define YYCTYPE     char
#define YYCURSOR    (ss->cursor)
#define YYLIMIT     (ss->limit)
#define YYMARKER    (ss->marker)

// This routine needs to force a return if 0 bytes were read because
// otherwise we might end up scanning garbage waaay off the end of
// the buffer.  We ignore n because there can be cases where there
// are less than n bytes left in the file, but it's perfectly valid
// data and one or more tokens will match.  n is useless.
// We also don't want to return prematurely.  If there's still data
// in the buffer, even if the read returned 0, we'll continue parsing.
// But, if read is at eof and there's no data left in the buffer, then
// there's nothing to do BUT return 0.

#define YYFILL(n)   do { \
		int r = (*ss->read)(ss); \
		if(r <= 0 && (ss)->cursor >= (ss)->limit) { \
			return r; \
		} \
	} while(0);


// forward declaration
struct scanstate;


/** Prototype of read function
 *
 * You only need to know this if you're writing your own read functions.
 *
 * This function is used to fetch more data for the scanner.  It must
 * first shift the pointers in ss to make room (see read_shiftbuffer())
 * then load new data into the unused bytes at the end of the buffer.
 *
 * This routine returns 0 when there's no more data (EOF).
 * If it returns a value less than 0, that value will be returned
 * to the caller instead of a token.  This can indicate an error
 * condition, or just a situation such as EWOULDBLOCK.
 *
 * Because of the way re2c handles buffering, it's possible for the
 * read routine to be called multiple times after it has returned eof.
 * This isn't an error.  If your read routine is called when
 * ss->at_eof is true, you should just return without doing anything.
 *
 * All charptrs in the scanstate structure are declared const to help
 * ensure that you don't
 * accidentally end up modifying the buffer as it's being scanned.
 * This means that your read routine must cast them to be mutable
 * (char*) before reading them.  Only the readproc may modify the
 * data that's in the scan buffer.
 */

typedef int (*readproc)(struct scanstate *ss);


/** Prototype of scanner function
 *
 * A scanner is simply a function that accepts a scanstate
 * object and returns the next token in that stream.
 * The function will typically be generated with the
 * assistance of re2c, but it doesn't have to be!
 *
 * Once you have created the scanstate data structure,
 * pass it to the scanner.  If the scanner returns 0,
 * you hit EOF.  If the scanner returns a negative number,
 * then some sort of error was encountered.  Or, if you're
 * doing nonblocking I/O, then it just might mean that this
 * there's not enough data available to determine the next
 * token.
 */

typedef int (*scanproc)(struct scanstate *ss);



/** Represents the current state for a single scanner.
 *
 * This structure is used by a scanner to preserve its state.
 *
 * All charptrs are declared const to help ensure that you don't
 * accidentally end up modifying the buffer as it's being scanned.
 * This means that any time you want to read the buffer, you need
 * to cast the pointers to be nonconst.
 */

typedef struct scanstate {
    const char *cursor; ///< The current character being looked at by the scanner
    const char *limit;  ///< The last valid character in the current buffer.  If the previous read was short, then this will not point to the end of the actual buffer (bufptr + bufsiz).
    const char *marker; ///< Used internally by re2c engine to handle backtracking.

    // (these do a poor job of simulating capturing parens)
    const char *token;  ///< The start of the current token (manually updated by the scanner).
    int line;           ///< The scanner may or may not maintain the current line number in this field.
    int at_eof;         ///< You almost certainly don't want to be using this (unless you're writing a readproc).  Use scan_finished() instead.  This field gets set when the readproc realizes it has hit eof.  by convention 1 means eof, 2 means yes, at_eof is true, but because of an unrecoverable read error (todo: this should be formalized).

    const char *bufptr; ///< The buffer currently in use
    int bufsiz;         ///< The maximum number of bytes that the buffer can hold

    void *readref;      ///< Data specific to the reader (i.e. for readfp_attach() it's a FILE*).
    readproc read;      ///< The routine the scanner calls when the buffer needs to be reread.

    void *scanref;      ///< Data specific to the scanner
    scanproc state;     ///< some scanners are made up of multiple individual scan routines.  They store their state here.

    void *userref;      ///< Never touched by any r2 routines (except scanstate_init, which clears both fields).  This can be used to associate a parser with this scanner.
    void *userproc;     ///< That's just a suggestion though.  These fields are totally under your control.
} scanstate;


void scanstate_init(scanstate *ss, const char *bufptr, int bufsiz);
void scanstate_reset(scanstate *ss);


/** Returns true when there's no more data to be scanned.
 *
 * How what this macro does:
 *
 * If the reader has already marked the stream at eof, then we're finished.
 * Otherwise, if there's still more data in the buffer, then we're not
 * finished.  Finally, if there's no data in the buffer but we're not at
 * eof, then we need te execute a read to determine.  If the read doesn't
 * return any data, then we're finished.
 */

#define scan_finished(ss) \
    (((ss)->cursor < (ss)->limit) ? 0 : \
		 ((ss)->at_eof || ((*(ss)->read)(ss) <= 0)) \
    )


/** Fetches the next token in the stream from the scanner.
 */

#define scan_token(ss) ((*((ss)->state))(ss))


/** Returns a pointer to the first character of the
 *  most recently scanned token.
 */

#define token_start(ss) ((ss)->token)

/** Returns a pointer to one past the last character of the
 *  most recently scanned token.
 *
 *  token_end(ss) - token_start(ss) == token_length(ss)
 */

#define token_end(ss) ((ss)->cursor)

/** Returns the length of the most recently scanned token.
 */

#define token_length(ss) ((ss)->cursor - (ss)->token)


/** Pushes the current token back onto the stream
 *
 * Calling scan_pushback returns the scanner to the state it had
 * just before returning the current token.  If you decide that
 * you don't want to handle this token, you can push it back and
 * it will be returned again the next time scan_token() is called.
 *
 * Note that this only works once.  You cannot push multiple tokens back
 * into the scanner.  Also, the scanner may have internal state of its
 * own that does not get reset.  If so, the scanner may or may not provide
 * a routine to back its state up as well.
 *
 * Finally, this doesn't back the line number up.  If you're pushing
 * a token back and you care about having the correct line nubmer,
 * then you'll have to restore the line number to what it was before
 * you scanned the token that you're pushing back.
 *
 * Yes, it takes some pretty serious research to call this function safely.
 * However, when you need to, it can be amazingly useful.
 */

#define scan_pushback(ss) ((ss)->cursor = (ss)->token)


/** Sets the current line number in the scanner to the given value.
 */

#define set_line(ss,n) (ss->line=(n));


/** Increments the current line number by 1.
 */

#define inc_line(ss)   (ss->line++);


/** This should be called by ever scanproc
 *
 * This prepares the scanstate to scan a new token.
 */

#define scanner_enter(ss) ((ss)->token = (ss)->cursor)

#endif

