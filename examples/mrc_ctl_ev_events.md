
# Registering and Receiving EV State Change Events

This example demonstrates how a controller can use the APIs in `mrc_ctl.h` to
register for and process EV state change events. EV state change events are
used to notify the controller when the state of EVs in an EV profile changes.

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
array limits. For EV event support, the `MRC_CTL_OPT_CAP_EV_EVENT` capability
must be returned by the device.

```c
struct mrc_ctl_attr mrc_ctl_attr;

memset(&mrc_ctl_attr, 0, sizeof(mrc_ctl_attr));

if (mrc_ctl_query_device(ibv_ctx, &mrc_ctl_attr) != 0)
    return ERROR;

if (!(mrc_ctl_attr.opt_attr & MRC_CTL_OPT_CAP_EV_EVENT))
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

## Create an EV Event CQ

An EV event CQ is used to receive EV state change notifications.

```c
struct mrc_cq *ev_cq;
int            cqe = 128; /* number of entries required for the CQ */

ev_cq = mrc_ctl_create_ev_event_cq(mrc_ctx, cqe, NULL, NULL, 0);
if (!ev_cq)
    return ERROR;
```

---

## Register for EV Events on an EV Profile

Configure an EV profile to generate state change events for EVs. For
simplicity, the `ev_profile_id` used here would be a ID already created
for either an explicit or generated profile.

```c
struct mrc_ctl_ev_profile ev_profile;

memset(&ev_profile, 0, sizeof(ev_profile));

/* query the current profile configuration */
if (mrc_ctl_query_ev_profile(mrc_ctx, ev_profile_id, &ev_profile))
    return ERROR;

/* set the event mask to monitor for state changes */
ev_profile.ev_event_mask = (MRC_CTL_EV_GOOD | MRC_CTL_EV_DENIED);

/* update the profile with the new event mask */
if (mrc_ctl_modify_ev_profile(mrc_ctx, &ev_profile))
    return ERROR;
```

---

## Poll the EV Event CQ to Process Events

Poll the EV event CQ to retrieve and process EV state change events.

```c
struct mrc_ctl_ev_event events[4];
int i, num_events;

while (1) {
    num_events = mrc_ctl_poll_ev_event(ev_cq, 4, events);

    if (num_events < 0)
        return ERROR;

    if (num_events == 0) {
        break;

    for (i = 0; i < num_events; i++) {
        printf("EV Event:\n");
        printf("  ev_profile_id: %llu\n", events[i].ev_profile_id);
        printf("  ev_value:      %u\n",   events[i].ev.val.val);
        printf("  ev_state:      %u\n",   events[i].ev.state);
        printf("  drop_count:    %u\n",   events[i].drop_count);
    }
}
```

---

## Destroying Resources

```c
if (mrc_destroy_cq(cq)
    return ERROR;

if (mrc_destroy_context(mrc_ctx))
    return ERROR;
```

