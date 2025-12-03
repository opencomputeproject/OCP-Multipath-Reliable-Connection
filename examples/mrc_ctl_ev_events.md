
# Registering and Receiving EV State Change Events

This example demonstrates how a controller can use the APIs in `mrc_ctl.h` to
register for and process EV state change events. EV state change events are
used to notify the controller when the state of EVs in an EV profile change.

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
usage limits, and port/plane information. For EV event support, the
`MRC_CTL_OPT_CAP_EV_EVENT` capability must be returned by the device.

```c
struct mrc_ctl_device_attr mrc_ctl_attr;

if (mrc_ctl_query_device(ibv_ctx, &mrc_ctl_attr) != 0)
    return ERROR;

if (!(mrc_ctl_attr.opt_attr & MRC_CTL_OPT_CAP_EV_EVENT))
    return ERROR; /* EV events not supported */
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

## Create an EV Event CQ

An EV event CQ is used to receive EV state change notifications.

```c
struct mrc_cq *ev_cq;
int cqe = 128; /* number of entries required for the CQ */

ev_cq = mrc_ctl_create_ev_event_cq(mrc_ctx, cqe, NULL, NULL, 0);
if (!ev_cq)
    return ERROR;
```

## Register for EV Events on an EV Profile

Configure an EV profile to generate state change events for EVs. The example
here assumes an EV profile has already been created (either explicit or
generated mode). The event mask determines which state transitions generate
events.

```c
struct mrc_ctl_ev_profile_attr ev_profile_attr;
uint64_t ev_profile_id;
int attr_mask;

/* Target an existing EV profile ID */
ev_profile_id = 13;

/* Set the event mask to monitor for state changes
 * MRC_CTL_EV_GOOD: Generate events when EVs become GOOD
 * MRC_CTL_EV_ASSUMED_BAD: Generate events when EVs become ASSUMED_BAD
 */
ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
    .ev_event_mask = (MRC_CTL_EV_GOOD | MRC_CTL_EV_ASSUMED_BAD),
};

attr_mask = MRC_CTL_EV_PROFILE_EVENT_MASK;

/* Update the profile with the new event mask */
if (mrc_ctl_modify_ev_profile(mrc_ctx, ev_profile_id,
                              &ev_profile_attr, attr_mask) != 0)
    return ERROR;
```

## Poll the EV Event CQ to Process Events

Poll the EV event CQ to retrieve and process EV state change events. This
example shows processing 32b STEV EV events.

```c
struct mrc_ctl_ev_event events[4];
int i, num_events;

while (1) {
    num_events = mrc_ctl_poll_ev_event(ev_cq, 4, events);

    if (num_events < 0)
        return ERROR;

    if (num_events == 0)
        break; /* No more events */

    for (i = 0; i < num_events; i++) {
        uint32_t ev_val;

        /* Extract the 32b STEV value */
        memcpy(&ev_val, events[i].ev.val, sizeof(uint32_t));

        printf("EV Event:\n");
        printf("  profile_id:  %llu\n", events[i].profile_id);
        printf("  ev_value:    0x%08x\n", ev_val);
        printf("  ev_port:     %u\n", events[i].ev.port);
        printf("  drop_count:  %u\n", events[i].drop_count);
    }
}
```

## Understanding the Drop Count

The `drop_count` field indicates if any events were lost due to CQ overflow.

If `MRC_CTL_OPT_CAP_EV_EVENT_PRECISE_DROP_CNT` is supported, the field contains
the exact number of events dropped between the previous and current event.
Otherwise it contains 1 if any events were dropped, 0 if none were dropped.

```c
/* Check if precise drop count is supported */
if (mrc_ctl_attr.opt_attr & MRC_CTL_OPT_CAP_EV_EVENT_PRECISE_DROP_CNT) {
    if (events[i].drop_count > 0)
        printf("Warning: %u events were dropped\n", events[i].drop_count);
} else {
    if (events[i].drop_count)
        printf("Warning: Some events were dropped\n");
}
```

## Destroying Resources

To clean up, destroy the CQ and MRC context.

```c
if (mrc_destroy_cq(mrc_ctx))
    return ERROR;

if (mrc_destroy_context(mrc_ctx))
    return ERROR;

if (ibv_close_device(ibv_ctx))
    return ERROR;
```

