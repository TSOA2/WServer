#include <http.h>
#include <log.h>
#include <config.h>

#define WS_LITTLE_ENDIAN (*(uint8_t*)&(uint16_t){1})

typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/*
 * Unportable and ugly macros?
 *
 * TODO: don't use __BYTE_ORDER__ find some other way
 */

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define COMPOSE2(c0, c1) \
	((u16)c1<<8  | (u16)c0<<0)

#define COMPOSE4(c0, c1, c2, c3) \
	((u32)c3<<24 | (u32)c2<<16 | (u32)c1<<8 | (u32)c0<<0) \

#define COMPOSE8(c0, c1, c2, c3, c4, c5, c6, c7) \
	((u64)c7<<56 | (u64)c6<<48 | (u64)c5<<40 | (u64)c4<<32 | \
	 (u64)c3<<24 | (u64)c2<<16 | (u64)c1<<8  | (u64)c0<<0)

#else
#define COMPOSE2(c0, c1) \
	((u16)c0<<8  | (u16)c1<<0)

#define COMPOSE4(c0, c1, c2, c3) \
	((u32)c0<<24 | (u32)c1<<16 | (u32)c2<<8 | (u32)c3<<0) \

#define COMPOSE8(c0, c1, c2, c3, c4, c5, c6, c7) \
	((u64)c0<<56 | (u64)c1<<48 | (u64)c2<<40 | (u64)c3<<32 | \
	 (u64)c4<<24 | (u64)c5<<16 | (u64)c6<<8  | (u64)c7<<0)
#endif

static const char *status_code_map[] = {
	[200] = "200 OK",
	[201] = "201 Created",
	[202] = "202 Accepted",
	[203] = "203 Non-Authoritative Information",
	[204] = "204 No Content",
	[205] = "205 Reset Content",
	[206] = "206 Partial Content",
	[300] = "300 Multiple Choices",
	[301] = "301 Moved Permanently",
	[302] = "302 Found",
	[303] = "303 See Other",
	[304] = "304 Not Modified",
	[305] = "305 Use Proxy",
	[307] = "307 Temporary Redirect",
	[400] = "400 Bad Request",
	[401] = "401 Unauthorized",
	[403] = "403 Forbidden",
	[404] = "404 Not Found",
	[405] = "405 Method Not Allowed",
	[406] = "406 Not Acceptable",
	[407] = "407 Proxy Authentication Required",
	[408] = "408 Request Timeout",
	[409] = "409 Conflict",
	[410] = "410 Gone",
	[411] = "411 Length Required",
	[412] = "412 Precondition Failed",
	[413] = "413 Request Entity Too Large",
	[414] = "414 Request-URI Too Long",
	[415] = "415 Unsupported Media Type",
	[416] = "416 Request Range Not Satisfiable",
	[417] = "417 Expectation Failed",
	[500] = "500 Internal Server Error",
	[501] = "501 Not Implemented",
	[502] = "502 Bad Gateway",
	[503] = "503 Service Unavailable",
	[504] = "504 Gateway Timeout",
	[505] = "505 HTTP Version Not Supported",
};

const char *http_status_msg(int status_code)
{
	return status_code_map[status_code];
}

static inline int cmp_field_str(
	uint8_t **buf_idx,
	uint8_t *const buf_end,
	const uint8_t *lit
)
{
	for (; *lit != '\0' && *buf_idx != buf_end; lit++, (*buf_idx)++) {
		if (**buf_idx != *lit)
			return 0;
	}
	if (*buf_idx == buf_end)
		return 0;

	return 1;
}

#define EXPECT(b, e, c, s, o) \
	do { \
		if (*(b)++ != (c)) return (s); \
		if ((b) == (e)) return (o); \
	} while (0)

/*
 * FIELDS
 *
 * Return -1 if the buffer is incomplete
 * Return 0  if everything went smoothly / not the correct field
 * Return n  HTTP status code error
 */

static inline int field_content_length(
	HttpRequest *request,
	uint8_t **buf_idx,
	uint8_t *const buf_end
)
{
	const uint8_t lit[] = "Content-Length";
	if (!cmp_field_str(buf_idx, buf_end, (const uint8_t *) lit))
		return 0;

	EXPECT(*buf_idx, buf_end, ':', 400, -1);
	EXPECT(*buf_idx, buf_end, ' ', 400, -1);

	request->content_len = 0;
	if (isdigit(**buf_idx)) {
		while (*buf_idx != buf_end && isdigit(**buf_idx)) {
			request->content_len *= 10;
			request->content_len += **buf_idx - '0';
			(*buf_idx)++;
		}
	} else {
		return 400;
	}

	if (*buf_idx == buf_end) {
		request->content_len = 0;
		return -1;
	}

	if (**buf_idx != '\r')
		return 400;
	return 0;
}

