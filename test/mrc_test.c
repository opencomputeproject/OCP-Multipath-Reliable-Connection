
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>

#include "mrc.h"

#define CHK_ERR(rc, fmt, ...)                                      \
	do {                                                       \
		if (rc) {                                          \
			printf("ERROR: " fmt "\n", ##__VA_ARGS__); \
			exit(1);                                   \
		}                                                  \
	} while(0)

/* application state context data */
typedef struct {
	struct ibv_context *ib_ctx;
	struct ibv_port_attr port_attr;
	struct ibv_pd *pd;

	int ib_port;
	int gid;
} ctx_t;

void open_device(ctx_t *ctx, char *ib_dev_name)
{
	struct ibv_device **dev_list = NULL;
	struct ibv_device *ib_dev = NULL;
	int i, rc;

	ctx->ib_ctx = NULL;

	dev_list = ibv_get_device_list(NULL);
	CHK_ERR(!dev_list, "Failed to get device list");

	for (i = 0; dev_list[i]; i++) {
		if (!strcmp(ibv_get_device_name(dev_list[i]), ib_dev_name)) {
			ib_dev = dev_list[i];
			break;
		}
	}

	if (!ib_dev) {
		ibv_free_device_list(dev_list);
		CHK_ERR(1, "Failed to find device %s", ib_dev_name);
	}

	ctx->ib_ctx = ibv_open_device(ib_dev);
	CHK_ERR(!ctx->ib_ctx, "Failed to open device %s", ib_dev_name);

	rc = ibv_query_port(ctx->ib_ctx, ctx->ib_port, &ctx->port_attr);
	CHK_ERR(rc, "Failed to query port");

	ibv_free_device_list(dev_list);
}

void close_device(ctx_t *ctx)
{
	if (ctx->ib_ctx) {
		ibv_close_device(ctx->ib_ctx);
		ctx->ib_ctx = NULL;
	}
}

void create_pd(ctx_t *ctx)
{
	ctx->pd = ibv_alloc_pd(ctx->ib_ctx);
	CHK_ERR(!ctx->pd, "Failed to alloc PD");
}

void destroy_pd(ctx_t *ctx)
{
	if (ctx->pd) {
		ibv_dealloc_pd(ctx->pd);
		ctx->pd = NULL;
	}
}

static void usage(char *name)
{
	printf("%s [args...] -d <device>\n", basename(name));
	printf("  -d device   IB device to open\n");
	printf("  -x gid      address gid to use (default=0)\n");
}

int main(int argc, char **argv)
{
	char *ib_dev_name = NULL;
	ctx_t *ctx = NULL;
	int op;

	ctx = calloc(1, sizeof(*ctx));
	CHK_ERR(!ctx, "Failed to alloc ctx");

	ctx->ib_port = 1; /* XXX static for now */
	ctx->gid = 0;

	while ((op = getopt(argc, argv, "d:x:")) != -1) {
		switch (op) {
		case 'd':
			ib_dev_name = optarg;
			break;
		case 'x':
			ctx->gid = atoi(optarg);
			break;
		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	CHK_ERR(!ib_dev_name, "Must specify a device name");

	/* find and open the device for communication */
	open_device(ctx, ib_dev_name);
	create_pd(ctx);

	printf("Hello MRC!\n");
	/* XXX Do MRC stuff here! :-) */

	/* destroy resources */
	destroy_pd(ctx);
	close_device(ctx);
	free(ctx);

	return 0;
}

