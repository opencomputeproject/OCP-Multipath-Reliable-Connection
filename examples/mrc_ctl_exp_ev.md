
# Configuring an MRC Device with an Explicit EV Profile

This example demonstrates how a controller application can use the APIs in
`mrc_ctl.h` to configure an MRC device with an EV profile defining an explicit
array of EVs.

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
usage limits, and port/plane information. For explicit EV support, the
`MRC_CTL_EV_MODE_EXP` mode must be supported by the device.

```c
struct mrc_ctl_device_attr mrc_ctl_attr;

if (mrc_ctl_query_device(ibv_ctx, &mrc_ctl_attr) != 0)
    return ERROR;

if (!(mrc_ctl_attr.ev.ev_mode_mask & MRC_CTL_EV_MODE_EXP))
    return ERROR; /* Explicit EV mode not supported */
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

## Working with STEV EV Types

For STEV EV types, the EV value structure is a maximum of 32b and the format
profile must be configured for `MRC_CTL_EV_FMT_MODE_STEV`.

### Configure an EV Format Profile

Before creating an EV profile, we must configure an EV Format profile that
defines how EVs are encoded in packets. The EV Format profile follows a state
machine: `INIT -> OFFLINE -> ONLINE`. This example creates a STEV format profile
with two 16-bit fields.

```c
struct mrc_ctl_ev_fmt_profile_attr ev_fmt_profile_attr;
struct mrc_ctl_ev_fmt_field        fmt_fields[2];
uint64_t                           ev_fmt_profile_id;
int                                fmt_attr_mask;

/* Determine the format profile ID to use */
ev_fmt_profile_id = 13;

/* Define two 16-bit format fields (32 bits total for STEV) */
fmt_fields[0].width = 16; /* First field: 16 bits */
fmt_fields[1].width = 16; /* Second field: 16 bits */

/* Step 1: Transition INIT -> OFFLINE by setting mode and fields */
ev_fmt_profile_attr = (struct mrc_ctl_ev_fmt_profile_attr){
    .profile_state = MRC_CTL_PROFILE_OFFLINE,
    .ev_fmt_mode   = MRC_CTL_EV_FMT_MODE_STEV,
    .ev_fmt_op.op  = MRC_CTL_EV_FMT_OP_MODIFY_FIELDS,
    .ev_fmt_op.fmt_fields.fmt_fields = fmt_fields,
    .ev_fmt_op.fmt_fields.fmt_field_count = 2,
};

fmt_attr_mask = (MRC_CTL_EV_FMT_PROFILE_STATE |
                 MRC_CTL_EV_FMT_PROFILE_MODE |
                 MRC_CTL_EV_FMT_PROFILE_OP);

if (mrc_ctl_modify_ev_fmt_profile(mrc_ctx, ev_fmt_profile_id,
                                  &ev_fmt_profile_attr, fmt_attr_mask) != 0)
    return ERROR;

/* Step 2: Transition OFFLINE -> ONLINE */
ev_fmt_profile_attr = (struct mrc_ctl_ev_fmt_profile_attr){
    .profile_state = MRC_CTL_PROFILE_ONLINE,
};

fmt_attr_mask = MRC_CTL_EV_FMT_PROFILE_STATE;

if (mrc_ctl_modify_ev_fmt_profile(mrc_ctx, ev_fmt_profile_id,
                                  &ev_fmt_profile_attr, fmt_attr_mask) != 0)
    return ERROR;
```

### Configure an Explicit EV Profile

An explicit EV profile specifies a set of entropy values to be used by the
device. The EV profile follows a state machine: `INIT -> OFFLINE -> ONLINE`.
This example creates an EV profile with explicit EVs.

```c
struct mrc_ctl_ev_profile_attr ev_profile_attr;
uint64_t                       ev_profile_id;
uint64_t                       ev_fmt_profile_id;
int                            attr_mask;
uint32_t                       ev_count;

/* Determine the profile IDs to use */
ev_profile_id     = 1;
ev_fmt_profile_id = 13; /* Use the configured STEV format profile */

