
# Explicit EV Configuration

## Query Device

The first thing the application must do is query the device to verify the
`MRC_OPT_CAP_EV_EXP_ARRAY` capability is supported and also gather some other
capabilities specific to explicit EVs.

```C
struct ibv_context *ib_ctx;
struct mrc_attr     mrc_attr;
bool                ev_array_range = false;
bool                ev_array_shared = false;

memset(&mrc_attr, 0, sizeof(mrc_attr));

if (mrc_query_device(ib_ctx, &mrc_attr) != 0)
    return ERROR;

if ((mrc_attr.cap & MRC_OPT_CAP_EV_EXP_ARRAY) == 0) {
    /* device does not support programming an array of explicit EVs */
    return ERROR;
}

if (mrc_attr.cap & MRC_OPT_CAP_EV_EXP_ARRAY_RANGE)
    ev_array_range = true;

if (mrc_attr.cap & MRC_OPT_CAP_SHARED_EV_ARRAYS)
    ev_array_shared = true;
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
mrc_context_attr.mrc_api_version_used      = MRC_API_CURRENT_VERSION;
mrc_context_attr.ev_mode                   = MRC_EV_MODE_EXP_ARRAY;
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

## Get Various EV Attributes Needed to Create EV Array

In order for the application to provision the EV array, it must learn the
minumum and maximum size of the EV array, any EV array alignment restrictions,
the minumum number of EVs that must remain active, and the maximum value for
each EV.

```C
uint32_t min_num_ev    = 0;
uint32_t max_ev_count  = 0;
uint32_t num_ev_align  = 0;
uint32_t ev_min_active = 0;
uint32_t max_ev_val    = 0;

/* clear the QP attributes... */
memset(&ibv_qp_attr, 0, sizeof(ibv_qp_attr));
/* set QP attributes mask... */
ibv_qp_attr_mask = 0;

/* clear the MRC QP attributes... */
memset(&mrc_qp_attr, 0, sizeof(mrc_qp_attr));
/* set MRC QP attributes mask... */
mrc_qp_attr_mask  = 0;
mrc_qp_attr_mask |= MRC_QP_MIN_NUM_EV;
mrc_qp_attr_mask |= MRC_QP_MAX_EV_COUNT;
mrc_qp_attr_mask |= MRC_QP_NUM_EV_ALIGN;
mrc_qp_attr_mask |= MRC_QP_EV_MIN_ACTIVE;
mrc_qp_attr_mask |= MRC_QP_MAX_EV_VAL;

/* clear the QP init attributes... */
memset(&mrc_qp_init_attr, 0, sizeof(mrc_qp_init_attr));

if (mrc_query_qp(mrc_qp,
                 &ibv_qp_attr, ibv_qp_attr_mask,
                 &mrc_qp_attr, mrc_qp_attr_mask,
                 &mrc_qp_init_attr) != 0)
    return ERROR;

min_num_ev    = mrc_qp_init_attr.min_num_ev;
max_ev_count  = mrc_qp_init_attr.max_primed_ev_per_qp;
num_ev_align  = mrc_qp_init_attr.num_ev_align;
ev_min_active = mrc_qp_init_attr.min_active_ev_per_qp;
max_ev_val    = mrc_qp_init_attr.max_ev_bits;
```

## Create an EV Array

Before moving the QP to the `RTR` state, the application must provision the
EV array. This example here details creating an EV array and filling
each entry with a random value.

*Note: Here we try to create an array that contains 128 entries yet we must
obey the min/max/alignment EV array attributes returned by the provider driver
above.*

```C
int                  num_ev = 128;
struct mrc_ev_entry *mrc_ev_entry;
struct mrc_ev_array *mrc_ev_array;

/* make sure num_ev is not less than the minimum */
if (num_ev < min_num_ev)
    num_ev = min_num_ev;

/* make sure num_ev is not greater than the maximum */
if (num_ev > max_ev_count)
    num_ev = max_ev_count;

/*
 * If num_ev isn't aligned, round it UP to the next alignment. This code
 * assumes that the num_ev_align value supplied by the provider driver is a
 * power of two.
 */
