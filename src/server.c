#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <signal.h>

#if defined(__FreeBSD__) || defined(__NetBSD__) || \
	defined(__OpenBSD__) || defined(__DragonFly__) || \
	(defined(__APPLE__) && defined(__MACH__))

#define WSERVER_USE_KQUEUE (1)

#else

#error "WServer currently only supports the KQUEUE event system."

#endif

#if WSERVER_USE_KQUEUE
#include <sys/event.h>
#endif


#include <http.h>
#include <log.h>
#include <resource.h>
#include <config.h>

/* Listening socket */
static int wserver_lsocket = -1;

/* Event file descriptor */
static int wserver_efd     = -1;

/*
 * ASCII art from patorjk.com
 * Font authors listed on website
 * Font: Ivrit
 */
static const char *wserver_title_text =
" __        __ ____   _____  ____ __     __ _____  ____\n"
" \\ \\      / // ___| | ____||  _ \\\\ \\   / /| ____||  _ \\\n"
"  \\ \\ /\\ / / \\___ \\ |  _|  | |_) |\\ \\ / / |  _|  | |_) |\n"
"   \\ V  V /   ___) || |___ |  _ <  \\ V /  | |___ |  _ <\n"
"    \\_/\\_/   |____/ |_____||_| \\_\\  \\_/   |_____||_| \\_\\\n"
"\tIt's a web server!\n";

/*
 * Set the socket options for the listening socket.
 */