/* Calculate EV count (must respect alignment requirements) */
ev_count = mrc_ctl_attr.ev.ev_max_count_profile;
if (ev_count % mrc_ctl_attr.ev.ev_count_align != 0)
    ev_count -= (ev_count % mrc_ctl_attr.ev.ev_count_align);

/* Step 1: Transition INIT -> OFFLINE by setting mode, format, count, and fields */
ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
    .profile_state     = MRC_CTL_PROFILE_OFFLINE,
    .ev_mode           = MRC_CTL_EV_MODE_EXP,
    .ev_fmt_profile_id = ev_fmt_profile_id,
    .ev_count          = ev_count,
    .ev_min_active     = (ev_count / 4),
    .ev_event_mask     = (MRC_CTL_EV_GOOD | MRC_CTL_EV_ASSUMED_BAD),
};

attr_mask = (MRC_CTL_EV_PROFILE_STATE |
             MRC_CTL_EV_PROFILE_MODE |
             MRC_CTL_EV_PROFILE_FMT_ID |
             MRC_CTL_EV_PROFILE_COUNT |
             MRC_CTL_EV_PROFILE_MIN_ACTIVE |
             MRC_CTL_EV_PROFILE_EVENT_MASK);

if (mrc_ctl_modify_ev_profile(mrc_ctx, ev_profile_id,
                              &ev_profile_attr, attr_mask) != 0)
    return ERROR;
```

### Populate the Explicit EVs

For explicit mode, the EV array starts unpopulated. We must replace each
unpopulated entry with actual EV values before transitioning to ONLINE state.

```c
struct mrc_ctl_ev cur_ev;
struct mrc_ctl_ev new_ev;
uint32_t i, tmp_ev_val;

/* Populate all EVs in the profile */
for (i = 0; i < ev_count; i++) {
    /* Current EV is unpopulated */
    cur_ev = MRC_CTL_EV_UNPOPULATED;
    tmp_ev_val = ((i << 16) | i); /* EV value = array index in both fields */

    /* Set new EV value and port */
    memset(&new_ev, 0, sizeof(new_ev));
    memcpy(new_ev.val, &tmp_ev_val, sizeof(uint32_t));
    new_ev.port = 1; /* Port number from port_mask, must vary for spraying! */

    /* Replace operation */
    ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
        .ev_op.op = MRC_CTL_EV_OP_REPLACE_EV,
        .ev_op.replace_ev.cur_ev = cur_ev, /* Replace first unpopulated EV */
        .ev_op.replace_ev.new_ev = new_ev,
        .ev_op.replace_ev.all_copies = 0, /* Replace one entry */
    };

    attr_mask = MRC_CTL_EV_PROFILE_EV_OP;

    if (mrc_ctl_modify_ev_profile(mrc_ctx, ev_profile_id,
                                  &ev_profile_attr, attr_mask) != 0)
        return ERROR;
}
```

### Transition to ONLINE State

Once all EVs are populated, transition the profile to ONLINE state to make it
usable by QPs.

```c
ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
    .profile_state = MRC_CTL_PROFILE_ONLINE,
};

attr_mask = MRC_CTL_EV_PROFILE_STATE;

if (mrc_ctl_modify_ev_profile(mrc_ctx, ev_profile_id,
                              &ev_profile_attr, attr_mask) != 0)
    return ERROR;
```

### Modify EV State to DENIED for a Subset of EVs

After an EV profile is `ONLINE`, EV entries can have their state changed to
`DENIED`. The example below denies a subset of EVs specified by a value range.

```c
uint32_t ev_start_val = 13;
uint32_t ev_end_val   = 42;
uint32_t i, tmp_ev_val;

