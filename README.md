**This repository is a companion to OCP Multipath Reliable Connection Specification.**  
**FIXME \- add link when published.**

The software APIs are broken into two different categories:Application API and Controller API. 

**MRC Application API (**mrc.h**)** This API is utilized by RDMA applications (such as NCCL, and RCCL) and is directly modeled after the `libibverbs` direct verbs API design for creating, modifying, and destroying resources. Its primary functions include:

* **Device and Resource Management:** Allows the application to query MRC device capabilities, allocate MRC Queue Pairs (QPs), and create Completion Queues (CQs) and completion channels.  
* **Work Requests & Events:** Enables the application to post Send and Receive work requests to the MRC QP and handles MRC asynchronous error events.  
* **Protocol Restrictions:** MRC API only supports Write and Write IMM operations. **READ, SEND, or ATOMIC operations are not** permitted. Additionally, buffers for IMM operations are expected to be posted before the application starts. Therefore, RNR (Receiver Not Ready) and RNR is not supported and the hardware must ensure that a maximum configured IMM operations are outstanding. 

**MRC Controller API (**mrc\_ctl.h**)** This API is designed for a single privileged controller application per node running with `CAP_NET_ADMIN` capabilities. The controller can run as a background daemon or a command-line tool and is responsible for managing underlying persistent device states. It is also possible to integrate the controller APIs directly into an application. Its primary functions include:

* **Entropy Value (EV) Management:** Configures **EV Profiles** (which dictate how packets are sprayed across multiple network paths using modes like explicitly defined EVs, NIC-generated EVs, or SRv6). It also modifies individual EV states (e.g., DENY) and processes asynchronous EV events when the NIC detects bad network paths.  
* **EV Probes:** Sends connectionless, out-of-band probes to test specific network paths to destinations without tying them to a specific user QP.  
* **Congestion Control (CC) Management:** Defines and configures CC profiles for MRC QPs, defaulting to the NSCC algorithm while allowing vendors to expose custom congestion control algorithms.  
* **Capability Discovery:** Queries the device for vendor-specific MRC capabilities to guide how EV and CC profiles should be safely configured.

**Integration Notes:** The MRC library relies on `libibverbs` for the initial device discovery and `ibv_context` management when opening devices. Furthermore, the user application (using `mrc.h`) and the privileged controller (using `mrc_ctl.h`) are expected to communicate out-of-band so the user application knows which EV and CC profiles have been configured and can associate them with its QPs during initialization