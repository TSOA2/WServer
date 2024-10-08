#include <http.h>
#include <config.h>
#include <log.h>

#if WSERVER_ENABLE_LOG
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <errno.h>
#endif

#if WSERVER_ENABLE_LOG
#define MAYBE_UNUSED
static FILE *wserver_log_file;
#else
#define MAYBE_UNUSED __attribute__((unused))
#endif

void log_init(void)
{
#if WSERVER_ENABLE_LOG
#ifdef WSERVER_LOG_FILE
	wserver_log_file = fopen(WSERVER_LOG_FILE, "w");
	if (!wserver_log_file) {
		fprintf(stderr,
			"failed to open log file (" WSERVER_LOG_FILE "): %s\n",
			strerror(errno)
		);
		wserver_log_file = DEFAULT_LOG_FHANDLE;
	}
#else
	wserver_log_file = DEFAULT_LOG_FHANDLE;
#endif
#endif
}

#if WSERVER_ENABLE_LOG
static void log_fmt_time(void)
{
	struct timeval tv;
	struct tm *tm_val;

	gettimeofday(&tv, NULL);
	tm_val = localtime(&tv.tv_sec);
	if (tm_val) {
		char buffer[64];
		memset(buffer, 0, sizeof(buffer));
		strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_val);
		fprintf(wserver_log_file, "%s.%06d", buffer, tv.tv_usec);
	} else {
		fprintf(wserver_log_file, "[UNKNOWN TIME]");
	}

	fprintf(wserver_log_file, ": ");
}
#endif

void log_write(MAYBE_UNUSED const char *fmt, ...)
{
#if WSERVER_ENABLE_LOG
	va_list args;
	va_start(args, fmt);

	log_fmt_time();
	vfprintf(wserver_log_file, fmt, args);

	fflush(wserver_log_file);

	va_end(args);
#endif
}

void log_write_notime(MAYBE_UNUSED const char *fmt, ...)
{
#if WSERVER_ENABLE_LOG
	va_list args;
	va_start(args, fmt);

	vfprintf(wserver_log_file, fmt, args);
	fflush(wserver_log_file);

	va_end(args);
#endif
}

void log_error(MAYBE_UNUSED const char *fmt, ...)
{
#if WSERVER_ENABLE_LOG
	va_list args;
	va_start(args, fmt);

	log_fmt_time();
	fprintf(wserver_log_file, "[ERROR]: ");
	vfprintf(wserver_log_file, fmt, args);

	fflush(wserver_log_file);

	va_end(args);
#endif
}

void log_destroy(void)
{
#if WSERVER_ENABLE_LOG
#ifdef WSERVER_LOG_FILE
	if (wserver_log_file)
		fclose(wserver_log_file);
#endif
	wserver_log_file = NULL;
#endif
}

#if WSERVER_ENABLE_LOG
const char *method_types[] = {
	[HTTP_NONE] = "None",
	[HTTP_GET]  = "GET",
	[HTTP_PUT]  = "PUT",
	[HTTP_POST] = "POST",
	[HTTP_HEAD] = "HEAD",
	[HTTP_DELETE] = "DELETE",
	[HTTP_OPTIONS] = "OPTIONS",
	[HTTP_TRACE] = "TRACE",
};
#endif

void log_bytes(MAYBE_UNUSED uint8_t *buf, MAYBE_UNUSED uint32_t byte_size)
{
#if WSERVER_ENABLE_LOG
	for (uint32_t i = 0; i < byte_size; i++)
		(void) fputc((int) buf[i], wserver_log_file);
	fflush(wserver_log_file);
#endif
}

void log_http_req(MAYBE_UNUSED HttpRequest *req)
{
#if WSERVER_ENABLE_LOG
	if (!req || !req->buf.buf) return;

	fprintf(wserver_log_file, 
			"[HTTP REQUEST] {\n"
				"\tMethod Type:  %s\n"
				"\tPath: ",
			method_types[req->method]
		);

	if (req->path)
		log_bytes(req->path, (uint32_t) req->path_len);

	fprintf(wserver_log_file,
				"\n"
				"\tBuffer address:  %lx\n"
				"\tUsed:            %u\n"
				"\tSize:            %u\n"
				"\tContent address: %lx\n"
				"\tContent len:     %lu\n"
				"\tFinished?        %s\n"
			"}\n",
			(uintptr_t) req->buf.buf,
			req->buf.used,
			req->buf.size,
			(uintptr_t) req->content,
			req->content_len,
			req->buf.progress ? "true" : "false"
		);

	log_bytes(req->buf.buf, req->buf.used);

	fprintf(wserver_log_file, "[END OF HTTP REQUEST]\n");

	fflush(wserver_log_file);
#endif
}