for (i = ev_start_val; i <= ev_end_val; i++) {
    struct mrc_ctl_ev ev;
    tmp_ev_val = ((i << 16) | i); /* EV value = array index in both fields */

    memset(&ev, 0, sizeof(ev));
    memcpy(ev.val, &tmp_ev_val, sizeof(uint32_t));
    ev.port = 1; /* set accordingly for original config w/ spraying */

    ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
        .ev_op.op = MRC_CTL_EV_OP_MODIFY_EV_STATE,
        .ev_op.modify_ev_state.ev = ev,
        .ev_op.modify_ev_state.state = MRC_CTL_EV_DENIED,
    };

    attr_mask = MRC_CTL_EV_PROFILE_EV_OP;

    if (mrc_ctl_modify_ev_profile(mrc_ctx, ev_profile_id,
                                  &ev_profile_attr, attr_mask) != 0)
        return ERROR;
}
```

### Replace the Values of a Subset of EVs

After an EV profile is ONLINE, the EV values can be replaced (if the device
supports `MRC_CTL_OPT_CAP_EV_PROFILE_MODIFY_ONLINE`). The example below
modifies a subset of EVs specified by a value range and offset value.

```c
uint32_t ev_start_val = 13;
uint32_t ev_end_val   = 42;
uint32_t ev_shift     = 128;
uint32_t i, tmp_ev_val;

for (i = ev_start_val; i <= ev_end_val; i++) {
    struct mrc_ctl_ev old_ev, new_ev;
    uint32_t old_val, new_val;

    old_val = ((i << 16) | i);
    new_val = (((i + ev_shift) << 16) | (i + ev_shift));

    memset(&old_ev, 0, sizeof(old_ev));
    memcpy(old_ev.val, &old_val, sizeof(uint32_t));
    old_ev.port = 1; /* set accordingly for original config w/ spraying */

    memset(&new_ev, 0, sizeof(new_ev));
    memcpy(new_ev.val, &new_val, sizeof(uint32_t));
    new_ev.port = 1; /* set accordingly for original config w/ spraying */

    ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
        .ev_op.op = MRC_CTL_EV_OP_REPLACE_EV,
        .ev_op.replace_ev.cur_ev = old_ev,
        .ev_op.replace_ev.new_ev = new_ev,
        .ev_op.replace_ev.all_copies = 0, /* Replace first match */
    };

    attr_mask = MRC_CTL_EV_PROFILE_EV_OP;

    if (mrc_ctl_modify_ev_profile(mrc_ctx, ev_profile_id,
                                  &ev_profile_attr, attr_mask) != 0)
        return ERROR;
}
```

### Query the EV Profile

After an EV profile has been created, it can be queried to get its current
state and retrieve the EV array.

```c
struct mrc_ctl_ev *ev_array;
uint32_t i;

/* First, query basic profile attributes */
attr_mask = (MRC_CTL_EV_PROFILE_STATE |
             MRC_CTL_EV_PROFILE_MODE |
             MRC_CTL_EV_PROFILE_FMT_ID |
             MRC_CTL_EV_PROFILE_COUNT |
             MRC_CTL_EV_PROFILE_MIN_ACTIVE |
             MRC_CTL_EV_PROFILE_EVENT_MASK);

if (mrc_ctl_query_ev_profile(mrc_ctx, ev_profile_id,
                             &ev_profile_attr, attr_mask) != 0)
    return ERROR;

if (ev_profile_attr.ev_mode != MRC_CTL_EV_MODE_EXP)
    return ERROR;

/* Allocate array to receive EV entries */
ev_array = calloc(ev_profile_attr.ev_count, sizeof(*ev_array));
if (!ev_array)
    return ERROR;

/* Query the EV array */
ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
    .ev_op.op = MRC_CTL_EV_OP_QUERY_EV_ARRAY,
    .ev_op.query_ev_array.ev = ev_array,
};

attr_mask = MRC_CTL_EV_PROFILE_EV_OP;

if (mrc_ctl_query_ev_profile(mrc_ctx, ev_profile_id,
                             &ev_profile_attr, attr_mask) != 0) {
    free(ev_array);
    return ERROR;
}

/* Print the EV array */
for (i = 0; i < ev_profile_attr.ev_count; i++) {
    uint32_t val;
    memcpy(&val, ev_array[i].val, sizeof(uint32_t));
    printf("EV[%d]: value = %u, port = %u\n", i, val, ev_array[i].port);
}

