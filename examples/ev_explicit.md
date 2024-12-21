
# Explicit EV Configuration

## Query Device

The first thing the application must do is query the device to verify the
`MRC_OPT_CAP_EV_EXP_ARRAY` capability is supported.

```C
struct ibv_context *ib_ctx;
struct mrc_attr     mrc_attr;

memset(&mrc_attr, 0, sizeof(mrc_attr));

if (mrc_query_device(ib_ctx, &mrc_attr) != 0)
    return ERROR;

if ((mrc_attr.cap & MRC_OPT_CAP_EV_EXP_ARRAY) == 0) {
    /* device does not support programming an array of explicit EVs */
    return ERROR;
}
```

## Create MRC Context

Next step is create a new MRC context. For explicit EVs there is no need
to specify a "generation allowed format" since that only applies to the
`MRC_OPT_CAP_EV_GEN_ARRAY` and `MRC_OPT_CAP_EV_PRIMED_GEN_ARRAY`
capabilities.

```C
struct mrc_context_attr  mrc_context_attr;
struct mrc_context      *mrc_context;

memset(&mrc_context_attr, 0, sizeof(mrc_context_attr));
mrc_context_attr.version                   = MRC_API_CURRENT_VERSION;
mrc_context_attr.mrc_ev_mode               = MRC_OPT_CAP_EV_EXP_ARRAY;
mrc_context_attr.allow_fmt                 = NULL;
mrc_context_attr.mrc_ev_num_lsb_plane_bits = 0x7; /* 8 planes */

mrc_context = mrc_create_context(ib_ctx, &mrc_context_attr);
if (mrc_context == NULL)
    return ERROR;
```

## Create an MRC QP

Creating an MRC QP does not involve any EV configuration. EVs will be
configured during the QP modify process. After QP creation, the QP is in the
`RST` state.

```C
struct mrc_qp_init_attr  mrc_qp_init_attr;
struct mrc_qp           *mrc_qp;

memset(&mrc_qp_init_attr, 0, sizeof(mrc_qp_init_attr));
/* fill in MRC QP init attributes... */

mrc_qp = mrc_create_qp(mrc_context, &mrc_qp_init_attr);
if (mrc_qp == NULL)
    return ERROR;
```

## Modify the QP to the INIT State

The QP must now be moved to the `INIT` state. There is no EV configuration
that occurs at this phase of QP bringup.

```C
struct ibv_qp_attr ibv_qp_attr;
int                ibv_qp_attr_mask;
struct mrc_qp_attr mrc_qp_attr;
enum               mrc_qp_attr_mask;

/* fill in QP attributes... */
memset(&ibv_qp_attr, 0, sizeof(ibv_qp_attr));
ibv_qp_attr.state = IBV_QPS_INIT;
/* set QP attributes mask... */
ibv_qp_attr_mask  = 0;
ibv_qp_attr_mask |= IBV_QP_STATE;

/* fill in MRC QP attributes... */
memset(&mrc_qp_attr, 0, sizeof(mrc_qp_attr));
/* set MRC QP attributes mask... */
mrc_qp_attr_mask = 0;

if (mrc_modify_qp(mrc_qp,
                  &ibv_qp_attr, ibv_qp_attr_mask,
                  &mrc_qp_attr, mrc_qp_attr_mask) != 0)
    return ERROR;
```

## Get the Maximum EV Values via Query QP

In order for the application to provision the EV array, it must learn the
maximum number of EVs supported and the maximum value for each EV.

```C
/* clear the QP attributes... */
memset(&ibv_qp_attr, 0, sizeof(ibv_qp_attr));

/* clear the MRC QP attributes... */
memset(&mrc_qp_attr, 0, sizeof(mrc_qp_attr));
/* set MRC QP attributes mask... */
mrc_qp_attr_mask  = 0;
mrc_qp_attr_mask |= MRC_QP_ATTR_MAX_EV_COUNT;
mrc_qp_attr_mask |= MRC_QP_ATTR_MAX_EV_VAL;

/* clear the QP init attributes... */
memset(&mrc_qp_init_attr, 0, sizeof(mrc_qp_init_attr));

if (mrc_query_qp(mrc_qp,
                 &ibv_qp_attr, ibv_qp_attr_mask,
                 &mrc_qp_attr, mrc_qp_attr_mask,
                 &mrc_qp_init_attr) != 0)
    return ERROR;

/*
 * Maximum values:
 *   mrc_qp_attr.max_ev_per_qp
 *   mrc_qp_attr.max_ev_val
 */
```

