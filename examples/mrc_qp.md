
# MRC QP Lifecycle Management

This example demonstrates how an application developer can use the MRC APIs
in `mrc.h` to find and open a device, manage an MRC context, and create a Queue
Pair (QP) for RDMA operations.

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

Once the device is open, the application can query the device to learn the
device's MRC capabilities. This includes the MRC version and limits for max
outstanding WRITE w/ Imm operations and max QP group/hint resources.

```c
struct mrc_attr mrc_attr;

memset(&mrc_attr, 0, sizeof(mrc_attr));

if (mrc_query_device(ibv_ctx, &mrc_attr) != 0)
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

## Create a Completion Queue (CQ)

A CQ is required to handle completions for work requests sent on the QP.

```c
struct mrc_cq *cq;
int            cqe = 128; /* number of entries required for the CQ */

cq = mrc_create_cq(mrc_ctx, cqe, NULL, NULL, 0);
if (!cq)
    return ERROR;
```

---

## Create an MRC QP

Creating an MRC QP takes a set of attributes that is equivalent to
`ibv_create_qp()`. After QP creation, the QP is in the `RST` state.

```c
struct mrc_qp *qp;
struct mrc_qp_init_attr mrc_qp_init_attr;

memset(&mrc_qp_init_attr, 0, sizeof(mrc_qp_init_attr));
/* fill in MRC QP init attributes... */
mrc_qp_init_attr = {
    //.qp_context = <app_defined>,
    .send_cq = cq,
    .recv_cq = cq,
    .sq_sig_all = 1,
    .cap = {
        .max_send_wr = 128,
        .max_recv_wr = 128,
        .max_send_sge = 1,
        .max_recv_sge = 1,
    },
    .pd = NULL,
    .send_ops_flags = (IBV_QP_EX_WITH_RDMA_WRITE |
                       IBV_QP_EX_WITH_RDMA_WRITE_WITH_IMM),
};

qp = mrc_create_qp(mrc_ctx, &qp_init_attr);
if (!qp)
    return ERROR;
```

---

## Modify the MRC QP to the INIT State

Before the QP can be used, it must be transitioned through the RDMA QP state
machine. The first state change is from `RST` to `INIT`. The example code
below doesn't create a QP group or QP hint and gets the EV profile ID from an
evironment variable.

```c
struct ibv_qp_attr ibv_qp_attr;
int                ibv_qp_attr_mask;
struct mrc_qp_attr mrc_qp_attr;
int                mrc_qp_attr_mask;
uint64_t           ev_profile_id;

/* get the EV profile ID from the environment variable */
ev_profile_id = strtoul(getenv("MRC_QP_EV_PROFILE_ID"), NULL, 10);

/* fill in QP attributes... */
memset(&ibv_qp_attr, 0, sizeof(ibv_qp_attr));
ibv_qp_attr.state = IBV_QPS_INIT;
/* set QP attributes mask... */
ibv_qp_attr_mask  = 0;
ibv_qp_attr_mask |= IBV_QP_STATE;

/* fill in MRC QP attributes... */
memset(&mrc_qp_attr, 0, sizeof(mrc_qp_attr));
mrc_qp_attr.ev_profile_id = ev_profile_id;
mrc_qp_attr.qp_hint       = NULL;
/* set MRC QP attributes mask... */
mrc_qp_attr_mask  = 0;
mrc_qp_attr_mask |= MRC_QP_EV_PROFILE_ID;
mrc_qp_attr_mask |= MRC_QP_HINT;

if (mrc_modify_qp(mrc_qp,
                  &ibv_qp_attr, ibv_qp_attr_mask,
                  &mrc_qp_attr, mrc_qp_attr_mask) != 0)
    return ERROR;
```

---

## Modify the MRC QP to the RTR State

The next state change is from `INIT` to `RTR`. Note that after a successful
change to `RTR`, receive work requests can be posted to the `RQ` which are
required for receiving WRITE w/ Imm operations from the peer.

```c
/* fill in QP attributes... */
memset(&ibv_qp_attr, 0, sizeof(ibv_qp_attr));
ibv_qp_attr.state = IBV_QPS_RTR;
/* set QP attributes mask... */
ibv_qp_attr_mask  = 0;
ibv_qp_attr_mask |= IBV_QP_STATE;

