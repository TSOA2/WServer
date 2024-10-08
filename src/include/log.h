#ifndef _LOG_HEADER_GUARD
#define _LOG_HEADER_GUARD

#define DEFAULT_LOG_FHANDLE stdout

/*
 * Initialize the logging system.
 * This is based upon the WSERVER_LOG_FILE macro in config.h
 */
void log_init(void);

/*
 * Outputs formatted text to the log file, with prefixed time.
 */
void log_write(const char *, ...);

/*
 * Outputs formatted text to the log file, without prefixed time.
 */
void log_write_notime(const char *, ...);

/*
 * Outputs formatted text to the log file, with prefixed time and error.
 */
void log_error(const char *, ...);

/*
 * Destroys the logging system.
 */
void log_destroy(void);

struct HttpRequest;

/*
 * Log a selection of bytes from a buffer.
 */
void log_bytes(uint8_t *, uint32_t);

/*
 * Safely logs an http request for debugging purposes.
 */
void log_http_req(HttpRequest *);

#endif // _LOG_HEADER_GUARD