if ((num_ev % num_ev_align) != 0) {
    num_ev = ((num_ev + num_ev_align - 1) & ~(num_ev_align - 1));

/* allocate a temporary EV array */
mrc_ev_entry = malloc(num_ev * sizeof(*mrc_ev_entry));
if (mrc_ev_entry == NULL)
    return ERROR;

/* fill in the array with random values under the max EV value */
for (int i = 0; i < num_ev; i++) {
    mrc_ev_entry[i].state = MRC_EV_GOOD;
    mrc_ev_entry[i].val   = (random() % max_ev_val);
}

/* create the EV array */
mrc_ev_array = mrc_create_ev_array_explicit(mrc_context,
                                            num_ev,
                                            false, /* shared EV array */
                                            mrc_ev_entry);
if (mrc_ev_array == NULL)
    return ERROR;

/* the temporary EV array can now be freed */
free(mrc_ev_entry);
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
mrc_qp_attr.num_ev               = num_ev;
mrc_qp_attr.min_active_ev_per_qp = ev_min_active; /* no change */
mrc_qp_attr.ev_array             = mrc_ev_array;
/* set MRC QP attributes mask... */
mrc_qp_attr_mask  = 0;
mrc_qp_attr_mask |= MRC_QP_MAX_EV_COUNT;
mrc_qp_attr_mask |= MRC_QP_EV_MIN_ACTIVE;
mrc_qp_attr_mask |= MRC_QP_EV_ARRAY;

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
modify `RTS` to `RTS` transition. Here the entire set of EVs is updated with a new
random value. Note that the application could update any subset of the EVs in
the array.

*Note: The EV array itself cannot be changed (i.e., swapped out) or resized.
The only thing the application can do is update EV values in the array.*

```C
struct mrc_ev_entry tmp_ev_entry;

for (int i = 0; i < num_ev; i++) {
    tmp_ev_entry.state = MRC_EV_GOOD;
    tmp_ev_entry.val   = (random() % max_ev_val);

    if (mrc_update_ev_entry(mrc_ev_array, i, &tmp_ev_entry) == -1)
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
mrc_qp_attr_mask |= MRC_QP_EV_ARRAY_VALUES;

if (mrc_modify_qp(mrc_qp,
                  &ibv_qp_attr, ibv_qp_attr_mask,
                  &mrc_qp_attr, mrc_qp_attr_mask) != 0)
    return ERROR;
```

## Get the EV Array via Query QP

The application can retrieve a copy of the current EV in use on the QP. The
returned `mrc_ev_array` is ephemeral and must be destroyed by the application
when it's done with it. The application will want to make this call in order
to get the current state of the EVs in the QP's EV array.

```C
/* clear the QP attributes... */
memset(&ibv_qp_attr, 0, sizeof(ibv_qp_attr));
/* set QP attributes mask... */
ibv_qp_attr_mask = 0;

/* clear the MRC QP attributes... */
memset(&mrc_qp_attr, 0, sizeof(mrc_qp_attr));
/* set MRC QP attributes mask... */
mrc_qp_attr_mask  = 0;
mrc_qp_attr_mask |= MRC_QP_EV_ARRAY;
mrc_qp_attr_mask |= MRC_QP_EV_ARRAY_SIZE;

/* clear the QP init attributes... */
memset(&mrc_qp_init_attr, 0, sizeof(mrc_qp_init_attr));

if (mrc_query_qp(mrc_qp,
                 &ibv_qp_attr, ibv_qp_attr_mask,
                 &mrc_qp_attr, mrc_qp_attr_mask,
                 &mrc_qp_init_attr) != 0)
    return ERROR;

for (int i = 0; i < mrc_qp_attr.num_ev; i++) {
    /* get the EV entry */
    if (mrc_get_ev_entry(mrc_qp_attr.ev_array, i, &tmp_ev_entry) == -1)
        return ERROR;

    /* check the EV entry */
    if (tmp_ev_entry.state != MRC_EV_GOOD) {
        /* do something... */
    }
}

/* all done with the returned EV array so destroy it */
if (mrc_destroy_ev_array(mrc_qp_attr.ev_array) != 0)
    return ERROR;
```