/* fill in MRC QP attributes... */
memset(&mrc_qp_attr, 0, sizeof(mrc_qp_attr));
mrc_qp_attr.mpr.mpr_dest         = 8;
mrc_qp_attr.mpr.dynamic_mpr_dest = 0;
mrc_qp_attr.wimm.max_wimm_dest   = 128;
/* set MRC QP attributes mask... */
mrc_qp_attr_mask  = 0;
mrc_qp_attr_mask |= MRC_QP_MPR_DEST;
mrc_qp_attr_mask |= MRC_QP_DYNAMIC_MPR_DEST;
mrc_qp_attr_mask |= MRC_QP_MAX_WIMM_DEST;

if (mrc_modify_qp(mrc_qp,
                  &ibv_qp_attr, ibv_qp_attr_mask,
                  &mrc_qp_attr, mrc_qp_attr_mask) != 0)
    return ERROR;
```

---

## Modify the MRC QP to the RTS State

The next state change is from `RTR` to `RTS`. Note that after a successful
change to `RTS`, send work requests can be posted to the `SQ`.

```c
/* fill in QP attributes... */
memset(&ibv_qp_attr, 0, sizeof(ibv_qp_attr));
ibv_qp_attr.state = IBV_QPS_RTS;
/* set QP attributes mask... */
ibv_qp_attr_mask  = 0;
ibv_qp_attr_mask |= IBV_QP_STATE;

/* fill in MRC QP attributes... */
memset(&mrc_qp_attr, 0, sizeof(mrc_qp_attr));
mrc_qp_attr.mpr.mpr               = 8;
mrc_qp_attr.wimm.max_wimm         = 128;
mrc_qp_attr.timeout               = 8;
mrc_qp_attr.retry.retry_cnt_fixed = 8;
mrc_qp_attr.retry.retry_cnt_exp   = 32;
/* set MRC QP attributes mask... */
mrc_qp_attr_mask  = 0;
mrc_qp_attr_mask |= MRC_QP_MPR;
mrc_qp_attr_mask |= MRC_QP_MAX_WIMM;
mrc_qp_attr_mask |= MRC_QP_TIMEOUT;
mrc_qp_attr_mask |= MRC_QP_RETRY_CNT;

if (mrc_modify_qp(mrc_qp,
                  &ibv_qp_attr, ibv_qp_attr_mask,
                  &mrc_qp_attr, mrc_qp_attr_mask) != 0)
    return ERROR;
```

---

## Querying an MRC QP

An MRC QP can be queried at any time (i.e., in any state) after creation to
get its currently configured values.

```c
/* clear the QP attributes... */
memset(&ibv_qp_attr, 0, sizeof(ibv_qp_attr));
/* set QP attributes mask... */
ibv_qp_attr_mask  = 0;
ibv_qp_attr_mask |= IBV_QP_STATE;

/* clear the MRC QP attributes... */
memset(&mrc_qp_attr, 0, sizeof(mrc_qp_attr));
/* set MRC QP attributes mask... */
mrc_qp_attr_mask  = 0;
mrc_qp_attr_mask |= MRC_QP_MAX_WIMM;
mrc_qp_attr_mask |= MRC_QP_MAX_WIMM_DEST;
mrc_qp_attr_mask |= MRC_QP_MPR;
mrc_qp_attr_mask |= MRC_QP_MPR_DEST;
mrc_qp_attr_mask |= MRC_QP_DYNAMIC_MPR_DEST;
mrc_qp_attr_mask |= MRC_QP_TIMEOUT;
mrc_qp_attr_mask |= MRC_QP_EV_PROFILE_ID;
mrc_qp_attr_mask |= MRC_QP_HINT;
mrc_qp_attr_mask |= MRC_QP_RETRY_CNT;
mrc_qp_attr_mask |= MRC_QP_VENDOR_CFG;

/* clear the MRC QP init attributes... */
memset(&mrc_qp_init_attr, 0, sizeof(mrc_qp_init_attr));

if (mrc_query_qp(mrc_qp,
                 &ibv_qp_attr, ibv_qp_attr_mask,
                 &mrc_qp_attr, mrc_qp_attr_mask,
                 &mrc_qp_init_attr) != 0)
    return ERROR;
```

---

## Destroying Resources

```c
if (mrc_destroy_qp(qp))
    return ERROR;

if (mrc_destroy_cq(cq)
    return ERROR;

if (mrc_destroy_context(mrc_ctx))
    return ERROR;
```

