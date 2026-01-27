
# Sending and Receiving EV Probes

This example demonstrates how a controller can use the APIs in `mrc_ctl.h` to
send and receive EV probes. EV probes allow the controller to measure
connectivity or path quality to a destination over a specified path.

## Find and Open a Device

An RDMA application must first find the device it wants to open and then
open a context against it.

```c
struct ibv_device  **dev_list;
struct ibv_context  *ibv_ctx = NULL;

/* get the list of RDMA devices */
dev_list = ibv_get_device_list(NULL);
if (!dev_list)
    return ERROR;

/*
 * This example opens the first available device, replace this as needed
 * to search for a specific device to open by name.
 */
ibv_ctx = ibv_open_device(dev_list[0]);
if (!ibv_ctx) {
    ibv_free_device_list(dev_list);
    return ERROR;
}

ibv_free_device_list(dev_list);
```

## Query the Device

Once the device is open, the controller can query the device to learn the
device's MRC controller capabilities. This includes available features, EV/CC
usage limits, and port/plane information. For EV probe support, the
`MRC_CTL_OPT_CAP_EP_OP_EV_PROBE` capability must be returned by the device.

```c
struct mrc_ctl_device_attr mrc_ctl_attr;

if (mrc_ctl_query_device(ibv_ctx, &mrc_ctl_attr) != 0)
    return ERROR;

if (!(mrc_ctl_attr.opt_attr & MRC_CTL_OPT_CAP_EP_OP_EV_PROBE))
    return ERROR; /* EV probes are not supported */
```

## Create an MRC Context

The next step is to create the MRC context which is the parent object used for
all MRC operations and it tracks resources allocated by the process.

```c
struct mrc_context_attr  mrc_ctx_attr;
struct mrc_context      *mrc_ctx;

mrc_ctx_attr = (struct mrc_context_attr){
    .mrc_api_version_used = MRC_API_CURRENT_VERSION,
};

mrc_ctx = mrc_create_context(ibv_ctx, &mrc_ctx_attr);
if (!mrc_ctx)
    return ERROR;
```

## Sending EV Probe Requests

In this example, RoCEv2 GIDs (IPv4) are used for the source and destination
addresses. 32b STEV values are used and the procedure is identical for SRv6
and SRv6+SRH paths.

```c
struct mrc_ctl_ep_req ep_reqs[2];
struct mrc_ctl_ep_rsp ep_rsps[2];
int i, num_rsps;
uint32_t req_ev_val;

memset(ep_reqs, 0, sizeof(ep_reqs));
memset(ep_rsqs, 0, sizeof(ep_rsqs));

/* Probe 1 */
ep_reqs[0].req_id = 1;
inet_pton(AF_INET6, "10.0.0.1", &ep_reqs[0].sgid);
inet_pton(AF_INET6, "10.0.0.2", &ep_reqs[0].dgid);
ep_reqs[0].ev_fmt_mode = MRC_CTL_EV_FMT_MODE_STEV;
req_ev_val = 0x12345678;
memcpy(ep_reqs[0].req_ev.val, &req_ev_val, sizeof(uint32_t));
ep_reqs[0].req_ev.port = 1;

/* Probe 2 */
ep_reqs[1].req_id = 2;
inet_pton(AF_INET6, "10.0.0.1", &ep_reqs[1].sgid);
inet_pton(AF_INET6, "10.0.0.3", &ep_reqs[1].dgid);
ep_reqs[1].ev_fmt_mode = MRC_CTL_EV_FMT_MODE_STEV;
req_ev_val = 0x87654321;
memcpy(ep_reqs[1].req_ev.val, &req_ev_val, sizeof(uint32_t));
ep_reqs[1].req_ev.port = 1;

/* Send probes with 2ms timeout */
if (mrc_ctl_ep_batch_send_wait(mrc_ctx, 0, MRC_CTL_EP_OP_EV_PROBE,
                               ep_reqs, 2, 2000000,
                               ep_rsps, &num_rsps) != 0)
    return ERROR;

/* Process responses */
for (i = 0; i < num_rsps; i++) {
    /* Match response to request using req_id */
    printf("Probe Response:\n");
    printf("  req_id:       %u\n",    ep_rsps[i].req_id);
    printf("  port:         %u\n",    ep_rsps[i].port);
    printf("  rtt:          %u ns\n", ep_rsps[i].rtt);
    printf("  adj_svc_time: %d\n",    ep_rsps[i].adj_svc_time);
}

if (num_rsps < 2) {
    printf("Warning: Only received %d of 2 responses (timeout)\n", num_rsps);
}
```

## Destroying Resources

To clean up, destroy the MRC context.

```c
if (mrc_destroy_context(mrc_ctx))
    return ERROR;

if (ibv_close_device(ibv_ctx))
    return ERROR;
```

