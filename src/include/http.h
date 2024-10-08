#ifndef _HTTP_HEADER_GUARD
#define _HTTP_HEADER_GUARD

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdint.h>

typedef enum {
	HTTP_NONE,
	HTTP_GET,
	HTTP_PUT,
	HTTP_POST,
	HTTP_HEAD,
	HTTP_DELETE,
	HTTP_OPTIONS,
	HTTP_TRACE,
} HttpMethod;

typedef struct {
	uint8_t *buf;
	uint32_t used;
	uint32_t size;

	/* 1 for finished (ready to parse), 0 for still in progress) */
	uint8_t progress;
} HttpBuffer;

/*
 * The exhaustive list can be found here:
 * https://www.iana.org/assignments/media-types/media-types.xhtml#font
 *
 * But implementing all of those doesn't fit the goal of the project.
 * (what is the goal of this project?)
 */
typedef enum {
	ACCTYPE_TEXT_ALL,
	ACCTYPE_TEXT_PLAIN,
	ACCTYPE_TEXT_HTML,
	ACCTYPE_TEXT_CSS,
	ACCTYPE_TEXT_JAVASCRIPT,
	ACCTYPE_TEXT_XML,

	ACCTYPE_IMAGE_JPEG,
	ACCTYPE_IMAGE_PNG,
	ACCTYPE_LAST,
} AcceptType;
#define NUM_ACCEPT_TYPES (ACCTYPE_LAST - 1)


typedef struct {
	AcceptType types[NUM_ACCEPT_TYPES]; // in order of precedence
	uint8_t types_len;
} AcceptField;

typedef struct {
	HttpMethod method;
	HttpBuffer buf;

	/*
	 * Important! These buffers won't be valid anymore
	 * once the HttpBuffer is cleaned up.
	 */

	uint8_t *content;
	size_t content_len;

	uint8_t *path;
	uint8_t path_len;

	AcceptField accept_field;

	/*
	 * If an error was detected while parsing the HTTP
	 * request, this will hold the recommended status
	 * code to send to the client.
	 */
	int parser_status;
} HttpRequest;

/*
 * Get the corresponding status codde message for the status
 * code number.
 */
const char *http_status_msg(int);

/*
 * This function checks if the request is finished. While it is
 * checking this, it also parses the request.
 *
 * Returns 0 if the request is valid.
 * Returns an HTTP status code if it's not valid.
 *
 * It will set request->buf->progress to 1 if the request is finished.
 */
int http_check_done(HttpRequest *);

/*
 * Allocates an HttpRequest.
 */
#define http_alloc_req() calloc(1, sizeof(HttpRequest))

/*
 * Resets an HttpRequest:
 * deletes the buffer,
 * initializes to zero.
 */

#define http_reset_req(request) \
	do { \
		if (!(request)) return; \
		if ((request)->buf.buf) { \
			free((request)->buf.buf); \
			(request)->buf.buf = NULL; \
		} \
		(void) memset((request), 0, sizeof(HttpRequest)); \
	} while (0)

/*
 * Frees a request.
 */
#define http_free_req(request) free((request))

#endif // _HTTP_HEADER_GUARD