free(ev_array);
```

## Working with SRv6 EV Types

For SRv6 EV types, the EV value structure differs from STEV. The first 128 bits
(16 bytes) hold the SRv6 address. The format profile must be configured for
`MRC_CTL_EV_FMT_MODE_SRV6`. The format profile defines this wire format and the
procedure for configuring an EV profile that references this format profile is
similar to that shown above for STEV.

### Configure an SRv6 EV Format Profile

This example creates an SRv6 format profile with a 32-bit locator field followed
by four 16-bit uSID fields, for a total of 96 bits.

```c
struct mrc_ctl_ev_fmt_profile_attr srv6_fmt_profile_attr;
struct mrc_ctl_ev_fmt_field        srv6_fmt_fields[5];
uint64_t                           srv6_fmt_profile_id;
int                                srv6_fmt_attr_mask;

/* Determine the SRv6 format profile ID to use */
srv6_fmt_profile_id = 14;

/* Format fields: 1x 32b locator + 4x 16b uSID fields (96b total) */
srv6_fmt_fields[0].width = 32; /* Locator field: 32 bits */
srv6_fmt_fields[1].width = 16; /* uSID 1: 16 bits */
srv6_fmt_fields[2].width = 16; /* uSID 2: 16 bits */
srv6_fmt_fields[3].width = 16; /* uSID 3: 16 bits */
srv6_fmt_fields[4].width = 16; /* uSID 4: 16 bits */

/* Step 1: Transition INIT -> OFFLINE by setting mode and fields */
srv6_fmt_profile_attr = (struct mrc_ctl_ev_fmt_profile_attr){
    .profile_state = MRC_CTL_PROFILE_OFFLINE,
    .ev_fmt_mode   = MRC_CTL_EV_FMT_MODE_SRV6,
    .ev_fmt_op.op  = MRC_CTL_EV_FMT_OP_MODIFY_FIELDS,
    .ev_fmt_op.fmt_fields.fmt_fields = srv6_fmt_fields,
    .ev_fmt_op.fmt_fields.fmt_field_count = 5,
};

srv6_fmt_attr_mask = (MRC_CTL_EV_FMT_PROFILE_STATE |
                      MRC_CTL_EV_FMT_PROFILE_MODE |
                      MRC_CTL_EV_FMT_PROFILE_OP);

if (mrc_ctl_modify_ev_fmt_profile(mrc_ctx, srv6_fmt_profile_id,
                                  &srv6_fmt_profile_attr, srv6_fmt_attr_mask) != 0)
    return ERROR;

/* Step 2: Transition OFFLINE -> ONLINE */
srv6_fmt_profile_attr = (struct mrc_ctl_ev_fmt_profile_attr){
    .profile_state = MRC_CTL_PROFILE_ONLINE,
};

srv6_fmt_attr_mask = MRC_CTL_EV_FMT_PROFILE_STATE;

if (mrc_ctl_modify_ev_fmt_profile(mrc_ctx, srv6_fmt_profile_id,
                                  &srv6_fmt_profile_attr, srv6_fmt_attr_mask) != 0)
    return ERROR;
```

### Query an SRv6 EV ID

For SRv6 EV types, you can query the EV identifier for a specific EV
value. This operation is only available when the EV Format mode is
`MRC_CTL_EV_FMT_MODE_SRV6`.

```c
struct mrc_ctl_ev ev;
struct in6_addr srv6_addr;
uint32_t ev_id;

/* Build the SRv6 address to query */
memset(&srv6_addr, 0, sizeof(srv6_addr));
srv6_addr.s6_addr[0] = 0xde;
srv6_addr.s6_addr[1] = 0xad;
srv6_addr.s6_addr[2] = 0xca
srv6_addr.s6_addr[3] = 0xfe;
/* ... set the rest of the address (uSIDs) ... */

/* Setup EV to query */
memset(&ev, 0, sizeof(ev));
memcpy(ev.val, &srv6_addr, sizeof(srv6_addr));
ev.port = 1;

/* Query the EV ID */
ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
    .ev_op.op = MRC_CTL_EV_OP_QUERY_EV_ID,
    .ev_op.query_ev_id.ev = ev,
};

attr_mask = MRC_CTL_EV_PROFILE_EV_OP;