## Create an EV Array

Before moving the QP to the `RTR` state, the application must provision the
EV array. This example here details creating and EV array and filling
each entry with a random value.

*Note: The maximum EVs and maximum EV value are learned from the prior
call to `mrc_query_qp()`. The example continued here assumes these values are
128 entries and full 32b values.*

```C
int                      num_ev = 128, actual_ev;
struct mrc_ev_array     *mrc_ev_array;
struct mrc_ev_init_attr  mrc_ev_init_attr;
struct mrc_ev_entry     *mrc_ev_entry;

/* create an array of random EVs */
mrc_ev_entry = malloc(num_ev * sizeof(*mrc_ev_entry));
if (mrc_ev_entry == NULL)
    return ERROR;

for (int i; i < num_ev; i++) {
    mrc_ev_entry[i].state = MRC_EV_GOOD;
    mrc_ev_entry[i].val   = random();
}

memset(&mrc_ev_init_attr, 0, sizeof(mrc_ev_init_attr));
mrc_ev_init_attr.u.exp_attr.entries = mrc_ev_entry;
mrc_ev_init_attr.u.exp_attr.num_ev  = num_ev;

mrc_ev_array = mrc_create_ev_array(mrc_context, num_ev, &mrc_ev_init_attr);
if (mrc_ev_array == NULL)
    return ERROR;

if (mrc_get_ev_array_len(mrc_ev_array, &actual_ev) == -1)
    return ERROR;

if (actual_ev > num_ev) {
    /*
     * The hardware returned more EVs than requested. The extra EVs need to be
     * initialized with random values... ???
     */
}

num_ev = actual_ev;
```

## Modify the QP to the RTR State

With the EV array ready to go, the QP can now be programmed with the EVs and
moved to the `RTR` state.

```C
/* fill in QP attributes... */
memset(&ibv_qp_attr, 0, sizeof(ibv_qp_attr));
ibv_qp_attr.state = IBV_QPS_RTR;
/* set QP attributes mask... */
ibv_qp_attr_mask  = 0;
ibv_qp_attr_mask |= IBV_QP_STATE;

/* fill in MRC QP attributes... */
memset(&mrc_qp_attr, 0, sizeof(mrc_qp_attr));
mrc_qp_attr.max_ev_per_qp        = num_ev;
mrc_qp_attr.min_active_ev_per_qp = 4;
mrc_qp_attr.ev_array             = mrc_ev_array;
/* set MRC QP attributes mask... */
mrc_qp_attr_mask  = 0;
mrc_qp_attr_mask |= MRC_QP_ATTR_MAX_EV_COUNT;
mrc_qp_attr_mask |= MRC_QP_ATTR_EV_MIN_ACTIVE;
mrc_qp_attr_mask |= MRC_QP_ATTR_EV_ARRAY;

if (mrc_modify_qp(mrc_qp,
                  &ibv_qp_attr, ibv_qp_attr_mask,
                  &mrc_qp_attr, mrc_qp_attr_mask) != 0)
    return ERROR;

```

## Modify the QP to the RTS State

No EV configuration occurs during this QP state transition.

```C
/* fill in QP attributes... */
memset(&ibv_qp_attr, 0, sizeof(ibv_qp_attr));
ibv_qp_attr.state = IBV_QPS_RTS;
/* set QP attributes mask... */
ibv_qp_attr_mask  = 0;
ibv_qp_attr_mask |= IBV_QP_STATE;

/* fill in MRC QP attributes... */
memset(&mrc_qp_attr, 0, sizeof(mrc_qp_attr));
/* set MRC QP attributes mask... */
mrc_qp_attr_mask = 0;

if (mrc_modify_qp(mrc_qp,
                  &ibv_qp_attr, ibv_qp_attr_mask,
                  &mrc_qp_attr, mrc_qp_attr_mask) != 0)
    return ERROR;
```

## Replace Entire EV Array

This example shows how the application can update all the EVs in the EV array
and then update the QP with the new values. This can only occur in the QP
modify `RTS->RTS` transition. Here the entire set of EVs is updated with a new
32b random value.

