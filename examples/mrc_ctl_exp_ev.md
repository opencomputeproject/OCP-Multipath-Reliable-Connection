
# Configuring an MRC Device with an Explicit EV Profile

This example demonstrates how a controller application can use the APIs in
`mrc_ctl.h` to configure an MRC device with an EV profile defining an explicit
array of EVs.

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
array limits. For explicit EV support, the `MRC_CTL_OPT_CAP_EV_EXP_ARRAY`
capability must be returned by the device.

```c
struct mrc_ctl_attr mrc_ctl_attr;

memset(&mrc_ctl_attr, 0, sizeof(mrc_ctl_attr));

if (mrc_ctl_query_device(ibv_ctx, &mrc_ctl_attr) != 0)
    return ERROR;

if (!(mrc_ctl_attr.opt_attr & MRC_CTL_OPT_CAP_EV_EXP_ARRAY))
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

## Create a New Explicit EV Profile

An explicit EV profile specifies a set of entropy values to be used by the
device. The example code below creates and array of `max_ev_count` entries
as returned by `mrc_ctl_query_device()`.

```c
struct mrc_ctl_ev_entry   *ev_array;
struct mrc_ctl_ev_profile  ev_profile;
uint64_t                   ev_profile_id;

ev_array = calloc(1, (mrc_ctl_attr.max_ev_count * sizeof(*ev_array)));
if (ev_array == NULL)
    return ERROR;

for (int i = 0; i < mrc_ctl_attr.max_ev_count; i++) {
    ev_array[i].state   = MRC_CTL_EV_GOOD;
    ev_array[i].val.val = i; /* set the EV value to the array index */
}

/* here the controller would determine the EV profile ID to use */
ev_profile_id = 1;

memset(&ev_profile, 0, sizeof(ev_profile));
ev_profile = {
    .ev_profile_id         = ev_profile_id,
    .ev_mode               = MRC_CTL_EV_MODE_EXP_ARRAY,
    .ev_count              = mrc_ctl_attr.max_ev_count,
    .ev_min_active         = (mrc_ctl_attr.max_ev_count / 4),
    .u.ev_exp.ev_exp_array = ev_array,
    .ev_event_mask         = (MRC_CTL_EV_GOOD |
                              MRC_CTL_EV_ASSUMED_BAD),
};

if (mrc_ctl_modify_ev_profile(mrc_ctx, &ev_profile)) {
    free(ev_array);
    return ERROR;
}
```

---

## Deny a Subset of EVs

After an EV profile has been created, EV entries can be set to `DENIED`. The
example below denies a subset of EVs specified by a value range.

```c
uint32_t ev_start_val = 13;
uint32_t ev_end_val   = 42;
uint32_t i;

for (i = ev_start_val; i <= ev_end_val; i++) {
    mrc_ctl_ev_t ev = { .val = i };

    if (mrc_ctl_update_ev(mrc_ctx, ev_profile_id, &ev, MRC_CTL_EV_DENIED))
        return ERROR;
}
```

---

## Replace the Values of a Subset of EVs

After an EV profile has been created, the EV values can be modified. The
example below modifies a subset of EVs specified by a value range and offset
value to modify the existing values from.

```c
uint32_t ev_start_val = 13;
uint32_t ev_end_val   = 42;
uint32_t ev_shift     = 128;
uint32_t i;

for (i = ev_start_val; i <= ev_end_val; i++) {
    mrc_ctl_ev_t old_ev = { .val = i };
    mrc_ctl_ev_t new_ev = { .val = (i + ev_shift) };

    if (mrc_ctl_replace_ev(mrc_ctx, ev_profile_id, &old_ev, &new_ev))
        return ERROR;
}
```

---

## Query the EV Profile

After an EV profile has been created, it can be queried to get its current
state.

```c
struct mrc_ctl_ev_profile ev_profile;
uint32_t i;

memset(&ev_profile, 0, sizeof(ev_profile));

if (mrc_ctl_query_ev_profile(mrc_ctx, ev_profile_id, &ev_profile))
    return ERROR;

if (ev_profile.ev_mode != MRC_CTL_EV_MODE_EXP_ARRAY)
    return ERROR;

for (i = 0; i < ev_profile.ev_count; i++) {
    printf("  EV[%d]: value = %u (state = %d)\n", i,
           ev_profile.u.ev_exp.ev_exp_array[i].val.val,
           ev_profile.u.ev_exp.ev_exp_array[i].state);
}

free(ev_profile.u.ev_exp.ev_exp_array);
```

---

## Destroying Resources

The EV profile can be destroyed when it is no longer needed.

```c
if (mrc_ctl_destroy_ev_profile(mrc_ctx, ev_profile_id))
    return ERROR;

if (mrc_destroy_context(mrc_ctx))
    return ERROR;
```

