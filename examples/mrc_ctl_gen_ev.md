
# Configuring an MRC Device with a Generated EV Profile

This example demonstrates how a controller application can use the APIs in
`mrc_ctl.h` to configure an MRC device with an EV profile defining a generated
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
usage limits, and port/plane information. For generated EV support, the `MRC_CTL_EV_MODE_GEN` mode must be supported by the device.

```c
struct mrc_ctl_device_attr mrc_ctl_attr;

if (mrc_ctl_query_device(ibv_ctx, &mrc_ctl_attr) != 0)
    return ERROR;

if (!(mrc_ctl_attr.ev.ev_mode_mask & MRC_CTL_EV_MODE_GEN))
    return ERROR; /* Generated EV mode not supported */
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
machine: `INIT -> OFFLINE -> ONLINE`.

In this STEV example, the generation format consists of three tiers/fields:
- **T0**: Maximum of 180 links (8 bits)
    - Since 180 isn't a power of two, values 0-179 are valid and 180-255 must
      be denied. For this example the `max_val` in the EV profile is set to the
      full width of the underlying format field. This is done to demonstrate
      denying a set of EVs after the EV profile is created. The controller
      could instead set the `max_val` for the EV field to 179 indicating to
      the NIC that this field's generated values must be capped accordingly.
- **T1**: Maximum of 16 links (4 bits)
- **T2**: Maximum of 16 links (4 bits)

```c
struct mrc_ctl_ev_fmt_profile_attr ev_fmt_profile_attr;
struct mrc_ctl_ev_fmt_field        fmt_fields[3];
uint64_t                           ev_fmt_profile_id;
int                                fmt_attr_mask;

/* Determine the format profile ID to use */
ev_fmt_profile_id = 13;

/* Define three format tiers/fields (32 bits max for STEV) */
fmt_fields[0].width = 8; /* First tier/field: 8 bits */
fmt_fields[1].width = 4; /* Second tier/field: 4 bits */
fmt_fields[2].width = 4; /* Third tier/field: 4 bits */