static inline int lsocket_set_opts(void)
{
	if (setsockopt(
			wserver_lsocket,
			SOL_SOCKET,
			SO_REUSEADDR,
			&(int){1},
			sizeof(int)) < 0) {
		log_error("setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
		return -1;
	}

	if (setsockopt(
			wserver_lsocket,
			SOL_SOCKET,
			SO_REUSEPORT,
			&(int){1},
			sizeof(int)) < 0) {
		log_error("setsockopt(SO_REUSEPORT) failed: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

/*
 * Initialize the listening socket based on configuration.
 */
static inline int lsocket_init(void)
{
	struct addrinfo hint;
	struct addrinfo *ll, *start;
	int error;

	memset(&hint, 0, sizeof(hint));
	hint.ai_flags    = AI_PASSIVE;
	hint.ai_family   = AF_INET;
	hint.ai_socktype = SOCK_STREAM;

	if ((error = getaddrinfo(NULL, WSERVER_PORT, &hint, &start)) != 0) {
		log_error("getaddrinfo() failed: %s\n", gai_strerror(error));
		return -1;
	}

	for (ll = start; ll != NULL; ll = ll->ai_next) {
		wserver_lsocket = socket(
			ll->ai_family,
			ll->ai_socktype,
			ll->ai_protocol
		);
		if (wserver_lsocket < 0) {
			log_write("socket() failed: %s\n", strerror(errno));
			continue;
		}

		if (lsocket_set_opts() < 0) {
			log_write("setsocketopt()'s failed: %s\n", strerror(errno));
			close(wserver_lsocket);
			continue;
		}

		if (bind(wserver_lsocket, ll->ai_addr, ll->ai_addrlen) < 0) {
			log_write("bind() failed: %s\n", strerror(errno));
			close(wserver_lsocket);
			continue;
		}

		break;
	}

	freeaddrinfo(start);

	if (ll == NULL) {
		log_error("failed to find socket.\n");
		wserver_lsocket = -1;
		return -1;
	}


	if (listen(wserver_lsocket, WSERVER_MAX_CON) < 0) {
		log_error("listen() failed: %s\n", strerror(errno));
		close(wserver_lsocket);
		wserver_lsocket = -1;
		return -1;
	}

	log_write("Successfully made listening socket.\n");
	return 0;
}

/*
 * Initialize the event system.
 */
static inline int event_init(void)
{
	wserver_efd = kqueue();
	if (wserver_efd < 0) {
		log_error("kqueue() failed: %s\n", strerror(errno));
		return -1;
	}

	const struct kevent add_event = {
		.ident  = wserver_lsocket,
		.filter = EVFILT_READ,
		.flags  = EV_ADD,
		.fflags = 0,
		.data   = 0,
		.udata  = NULL
	};
	if (kevent(wserver_efd, &add_event, 1, NULL, 0, NULL) < 0) {
		log_error("kevent() failed, unable to add listening socket to queue.\n");
		close(wserver_efd);
		return -1;
	}

	return 0;
}

/*
 * Read a buffer from a resource
 */
static inline void *read_resource(Resource *resource, size_t *size)
{
	int rfd = resource->fd;
	if (rfd < 0)
		return NULL;

	struct stat s;
	if (fstat(rfd, &s) < 0)
		return NULL;

	*size = s.st_size;

	void *buf = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, rfd, 0);
	if (buf == MAP_FAILED)
		return NULL;
	return buf;
}

/*
 * Answer a request on a connection
 */
static void answer_request(int asocket, HttpRequest *req)
{
	if (!req) return;

	if (req->method == HTTP_GET) {
		int status = 200;
		Resource *resource = NULL;
		uint8_t *buf = NULL;
		size_t content_len = 0;

		if (req->parser_status) {
			status = req->parser_status;
		} else {
			resource = resource_get(req->path, req->path_len);
			if (resource) {
				buf = read_resource(resource, &content_len);
			} else {
				content_len = 0;
				status = 404;
			}
		}

		const char *status_msg = http_status_msg(status);
		size_t status_len = strlen(status_msg);

		const char headers[] =
			"HTTP/1.1 %s\r\n"
			"Content-Length: %lu\r\n"
			"Connection: Keep-Alive\r\n"
			"Server: WServer\r\n"
			"\r\n"
		;

		char response[status_len + sizeof(headers) + 40];

		(void) sprintf(
			response,
			headers,
			status_msg,
			content_len
		);

		(void) send(asocket, response, strlen(response), 0);
		(void) send(asocket, buf, content_len, 0);

		(void) munmap(buf, content_len);
	}
}

/*
 * Read a request into a buffer from a socket connection.
 */
static int read_request_buf(int asocket, HttpRequest *req)
{
	if (asocket < 0 || !req)
		return -1;

	HttpBuffer *buf = &req->buf;
	const size_t block_size = (1000);
	const char *no_more_mem = "Ran out of memory. Unable to allocate request.\n";

	if (!buf->buf) {
		buf->buf = malloc(block_size);
		if (!buf->buf) {
			log_error(no_more_mem);
			return -1;
		}
		buf->size = block_size;
	}

	uint32_t bytes_left = buf->size - buf->used;
	if (bytes_left == 0) {
		buf->size += block_size;
		uint8_t *realloc_buf = realloc(buf->buf, buf->size);
		if (!realloc_buf) {
			log_error(no_more_mem);
			return -1;
		}
		buf->buf = realloc_buf;
		bytes_left = block_size;
	}

	uint8_t *end = buf->buf + buf->used;

	ssize_t bytes_recvd;
	switch ((bytes_recvd = recv(asocket, end, bytes_left, 0))) {
		case 0:
		case -1:
			return -1;
		default:
			buf->used += bytes_recvd;
	}

	if (buf->used >= (WSERVER_MAX_BUF * block_size))
		buf->progress = 1;

	return 0;
}

static inline int make_nonblock(int asocket)
{
	int fl = fcntl(asocket, F_GETFL, 0);
	if (fl < 0) return fl;
	if (fcntl(asocket, F_SETFL, fl | O_NONBLOCK) < 0)
		return fl;
	return 0;
}

/*
 * The main event loop of the server.
 */
static inline void lsocket_mainloop(void)
{
	if (event_init() < 0)
		return;

	struct kevent events[WSERVER_MAX_CON];

	for ( ;; ) {
		int new_events;
		new_events = kevent(wserver_efd, NULL, 0, events, WSERVER_MAX_CON, NULL);
		if (new_events < 0) {
			log_error("failed to get new events: kevent(): %s", strerror(errno));
			continue;
		}

		for (int i = 0; i < new_events; i++) {
			int selected_socket = events[i].ident;

			if (selected_socket == wserver_lsocket) {
				struct sockaddr sa;
				socklen_t sa_len;
				int asocket;

				asocket = accept(wserver_lsocket, &sa, &sa_len);
				if (asocket < 0) {
					log_error("accept() failed: %s\n", strerror(errno));
					continue;
				}

				if (make_nonblock(asocket) < 0) {
					close(asocket);
					continue;
				}

				HttpRequest *request = http_alloc_req();
				if (!request) {
					log_error("http_alloc_req() failed\n");
					close(asocket);
					continue;
				}

				struct kevent add_event;
				EV_SET(&add_event, asocket, EVFILT_READ, EV_ADD, 0, 0, request);
				if (kevent(wserver_efd, (const struct kevent *) &add_event, 1, NULL, 0, NULL) < 0) {
					log_error("kevent() failed\n");
					http_free_req(request);
					close(asocket);
					continue;
				}
			} else {
				if (events[i].flags & EV_ERROR) continue;

				switch (events[i].filter) {
					case EVFILT_READ:; {
						HttpRequest *request = (HttpRequest *) events[i].udata;
						int err_status;

						if (read_request_buf(selected_socket, request) < 0)
							goto read_error;

						if ((err_status = http_check_done(request))) {
							log_write("http_check_done() returned status code %d\n", err_status);
							request->parser_status = err_status;
						}

						if (request->buf.progress) {
							events[i].filter = EVFILT_WRITE;
							events[i].flags  = EV_ADD;
							if (kevent(wserver_efd, (const struct kevent *) &events[i], 1, NULL, 0, NULL) < 0)
								goto read_error;
						}

						break;
					read_error:
						http_reset_req(request);
						http_free_req(request);
						close(selected_socket);
						break;
					}
					case EVFILT_WRITE:; {
						HttpRequest *request = (HttpRequest *) events[i].udata;
						answer_request(selected_socket, request);

						http_reset_req(request);

						events[i].filter = EVFILT_READ;
						events[i].flags  = EV_ADD;
						if (kevent(wserver_efd, (const struct kevent *) &events[i], 1, NULL, 0, NULL) < 0) {
							http_free_req(request);
							close(selected_socket);
						}
						break;
					}
				}
			}
		}
	}
}

/*
 * Cleanup the listening socket.
 */
static void lsocket_destroy(void)
{
	if (wserver_lsocket != -1)
		close(wserver_lsocket);
	wserver_lsocket = -1;
	log_write("Successfully destroyed listening socket.\n");
}

static void general_cleanup(void)
{
	lsocket_destroy();
	resource_destroy();
	log_destroy();
}

int main(int argc, char **argv)
{
	(void) argc; (void) argv;

	atexit(general_cleanup);

	log_init();

	resource_init();

	log_write_notime("%s\n", wserver_title_text);
	log_write("Starting...\n");

	if (lsocket_init() < 0)
		return -1;

	lsocket_mainloop();
	return 0;
}
