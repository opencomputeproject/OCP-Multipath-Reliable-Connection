
# Generated EV Configuration

## Query Device

The first thing the application must do is query the device to verify the
`MRC_OPT_CAP_EV_GEN_ARRAY` capability is supported.

```C
struct ibv_context *ib_ctx;
struct mrc_attr     mrc_attr;

memset(&mrc_attr, 0, sizeof(mrc_attr));

if (mrc_query_device(ib_ctx, &mrc_attr) != 0)
    return ERROR;

if (mrc_attr.cap & MRC_OPT_CAP_EV_GEN_ARRAY) {
    /* configure paramaters to instruct the hardware how to generate EVs */
}
```

## Create MRC Context

Next step is create a new MRC context. For generated EVs it is required
to specify a "generation allowed format". See comment below showing an example
on how the `ev_allow_mask` and `ev_max_allowed_vals` could be configured by the
application.

```C
struct mrc_context_attr      mrc_context_attr;
struct mrc_context          *mrc_context;
uint32_t tmp_ev;

memset(&mrc_context_attr, 0, sizeof(mrc_context_attr));
mrc_context_attr.version                   = MRC_API_CURRENT_VERSION;
mrc_context_attr.mrc_ev_mode               = MRC_OPT_CAP_EV_GEN_ARRAY;
mrc_context_attr.mrc_ev_num_lsb_plane_bits = 0x7; /* 8 planes */

/*
 * Construct an ev_allow_mask and ev_max_allowed_vals for 4 hops (includes
 * the plane). Since the number of hops is even, the first field mask is 0's
 * and the last is 1's.
 *   - 1st hop: 0x7   max = 7   (8 planes)
 *   - 2nd hop: 0xff  max = 180 (180 links)
 *   - 3rd hop: 0xf   max = 15  (16 links)
 *   - 4th hop: 0xf   max = 15  (16 links)
 *
 * ev_allow_mask       = 0x000787f8
 * ev_max_allowed_vals = 0x0007fda7
 * ev_min_allowed_vals = 0x00000000
 */

tmp_ev = 0;
tmp_ev = ((tmp_ev << 0) | 0xf);  /* 4th hop, 1's */
tmp_ev =  (tmp_ev << 4);         /* 3rd hop, 0's */
tmp_ev = ((tmp_ev << 8) | 0xff); /* 2rd hop, 1's */
tmp_ev =  (tmp_ev << 3);         /* 1st hop, 0's */

mrc_context_attr.allow_fmt.ev_allow_mask = tmp_ev; /* result: 0x000787f8 */

tmp_ev = 0;
tmp_ev = ((tmp_ev << 0) | 15);  /* 4th hop max */
tmp_ev = ((tmp_ev << 4) | 15);  /* 3rd hop max */
tmp_ev = ((tmp_ev << 8) | 180); /* 2rd hop max */
tmp_ev = ((tmp_ev << 3) | 7);   /* 1st hop max */

mrc_context_attr.allow_fmt.ev_max_allowed_vals = tmp_ev; /* result: 0x0007fda7 */

mrc_context_attr.allow_fmt.ev_min_allowed_vals = 0;

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

In order for the application to allocate the EV array, it must learn the
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

Before moving the QP to the `RTR` state, the application must allocate the
EV array. This array will be filled in with generated EVs when transitioning to
`RTR`.

*Note: The maximum EVs are learned from the prior call to `mrc_query_qp()`. The
example continued here assumes it is 128 entries.

```C
int                      num_ev = 128;
struct mrc_ev_array     *mrc_ev_array;
struct mrc_ev_init_attr  mrc_ev_init_attr;
struct mrc_ev_entry     *mrc_ev_entry;

/* allocate an array of EVs */
mrc_ev_entry = malloc(num_ev * sizeof(*mrc_ev_entry));
if (mrc_ev_entry == NULL)
    return ERROR;

/* clear the array */
memset(mrc_ev_entry, 0, (num_ev * sizeof(*mrc_ev_entry));

memset(&mrc_ev_init_attr, 0, sizeof(mrc_ev_init_attr));
mrc_ev_init_attr.u.exp_attr.entries = mrc_ev_entry;
mrc_ev_init_attr.u.exp_attr.num_ev  = num_ev;

mrc_ev_array = mrc_create_ev_array(mrc_context, num_ev, &mrc_ev_init_attr);
if (mrc_ev_array == NULL)
    return ERROR;
```

## Modify the QP to the RTR State

The QP is now moved to the `RTR` state. During this state transition, the EVs
are generated and will be in use on the Responder side of the QP.

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

/*
 * The application can now call mrc_get_ev_entry() to get the contents
 * of an index in the mrc_ev_array.
 */
struct mrc_ev_entry tmp;

if (mrc_get_ev_entry(mrc_ev_array, 13, &tmp) == -1)
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

## Deny Some EVs

This example shows how the application can update deny some EVs in the EV
array and then update the QP with the new values. This can only occur in the
QP modify `RTS` to `RTS` transition. Here there are three deny mask/values
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

mrc_ev_deny[0].deny_mask  = 0x000007f8;
mrc_ev_deny[0].deny_value = 0x00000008; /* EV 1 on 2nd hop */

mrc_ev_deny[1].deny_mask  = 0x00007800;
mrc_ev_deny[1].deny_value = 0x00000300; /* EV 3 on 3rd hop */

mrc_ev_deny[2].deny_mask  = 0x00078000;
mrc_ev_deny[2].deny_value = 0x00078000; /* EV 15 on 4th hop */

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