/* Step 1: Transition INIT -> OFFLINE by setting mode and fields */
ev_fmt_profile_attr = (struct mrc_ctl_ev_fmt_profile_attr){
    .profile_state = MRC_CTL_PROFILE_OFFLINE,
    .ev_fmt_mode   = MRC_CTL_EV_FMT_MODE_STEV,
    .ev_fmt_op.op  = MRC_CTL_EV_FMT_OP_MODIFY_FIELDS,
    .ev_fmt_op.fmt_fields.fmt_fields = fmt_fields,
    .ev_fmt_op.fmt_fields.fmt_field_count = 3,
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

### Configure a Generated EV Profile

For generated EVs, the EV profile defines the field structure that describes
how an EV value is constructed. Each field has a width, initial value, min/max
range, and a mask indicating which bits are generated. The profile follows the
state machine: `INIT -> OFFLINE -> ONLINE`. With a generated EV profile, we can
immediately jump to the `ONLINE` state since there is no need to populate EVs
like with explicit profiles.

```c
struct mrc_ctl_ev_profile_attr ev_profile_attr;
struct mrc_ctl_ev_field        ev_fields[3];
uint64_t                       ev_profile_id;
uint64_t                       ev_fmt_profile_id;
int                            attr_mask;
uint32_t                       ev_count;

/* Determine the profile IDs to use */
ev_profile_id     = 1;
ev_fmt_profile_id = 13; /* Use the configured STEV format profile */

/* Define the three-field generation format:
 * Field 0 (T0): 8 bits, values 0-255 (will deny 180-255 later)
 * Field 1 (T1): 4 bits, values 0-15
 * Field 2 (T2): 4 bits, values 0-15
 */
memset(ev_fields, 0, sizeof(ev_fields));

/* T0 field: 8 bits */
ev_fields[0].width    = 8;
ev_fields[0].init_val = 0;
ev_fields[0].min_val  = 0;
ev_fields[0].max_val  = 255; /* Full 8-bit range */
ev_fields[0].mask     = 0xff; /* All bits generated */

/* T1 field: 4 bits */
ev_fields[1].width    = 4;
ev_fields[1].init_val = 0;
ev_fields[1].min_val  = 0;
ev_fields[1].max_val  = 15; /* 4-bit range */
ev_fields[1].mask     = 0x0f; /* All bits generated */

/* T2 field: 4 bits */
ev_fields[2].width    = 4;
ev_fields[2].init_val = 0;
ev_fields[2].min_val  = 0;
ev_fields[2].max_val  = 15; /* 4-bit range */
ev_fields[2].mask     = 0x0f; /* All bits generated */

/* Calculate EV count (hardware will generate this many combinations) */
ev_count = (ev_fields[0].max_val + 1) *
           (ev_fields[1].max_val + 1) *
           (ev_fields[2].max_val + 1); /* 256 * 16 * 16 = 65536 EVs */

/* Respect device limits and alignment */
if (ev_count > mrc_ctl_attr.ev.ev_max_count_profile)
    ev_count = mrc_ctl_attr.ev.ev_max_count_profile;

if (ev_count % mrc_ctl_attr.ev.ev_count_align != 0)
    ev_count -= (ev_count % mrc_ctl_attr.ev.ev_count_align);

/* Step 1: Transition INIT -> OFFLINE */
ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
    .profile_state     = MRC_CTL_PROFILE_OFFLINE,
    .ev_mode           = MRC_CTL_EV_MODE_GEN,
    .ev_fmt_profile_id = ev_fmt_profile_id,
    .ev_count          = ev_count,
    .ev_min_active     = (ev_count / 4),
    .ev_event_mask     = (MRC_CTL_EV_GOOD | MRC_CTL_EV_ASSUMED_BAD),
    .ev_op.op = MRC_CTL_EV_OP_MODIFY_FIELDS,
    .ev_op.fields.fields = ev_fields,
    .ev_op.fields.field_count = 3,
};

attr_mask = (MRC_CTL_EV_PROFILE_STATE |
             MRC_CTL_EV_PROFILE_MODE |
             MRC_CTL_EV_PROFILE_FMT_ID |
             MRC_CTL_EV_PROFILE_COUNT |
             MRC_CTL_EV_PROFILE_MIN_ACTIVE |
             MRC_CTL_EV_PROFILE_EVENT_MASK |
             MRC_CTL_EV_PROFILE_EV_OP);

if (mrc_ctl_modify_ev_profile(mrc_ctx, ev_profile_id,
                              &ev_profile_attr, attr_mask) != 0)
    return ERROR;

/* Step 2: Transition OFFLINE -> ONLINE */
ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
    .profile_state = MRC_CTL_PROFILE_ONLINE,
};

attr_mask = MRC_CTL_EV_PROFILE_STATE;

if (mrc_ctl_modify_ev_profile(mrc_ctx, ev_profile_id,
                              &ev_profile_attr, attr_mask) != 0)
    return ERROR;
```

### Query the EV Profile

After an EV profile has been created, it can be queried to get its current
state, generation fields, and the generated EV array.

```c
struct mrc_ctl_ev_field *query_fields;
struct mrc_ctl_ev *ev_array;
int query_field_count;
uint32_t i, val;

/* First, query basic profile attributes */
attr_mask = (MRC_CTL_EV_PROFILE_STATE |
             MRC_CTL_EV_PROFILE_MODE |
             MRC_CTL_EV_PROFILE_FMT_ID |
             MRC_CTL_EV_PROFILE_COUNT);

if (mrc_ctl_query_ev_profile(mrc_ctx, ev_profile_id,
                             &ev_profile_attr, attr_mask) != 0)
    return ERROR;

if (ev_profile_attr.ev_mode != MRC_CTL_EV_MODE_GEN)
    return ERROR;

/* Query the generation fields */
query_field_count = 10; /* Allocate space for up to 10 fields */
query_fields = calloc(query_field_count, sizeof(*query_fields));
if (!query_fields)
    return ERROR;

ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
    .ev_op.op = MRC_CTL_EV_OP_QUERY_FIELDS,
    .ev_op.fields.fields = query_fields,
    .ev_op.fields.field_count = query_field_count,
};

attr_mask = MRC_CTL_EV_PROFILE_EV_OP;

if (mrc_ctl_query_ev_profile(mrc_ctx, ev_profile_id,
                             &ev_profile_attr, attr_mask) != 0) {
    free(query_fields);
    return ERROR;
}

/* Print the fields */
printf("EV Generation Fields:\n");
query_field_count = ev_profile_attr.ev_op.fields.field_count;
for (i = 0; i < query_field_cound; i++) {
    printf("Field[%d]: width=%u, init=%u, min=%u, max=%u, mask=0x%x\n", i,
           query_fields[i].width,
           query_fields[i].init_val,
           query_fields[i].min_val,
           query_fields[i].max_val,
           query_fields[i].mask);
}

free(query_fields);

/* Allocate array to receive generated EV entries */
ev_array = calloc(ev_profile_attr.ev_count, sizeof(*ev_array));
if (!ev_array)
    return ERROR;

/* Query the generated EV array */
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

/* Print sample of generated EVs */
printf("Generated EVs:\n");
for (i = 0; i < ev_profile_attr.ev_count; i++) {
    memcpy(&val, ev_array[i].val, sizeof(uint32_t));
    printf("EV[%d]: value = 0x%08x, port = %u\n", i, val, ev_array[i].port);
}

free(ev_array);
```

### Modify EV State to DENIED for a Subset of EVs

After an EV profile is `ONLINE`, EV entries can have their state set to
`DENIED`. The example below denies the unused EVs in the T0 field (values
180-255). Note that the generated EVs must be queried first (as shown above)
to find which ones to deny.

```c
uint32_t i, val;

/* Deny EVs where T0 field (lowest 8 bits) is in range 180-255 */
for (i = 0; i < ev_profile_attr.ev_count; i++) {
    memcpy(&val, ev_array[i].val, sizeof(uint32_t));

    /* Check if T0 field is in the deny range */
    if ((val & 0xff) >= 180 && (val & 0xff) <= 255) {
        ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
            .ev_op.op = MRC_CTL_EV_OP_MODIFY_EV_STATE,
            .ev_op.modify_ev_state.ev = ev_array[i],
            .ev_op.modify_ev_state.state = MRC_CTL_EV_DENIED,
        };

        attr_mask = MRC_CTL_EV_PROFILE_EV_OP;

        if (mrc_ctl_modify_ev_profile(mrc_ctx, ev_profile_id,
                                      &ev_profile_attr, attr_mask) != 0) {
            free(ev_array);
            return ERROR;
        }
    }
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

### Configure a Generated EV Profile

The EV profile created here defines a mixed mode of fixed and generated bits
in the four uSIDs. The upper 8b of each uSID gets the tier number. The lower
8b are generated by the hardware. The first 32b is the locator and is a fixed
value used in all generated EVs.

```c
struct mrc_ctl_ev_field ev_fields_srv6[5];
uint64_t ev_profile_id_srv6;
uint64_t ev_fmt_profile_id_srv6;

ev_profile_id_srv6     = 2;
ev_fmt_profile_id_srv6 = 14;

/* Define five-field generation for SRv6:
 * Field 0: 32b fixed locator
 * Field 1: 16b first uSID (upper 8b 0x1, lower 8b generated)
 * Field 2: 16b second uSID (upper 8b 0x2, lower 8b generated)
 * Field 3: 16b third uSID (upper 8b 0x3, lower 8b generated)
 * Field 4: 16b fourth uSID (upper 8b 0x4, lower 8b generated)
 */
memset(ev_fields_srv6, 0, sizeof(ev_fields_srv6));

/* Field 0: 32b fixed locator */
ev_fields_srv6[0].width    = 32;
ev_fields_srv6[0].init_val = 0xdeadcafe;
ev_fields_srv6[0].min_val  = 0;
ev_fields_srv6[0].max_val  = 0;
ev_fields_srv6[0].mask     = 0;

/* Field 1: 16 bits uSID */
ev_fields_srv6[0].width    = 16;
ev_fields_srv6[0].init_val = 0x100;
ev_fields_srv6[0].min_val  = 0;
ev_fields_srv6[0].max_val  = 255;
ev_fields_srv6[0].mask     = 0xff;

/* Field 2: 16 bits uSID */
ev_fields_srv6[0].width    = 16;
ev_fields_srv6[0].init_val = 0x200;
ev_fields_srv6[0].min_val  = 0;
ev_fields_srv6[0].max_val  = 255;
ev_fields_srv6[0].mask     = 0xff;

/* Field 3: 16 bits uSID */
ev_fields_srv6[0].width    = 16;
ev_fields_srv6[0].init_val = 0x300;
ev_fields_srv6[0].min_val  = 0;
ev_fields_srv6[0].max_val  = 255;
ev_fields_srv6[0].mask     = 0xff;

/* Field 4: 16 bits uSID */
ev_fields_srv6[0].width    = 16;
ev_fields_srv6[0].init_val = 0x400;
ev_fields_srv6[0].min_val  = 0;
ev_fields_srv6[0].max_val  = 255;
ev_fields_srv6[0].mask     = 0xff;

/* Calculate EV count (hardware will generate this many combinations) */
ev_count = (ev_fields[1].max_val + 1) *
           (ev_fields[2].max_val + 1) *
           (ev_fields[3].max_val + 1) *
           (ev_fields[4].max_val + 1); /* 16 * 16 * 16 * 16 = 65536 EVs */

/* Respect device limits and alignment */
if (ev_count > mrc_ctl_attr.ev.ev_max_count_profile)
    ev_count = mrc_ctl_attr.ev.ev_max_count_profile;

if (ev_count % mrc_ctl_attr.ev.ev_count_align != 0)
    ev_count -= (ev_count % mrc_ctl_attr.ev.ev_count_align);

/* Transition INIT -> OFFLINE */
ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
    .profile_state     = MRC_CTL_PROFILE_OFFLINE,
    .ev_mode           = MRC_CTL_EV_MODE_GEN,
    .ev_fmt_profile_id = ev_fmt_profile_id_srv6,
    .ev_count          = ev_count,
    .ev_min_active     = (ev_count / 4),
    .ev_event_mask     = (MRC_CTL_EV_GOOD | MRC_CTL_EV_ASSUMED_BAD),
    .ev_op.op = MRC_CTL_EV_OP_MODIFY_FIELDS,
    .ev_op.fields.fields = ev_fields_srv6,
    .ev_op.fields.field_count = 2,
};