if (mrc_ctl_query_ev_profile(mrc_ctx, ev_profile_id,
                             &ev_profile_attr, attr_mask) != 0)
    return ERROR;

ev_id = ev_profile_attr.ev_op.query_ev_id.ev_id;
printf("SRv6 EV ID: %u\n", ev_id);
```

## Working with SRv6+SRH EV Types

For SRv6 with a Segment Routing Header (SRH), both the SRv6 address and the
single segment SRH address must be specified. The first 128 bits hold the SRv6
address, and the second 128 bits hold the SRH segment address. The format
profile defines this wire format and the procedure for configuring an EV
profile that references this format profile is similar to that shown above
for STEV.

### Configure an SRv6+SRH EV Format Profile

This example creates an SRv6+SRH format profile with two separate uSID
structures. The first for the SRv6 address (32-bit locator with six 16-bit
uSIDs = 128 bits) and the second for the SRH segment (32-bit locator with
four 16-bit uSIDs = 96 bits), for a total of 224 bits.

```c
struct mrc_ctl_ev_fmt_profile_attr srv6_srh_fmt_profile_attr;
struct mrc_ctl_ev_fmt_field        srv6_srh_fmt_fields[12];
uint64_t                           srv6_srh_fmt_profile_id;
int                                srv6_srh_fmt_attr_mask;

/* Determine the SRv6+SRH format profile ID to use */
srv6_srh_fmt_profile_id = 15;

/* Define format fields (224 bits total):
 * SRv6 address (128 bits): 32-bit locator + 6x 16-bit uSID
 * SRH segment (96 bits): 32-bit locator + 4x 16-bit uSID
 */

/* SRv6 address fields */
srv6_srh_fmt_fields[0].width = 32; /* SRv6 locator: 32 bits */
srv6_srh_fmt_fields[1].width = 16; /* SRv6 uSID 1: 16 bits */
srv6_srh_fmt_fields[2].width = 16; /* SRv6 uSID 2: 16 bits */
srv6_srh_fmt_fields[3].width = 16; /* SRv6 uSID 3: 16 bits */
srv6_srh_fmt_fields[4].width = 16; /* SRv6 uSID 4: 16 bits */
srv6_srh_fmt_fields[5].width = 16; /* SRv6 uSID 5: 16 bits */
srv6_srh_fmt_fields[6].width = 16; /* SRv6 uSID 6: 16 bits */

/* SRH segment fields */
srv6_srh_fmt_fields[7].width  = 32; /* SRH locator: 32 bits */
srv6_srh_fmt_fields[8].width  = 16; /* SRH uSID 1: 16 bits */
srv6_srh_fmt_fields[9].width  = 16; /* SRH uSID 2: 16 bits */
srv6_srh_fmt_fields[10].width = 16; /* SRH uSID 3: 16 bits */
srv6_srh_fmt_fields[11].width = 16; /* SRH uSID 4: 16 bits */

/* Step 1: Transition INIT -> OFFLINE by setting mode and fields */
srv6_srh_fmt_profile_attr = (struct mrc_ctl_ev_fmt_profile_attr){
    .profile_state = MRC_CTL_PROFILE_OFFLINE,
    .ev_fmt_mode   = MRC_CTL_EV_FMT_MODE_SRV6,
    .ev_fmt_op.op  = MRC_CTL_EV_FMT_OP_MODIFY_FIELDS,
    .ev_fmt_op.fmt_fields.fmt_fields = srv6_srh_fmt_fields,
    .ev_fmt_op.fmt_fields.fmt_field_count = 12,
};

srv6_srh_fmt_attr_mask = (MRC_CTL_EV_FMT_PROFILE_STATE |
                          MRC_CTL_EV_FMT_PROFILE_MODE |
                          MRC_CTL_EV_FMT_PROFILE_OP);

if (mrc_ctl_modify_ev_fmt_profile(mrc_ctx, srv6_srh_fmt_profile_id,
                                  &srv6_srh_fmt_profile_attr,
                                  srv6_srh_fmt_attr_mask) != 0)
    return ERROR;

