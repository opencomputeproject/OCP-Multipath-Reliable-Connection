
# Configuring an MRC Device with a Generated EV Profile

This example demonstrates how a controller application can use the APIs in
`mrc_ctl.h` to configure an MRC device with an EV profile defining a generated
array of EVs.

In this example, the generation format consists of three fields:
- **T0**: Maximum of 180 links (8 bits)
    - Since 180 isn't a power of two, values 0-179 are valid and 180-255 must
      be denied
- **T1**: Maximum of 16 links (4 bits)
- **T2**: Maximum of 16 links (4 bits)

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
array limits. For generated EV support, the `MRC_CTL_OPT_CAP_EV_GEN_ARRAY`
capability must be returned by the device.

```c
struct mrc_ctl_attr mrc_ctl_attr;

memset(&mrc_ctl_attr, 0, sizeof(mrc_ctl_attr));

if (mrc_ctl_query_device(ibv_ctx, &mrc_ctl_attr) != 0)
    return ERROR;

if (!(mrc_ctl_attr.opt_attr & MRC_CTL_OPT_CAP_EV_GEN_ARRAY))
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

## Set the EV Generation Fields Format

Define the format for the generated EVs. The format must be specified before creating a generated EV profile.

1. Define an array of field widths for the generation format.
2. Call `mrc_ctl_ev_gen_fields_set()` to configure the format.

```c
uint8_t field_widths[3] = { 8, 4, 4 };

if (mrc_ctl_ev_gen_fields_set(mrc_ctx, field_widths, 3))
    return ERROR;
```

---

## Create a New Generated EV Profile

A generated EV profile specifies the parameters to be used by the device when
generating EVs. The example code below requests and array of `max_ev_count`
entries

```c
struct mrc_ctl_ev_profile ev_profile;
uint64_t                  ev_profile_id;

/* here the controller would determine the EV profile ID to use */
ev_profile_id = 1;

memset(&ev_profile, 0, sizeof(ev_profile));
ev_profile = {
    .ev_profile_id = ev_profile_id,
    .ev_mode       = MRC_CTL_EV_MODE_GEN_ARRAY,
    .ev_count      = mrc_ctl_attr.max_ev_count,
    .ev_min_active = (mrc_ctl_attr.max_ev_count / 4),
    .ev_event_mask = (MRC_CTL_EV_GOOD |
                      MRC_CTL_EV_ASSUMED_BAD),
};

if (mrc_ctl_modify_ev_profile(mrc_ctx, &ev_profile))
    return ERROR;
```

---

## Query the EV Profile

After an EV profile has been created, it can be queried to get its current
state and set of generated EVs.

```c
struct mrc_ctl_ev_profile ev_profile;
uint32_t i;

memset(&ev_profile, 0, sizeof(ev_profile));

if (mrc_ctl_query_ev_profile(mrc_ctx, ev_profile_id, &ev_profile))
    return ERROR;

if (ev_profile.ev_mode != MRC_CTL_EV_MODE_GEN_ARRAY)
    return ERROR;

for (i = 0; i < ev_profile.ev_count; i++) {
    printf("  EV[%d]: value = %u (state = %d)\n", i,
           ev_profile.u.ev_gen.ev_gen_array[i].val.val,
           ev_profile.u.ev_gen.ev_gen_array[i].state);
}

free(ev_profile.u.ev_gen.ev_gen_array);
```

---

## Deny a Subset of EVs

After an EV profile has been created, EV entries can be set to `DENIED`. The
example below denies the unused EVs in the T0 field. Other generated EV values
can also be denied and these values are learned by the controller after a call
to `mrc_ctl_query_ev_profile()` as shown above.

```c
uint32_t i, j;

for (i = 180; i <= 255; i++) {
    for (j = 0; j < ev_profile.ev_count; j++) {
        mrc_ctl_ev_t *ev = &ev_profile.u.ev_gen.ev_gen_array[j].val;
        if ((ev->val & 0xff) == i)
            mrc_ctl_update_ev(mrc_ctx, ev_profile_id, ev, MRC_CTL_EV_DENIED);
    }
}
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

