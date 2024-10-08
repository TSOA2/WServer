#ifndef _RESOURCE_HEADER_GUARD
#define _RESOURCE_HEADER_GUARD

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>

#include <limits.h>
#include <fts.h>

#include <errno.h>

#include <http.h>

typedef struct {
	int fd;
	AcceptType type;
} Resource;

/*
 * Initialize the resource system
 */
int resource_init(void);

/*
 * List all resources
 */
void resource_list(void);

/*
 * Get a resource
 */
Resource *resource_get(uint8_t *, uint8_t);

/*
 * Destroy the resource system
 */
void resource_destroy(void);

#endif // _RESOURCE_HEADER_GUARD