/* Step 2: Transition OFFLINE -> ONLINE */
srv6_srh_fmt_profile_attr = (struct mrc_ctl_ev_fmt_profile_attr){
    .profile_state = MRC_CTL_PROFILE_ONLINE,
};

srv6_srh_fmt_attr_mask = MRC_CTL_EV_FMT_PROFILE_STATE;

if (mrc_ctl_modify_ev_fmt_profile(mrc_ctx, srv6_srh_fmt_profile_id,
                                  &srv6_srh_fmt_profile_attr,
                                  srv6_srh_fmt_attr_mask) != 0)
    return ERROR;
```

## Destroying Resources

To clean up, transition the EV profile back through OFFLINE to INIT state,
then destroy the MRC context.

```c
/* Transition EV profiles: ONLINE -> OFFLINE */
ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
    .profile_state = MRC_CTL_PROFILE_OFFLINE,
};

attr_mask = MRC_CTL_EV_PROFILE_STATE;

if ((mrc_ctl_modify_ev_profile(mrc_ctx, ev_profile_id,
                               &ev_profile_attr, attr_mask) != 0) ||
    (mrc_ctl_modify_ev_profile(mrc_ctx, srv6_profile_id,
                               &ev_profile_attr, attr_mask) != 0) ||
    (mrc_ctl_modify_ev_profile(mrc_ctx, srv6_srh_profile_id,
                               &ev_profile_attr, attr_mask) != 0))
    return ERROR;

/* Transition EV profiles: OFFLINE -> INIT */
ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
    .profile_state = MRC_CTL_PROFILE_INIT,
};

attr_mask = MRC_CTL_EV_PROFILE_STATE;

if ((mrc_ctl_modify_ev_profile(mrc_ctx, ev_profile_id,
                               &ev_profile_attr, attr_mask) != 0) ||
    (mrc_ctl_modify_ev_profile(mrc_ctx, srv6_profile_id,
                               &ev_profile_attr, attr_mask) != 0) ||
    (mrc_ctl_modify_ev_profile(mrc_ctx, srv6_srh_profile_id,
                               &ev_profile_attr, attr_mask) != 0))
    return ERROR;

/* Transition EV format profiles: ONLINE -> OFFLINE */
ev_fmt_profile_attr = (struct mrc_ctl_ev_fmt_profile_attr){
    .profile_state = MRC_CTL_PROFILE_OFFLINE,
};

fmt_attr_mask = MRC_CTL_EV_FMT_PROFILE_STATE;

if ((mrc_ctl_modify_ev_fmt_profile(mrc_ctx, ev_fmt_profile_id,
                                   &ev_fmt_profile_attr, fmt_attr_mask) != 0) ||
    (mrc_ctl_modify_ev_fmt_profile(mrc_ctx, srv6_fmt_profile_id,
                                   &ev_fmt_profile_attr, fmt_attr_mask) != 0) ||
    (mrc_ctl_modify_ev_fmt_profile(mrc_ctx, srv6_srh_fmt_profile_id,
                                   &ev_fmt_profile_attr, fmt_attr_mask) != 0))
    return ERROR;

/* Transition EV format profiles: OFFLINE -> INIT */
ev_fmt_profile_attr = (struct mrc_ctl_ev_fmt_profile_attr){
    .profile_state = MRC_CTL_PROFILE_INIT,
};

fmt_attr_mask = MRC_CTL_EV_FMT_PROFILE_STATE;

if ((mrc_ctl_modify_ev_fmt_profile(mrc_ctx, ev_fmt_profile_id,
                                   &ev_fmt_profile_attr, fmt_attr_mask) != 0) ||
    (mrc_ctl_modify_ev_fmt_profile(mrc_ctx, srv6_fmt_profile_id,
                                   &ev_fmt_profile_attr, fmt_attr_mask) != 0) ||
    (mrc_ctl_modify_ev_fmt_profile(mrc_ctx, srv6_srh_fmt_profile_id,
                                   &ev_fmt_profile_attr, fmt_attr_mask) != 0))
    return ERROR;

if (mrc_destroy_context(mrc_ctx))
    return ERROR;

if (ibv_close_device(ibv_ctx))
    return ERROR;
```