static int skip_line_end(uint8_t **buf_idx, uint8_t *const buf_end)
{
	for (; *buf_idx != buf_end && **buf_idx != '\r'; (*buf_idx)++);

	if (*buf_idx == buf_end)
		return -1;

	if (buf_end - (*buf_idx) >= 2) {
		if (*(u16*)(*buf_idx) != COMPOSE2('\r','\n'))
			return 400;
	}

	*buf_idx += 2;
	return 0;
}

static inline int get_request_len(HttpRequest *request, uint8_t **header_end)
{
	HttpBuffer *buf = &request->buf;
	uint8_t *buf_idx = buf->buf;
	uint8_t * const buf_end = buf->buf + buf->used;

	if (request->content_len) {
		for (; buf_idx != buf_end; buf_idx++) {
			if (*buf_idx == '\r' && (buf_end - buf_idx >= 4)) {
				if (*(u32*)buf_idx == COMPOSE4('\r','\n','\r','\n')) {
					*header_end = buf_idx + 4;
					return 0;
				}
			}
		}
		if (buf_idx == buf_end)
			return -1;
	}

	/* Skip the request line */
	int ret;
	if ((ret = skip_line_end(&buf_idx, buf_end)))
		return ret;

	for (;;) {
		if ((ret = field_content_length(request, &buf_idx, buf_end)))
			return ret;

		if ((ret = skip_line_end(&buf_idx, buf_end)))
			return ret;

		if (buf_end - buf_idx >= 2) {
			if (*(u16*)buf_idx == COMPOSE2('\r','\n')) {
				*header_end = buf_idx + 2;
				break;
			}
		}
	}

	return 0;
}

/*
 * This function checks if the request is finished. While it is
 * checking this, it also parses the request.
 *
 * Returns 0 if the request is valid.
 * Returns an HTTP status code if it's not valid.
 *
 * It will set request->buf->progress to 1 if the request is finished.
 */
int http_check_done(HttpRequest *request)
{
	if (!request)
		return 500;

	HttpBuffer *buf = &request->buf;
	uint8_t *buf_idx = buf->buf;
	uint8_t * const buf_end = buf->buf + buf->used;

	if (request->method == HTTP_NONE) {
		if (buf->used > 4) {
			switch (*(u32 *) buf_idx) {
				case COMPOSE4('G','E','T',' '):
					request->method = HTTP_GET; buf_idx += 3; break;
				case COMPOSE4('P','U','T',' '):
					request->method = HTTP_PUT; buf_idx += 3; break;
				case COMPOSE4('P','O','S','T'):
					request->method = HTTP_POST; buf_idx += 4; break;
				case COMPOSE4('H','E','A','D'):
					request->method = HTTP_HEAD; buf_idx += 4; break;
				case COMPOSE4('D','E','L','E'):
					if (*(u16 *) (buf_idx + 4) == COMPOSE2('T','E')) {
						request->method = HTTP_DELETE;
						buf_idx += 6;
						break;
					} else {
						return 400;
					}
				case COMPOSE4('T','R','A','C'):
					if (*(buf_idx + 4) == 'E') {
						request->method = HTTP_TRACE;
						buf_idx += 5;
						break;
					} else {
						return 400;
					}
				default:
					if (*(u64 *) buf_idx ==
						COMPOSE8('O','P','T','I','O','N','S',' ')) {
						request->method = HTTP_OPTIONS;
						buf_idx += 7;
						break;
					} else {
						return 400;
					}

			}
		} else {
			goto request_not_finished;
		}
	}

	if (!request->path || request->path_len == 0) {
		if (*buf_idx++ != ' ')
			return 400;

		request->path = buf_idx;
		for (; (*buf_idx > 32 && *buf_idx < 127)
				&& (buf_idx != buf_end); buf_idx++, request->path_len++);

		if (buf_idx == buf_end)
			goto request_not_finished;

		if (!(*buf_idx > 32 || *buf_idx < 127))
			return 400;

		if (*buf_idx++ != ' ')
			return 400;
	}

	uint8_t *header_end = NULL;
	int ret;
	switch ((ret = get_request_len(request, &header_end))) {
		case 0: break;
		case -1: goto request_not_finished;
		default:
			return ret;
	}

	/*
	 * If the request uses a GET method, it's simple: just check for
	 * two consecutive CRLFs.
	 *
	 * However, if it's a POST (or some method that has a body), then
	 * check for the Content-Length.
	 */

	int request_is_done = 0;
	if (request->method == HTTP_GET) {
		if (header_end)
			request_is_done = 1;
	}

	request->content = header_end;

	if (request->method == HTTP_POST) {
		if (request->content_len) {
			size_t actual_len = ((size_t) buf_end) - ((size_t) request->content);
			request_is_done = 1;
			if (actual_len < request->content_len)
				goto request_not_finished;
			else if (request->content_len > actual_len)
				return 400;
		} else {
			/* The client doesn't tell us how long the body is */
			return 411;
		}
	}

	/* Do other methods later, just want to get this working. */

	request->buf.progress = request_is_done;
	return 0;

request_not_finished:
	if (!buf->progress)
		return 400;
	else
		buf->progress = 0;
	return 0;
}
