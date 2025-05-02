
# Sending and Receiving EV Probes

This example demonstrates how a controller can use the APIs in `mrc_ctl.h` to
send and receive EV probes. EV probes allow an the controller to measure
connectivity or path quality to a destination over a specified path.

---

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

---

## Query the Device

Once the device is open, the controller can query the device to learn the
device's MRC controller capabilities. This includes available features and EV
array limits. For EV probe support, the `MRC_CTL_OPT_CAP_EV_PROBE` capability
must be returned by the device.

```c
struct mrc_ctl_attr mrc_ctl_attr;

memset(&mrc_ctl_attr, 0, sizeof(mrc_ctl_attr));

if (mrc_ctl_query_device(ibv_ctx, &mrc_ctl_attr) != 0)
    return ERROR;

if (!(mrc_ctl_attr.opt_attr & MRC_CTL_OPT_CAP_EV_PROBE))
    return ERROR;
```

---

## Create an MRC Context

The next step is to create the MRC context which is the parent object used for
all MRC operations and it tracks resources allocated by the process.

```c
struct mrc_context_attr  mrc_ctx_attr;
struct mrc_context      *mrc_ctx;

memset(&mrc_ctx_attr, 0, sizeof(mrc_ctx_attr));
ctx_attr = { .mrc_api_version_used = MRC_API_CURRENT_VERSION };

mrc_ctx = mrc_create_context(ibv_ctx, &mrc_ctx_attr);
if (!mrc_ctx)
    return ERROR;
```

---

## Sending EV Probe Requests

In this example, raw IPv4 addresses from the `10.0.0.0/8` subnet are used for
the source and destination GIDs.

```c
struct mrc_ctl_ev_probe_req p_reqs[2];
struct mrc_ctl_ev_probe_rsp p_rsps[2];
int i, num_rsps;

inet_pton(AF_INET, "10.0.0.1", &p_reqs[0].sgid.raw);
inet_pton(AF_INET, "10.0.0.2", &p_reqs[0].dgid.raw);
p_reqs[0].probe_id   = 1;
p_reqs[0].req_ev.val = 13;
p_reqs[0].rsp_ev.val = 42;

inet_pton(AF_INET, "10.0.0.1", &p_reqs[1].sgid.raw);
inet_pton(AF_INET, "10.0.0.3", &p_reqs[1].dgid.raw);
p_reqs[1].probe_id   = 2;
p_reqs[1].req_ev.val = 26;
p_reqs[1].rsp_ev.val = 51;

if (mrc_ctl_probe_ev(mrc_ctx, 0, p_reqs, 2, 2000000, p_rsps, &num_rsps))
    return ERROR;

for (int i = 0; i < num_rsps; i++) {
    /* to get the original request path EV the probe_id must be matched */
    printf("Probe Response:\n");
    printf("  probe_id:     %u\n",    responses[i].probe_id);
    printf("  rtt:          %u ns\n", responses[i].rtt);
    printf("  adj_svc_time: %d\n",    responses[i].adj_svc_time);
}
```

---

## Destroying Resources

```c
if (mrc_destroy_context(mrc_ctx))
    return ERROR;
```