attr_mask = (MRC_CTL_EV_PROFILE_STATE |
             MRC_CTL_EV_PROFILE_MODE |
             MRC_CTL_EV_PROFILE_FMT_ID |
             MRC_CTL_EV_PROFILE_COUNT |
             MRC_CTL_EV_PROFILE_MIN_ACTIVE |
             MRC_CTL_EV_PROFILE_EVENT_MASK |
             MRC_CTL_EV_PROFILE_EV_OP);

if (mrc_ctl_modify_ev_profile(mrc_ctx, ev_profile_id_srv6,
                              &ev_profile_attr, attr_mask) != 0)
    return ERROR;

/* Transition OFFLINE -> ONLINE */
ev_profile_attr = (struct mrc_ctl_ev_profile_attr){
    .profile_state = MRC_CTL_PROFILE_ONLINE,
};

attr_mask = MRC_CTL_EV_PROFILE_STATE;

if (mrc_ctl_modify_ev_profile(mrc_ctx, ev_profile_id_srv6,
                              &ev_profile_attr, attr_mask) != 0)
    return ERROR;
```

## Working with SRv6+SRH EV Types

For SRv6 with a Segment Routing Header (SRH), both the SRv6 address and the
single segment SRH address must be specified. The first 128 bits hold the SRv6
address, and the second 128 bits hold the SRH segment address. The format
profile defines this wire format and the procedure for configuring an EV
profile that references this format profile is similar to that shown above
for STEV and SRv6.

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

To clean up, transition the EV profiles back through OFFLINE to INIT state,
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

