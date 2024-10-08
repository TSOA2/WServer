#include <resource.h>
#include <config.h>
#include <log.h>

typedef struct ResourceLL {
	char *path;
	Resource resource;
	struct ResourceLL *next;
} ResourceLL;

static FTS *resource_root;
static ResourceLL *resource_start;

static inline ResourceLL *allocate_resource_ll(void)
{
	ResourceLL *new_resource;
	if (!(new_resource = calloc(1, sizeof(ResourceLL)))) {
		log_error("ran out of memory for resources!\n");
		return NULL;
	}

	new_resource->resource.fd = -1;

	return new_resource;
}

static const struct file_ext {
	const char *s;
	AcceptType t;
} file_extensions[] = {
	{"txt",   ACCTYPE_TEXT_PLAIN},
	{"html",  ACCTYPE_TEXT_HTML},
	{"htm",   ACCTYPE_TEXT_HTML},
	{"css",   ACCTYPE_TEXT_CSS},
	{"js",    ACCTYPE_TEXT_JAVASCRIPT},
	{"xml",   ACCTYPE_TEXT_XML},

	{"jpeg",  ACCTYPE_IMAGE_JPEG},
	{"jpg",   ACCTYPE_IMAGE_JPEG},
	{"png",   ACCTYPE_IMAGE_PNG},
};

#define FILE_EXT_NUM (sizeof(file_extensions) / sizeof(struct file_ext))

static inline AcceptType get_file_type(char *name)
{
	char *file_ext = name;
	for (; *file_ext != '\0'; file_ext++);
	for (; file_ext != name && *file_ext != '.'; file_ext--);
	if (file_ext == name) {
		// Ex. "test"
		// would be considered plain text
		return ACCTYPE_TEXT_PLAIN;
	} else {
		file_ext++;
		for (size_t i = 0; i < FILE_EXT_NUM; i++) {
			if (strcmp(file_extensions[i].s, file_ext) == 0)
				return file_extensions[i].t;
		}
	}

	// TODO: uhh fix this
	return ACCTYPE_TEXT_PLAIN;
}

int resource_init(void)
{
	char *root_path[] = {".", NULL};
	int fts_opts = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;

	resource_root = fts_open((char * const *) root_path, fts_opts, NULL);
	if (!resource_root) {
		log_error("failed to load resources: fts_open(): %s\n", strerror(errno));
		return -1;
	}

	if (!fts_children(resource_root, 0))
		return 0;

	resource_start = allocate_resource_ll();
	if (!resource_start)
		return -1;

	ResourceLL **chosen = &resource_start;
				
	FTSENT *idx;
	while ((idx = fts_read(resource_root))) {
		if (idx->fts_info == FTS_F) {
			if (!(*chosen)) {
				*chosen = allocate_resource_ll();
				if (!(*chosen))
					continue;
			}

			/*
			 * Skip the + 1 on pathlen because
			 * we're skipping the preceding dot of the path.
			 *
			 * for example:
			 * ./test
			 * would be
			 * /test
			 */
			char *path = calloc(idx->fts_pathlen, 1);
			if (!path) {
				free(*chosen);
				*chosen = NULL;
				continue;
			}

			(void) strcpy(path, idx->fts_path + 1);

			(*chosen)->path = path;
			(*chosen)->resource.fd = open(idx->fts_path, O_RDONLY);
			if ((*chosen)->resource.fd < 0) {
				log_error("open(%s) failed: %s\n", path, strerror(errno));
				free(path);
				free(*chosen);
				*chosen = NULL;
				continue;
			}
			(*chosen)->resource.type = get_file_type(path);

			chosen = &(*chosen)->next;
		}
	}

	return 0;
}

void resource_list(void)
{
	struct ResourceLL *idx;
	for (idx = resource_start; idx != NULL; idx = idx->next)
		log_write("resource: %s\n", idx->path);
}

static int resource_cmp(ResourceLL *resource, uint8_t *path, uint8_t len)
{
	char *rpath = resource->path;
	uint8_t pcount = 0;
	while (*rpath != '\0' && pcount < len && *path != '?') {
		if (*rpath != *path)
			return 0;
		rpath++;
		pcount++;
		path++;
	}

	if (*rpath == '\0' && (pcount == len || *path == '?'))
		return 1;
	return 0;
}

Resource *resource_get(uint8_t *path, uint8_t len)
{
	struct ResourceLL *idx = resource_start;
	while (idx) {
		if (resource_cmp(idx, path, len))
			return &idx->resource;

		idx = idx->next;
	}

	return NULL;
}

void resource_destroy(void)
{
	if (!resource_start) return;

	struct ResourceLL *tmp;
	while (resource_start) {
		tmp = resource_start->next;
		if (resource_start->path)
			free(resource_start->path);
		if (resource_start->resource.fd >= 0)
			close(resource_start->resource.fd);
		free(resource_start);
		resource_start = tmp;
	}

	fts_close(resource_root);
}