```C
for (int i; i < num_ev; i++) {
    if (mrc_update_ev_state(mrc_ev_entry, i, MRC_EV_GOOD) == -1)
        return ERROR;

    if (mrc_update_ev(mrc_ev_entry, i, random()) == -1)
        return ERROR;
}

/* fill in QP attributes... */
memset(&ibv_qp_attr, 0, sizeof(ibv_qp_attr));
ibv_qp_attr.state = IBV_QPS_RTS;
/* set QP attributes mask... */
ibv_qp_attr_mask  = 0;
ibv_qp_attr_mask |= IBV_QP_STATE;

/* fill in MRC QP attributes... */
memset(&mrc_qp_attr, 0, sizeof(mrc_qp_attr));
mrc_qp_attr.ev_array = mrc_ev_array;
/* set MRC QP attributes mask... */
mrc_qp_attr_mask  = 0;
mrc_qp_attr_mask |= MRC_QP_ATTR_EV_ARRAY_VALUES;

if (mrc_modify_qp(mrc_qp,
                  &ibv_qp_attr, ibv_qp_attr_mask,
                  &mrc_qp_attr, mrc_qp_attr_mask) != 0)
    return ERROR;
```

## Deny Some EVs

This example shows how the application can update deny some EVs in the EV
array and then update the QP with the new values. This can only occur in the
QP modify `RTS->RTS` transition. Here there are three deny mask/values
created and applied to the EV array.

```C
int                 num_deny_ev = 3;
struct mrc_ev_deny *mrc_ev_deny;

if ((mrc_attr.cap & MRC_OPT_CAP_EV_EXP_ARRAY) == 0) {
    /* device does not support updating the EV deny array */
    return ERROR;
}

/* create an array of deny EVs */
mrc_ev_deny = malloc(num_deny_ev * sizeof(*mrc_ev_deny));
if (mrc_ev_deny == NULL)
    return ERROR;

mrc_ev_deny[0].deny_mask  = 0x0000ff00;
mrc_ev_deny[0].deny_value = 0x00000100;

mrc_ev_deny[1].deny_mask  = 0x0000ff00;
mrc_ev_deny[1].deny_value = 0x00000200;

mrc_ev_deny[2].deny_mask  = 0x00ff0000;
mrc_ev_deny[2].deny_value = 0x00800000;

if (mrc_udpate_ev_deny_array(mrc_ev_array, mrc_ev_deny, num_deny_ev) == -1)
    return ERROR;

/* fill in QP attributes... */
memset(&ibv_qp_attr, 0, sizeof(ibv_qp_attr));
ibv_qp_attr.state = IBV_QPS_RTS;
/* set QP attributes mask... */
ibv_qp_attr_mask  = 0;
ibv_qp_attr_mask |= IBV_QP_STATE;

/* fill in MRC QP attributes... */
memset(&mrc_qp_attr, 0, sizeof(mrc_qp_attr));
mrc_qp_attr.ev_array = mrc_ev_array;
/* set MRC QP attributes mask... */
mrc_qp_attr_mask  = 0;
mrc_qp_attr_mask |= MRC_QP_ATTR_EV_DENY_LIST;

if (mrc_modify_qp(mrc_qp,
                  &ibv_qp_attr, ibv_qp_attr_mask,
                  &mrc_qp_attr, mrc_qp_attr_mask) != 0)
    return ERROR;
```

## Get the EV Array via Query QP

The application can retrieve a copy of the current EV in use on the QP. The
returned `mrc_ev_array` returned is ephemeral and must be destroyed by the
application when done with it.

```C
/* clear the QP attributes... */
memset(&ibv_qp_attr, 0, sizeof(ibv_qp_attr));

/* clear the MRC QP attributes... */
memset(&mrc_qp_attr, 0, sizeof(mrc_qp_attr));
/* set MRC QP attributes mask... */
mrc_qp_attr_mask  = 0;
mrc_qp_attr_mask |= MRC_QP_ATTR_EV_ARRAY;

/* clear the QP init attributes... */
memset(&mrc_qp_init_attr, 0, sizeof(mrc_qp_init_attr));

if (mrc_query_qp(mrc_qp,
                 &ibv_qp_attr, ibv_qp_attr_mask,
                 &mrc_qp_attr, mrc_qp_attr_mask,
                 &mrc_qp_init_attr) != 0)
    return ERROR;

/*
 * Do something with the EV array...
 *   mrc_qp_attr.ev_array
 */

if (mrc_destroy_ev_array(mrc_qp_attr.ev_array) != 0)
    return ERROR;
```

