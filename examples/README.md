
# MRC API Examples

This directory contains example code demonstrating how to use the MRC APIs for RDMA programming. These examples cover both application-level APIs (`mrc.h`) and controller-level APIs (`mrc_ctl.h`).

## Application API Examples

- [MRC QP Management](mrc_qp.md)
    - Demonstrates how to create and configure MRC QPs for multipath RDMA communication. Covers device querying, QP creation, state transitions, and basic usage patterns.

## Controller API Examples

- [Explicit EV Profiles](mrc_ctl_exp_ev.md)
    - Shows how to configure EV profiles with explicitly specified entropy values.
        - Creating EV Format profiles for STEV, SRv6, and SRv6+SRH
        - Managing explicit EV arrays for different EV types
        - Profile state transitions and EV state management
        - Querying and modifying EV profiles

- [Generated EV Profiles](mrc_ctl_gen_ev.md)
    - Demonstrates how to configure EV profiles that automatically generate entropy values based on field definitions.
        - Defining generation field structures
        - Creating generated EV profiles for STEV, SRv6, and SRv6+SRH
        - Querying generated EV arrays
        - Denying subsets of generated EVs

- [Sending and Receiving EV Probes](mrc_ctl_ev_probes.md)
    - Shows how to use EV probes to measure connectivity and path quality.
        - Sending probe requests for STEV, SRv6, and SRv6+SRH paths
        - Processing probe responses and RTT measurements
        - Handling probe timeouts

- [Receiving EV State Change Events](mrc_ctl_ev_events.md)
    - Demonstrates how to register for and process EV state change events.
        - Creating EV event completion queues
        - Registering event masks on EV profiles
        - Polling and processing events for STEV, SRv6, and SRv6+SRH
        - Understanding drop counts and event overflow

## EV Expansion

- Sample application that demonstrates EV expansions of various EV Format profile and EV profile configurations (`./ev_expansion/*`)

## Benchmark Application

- Functional MRC Application (`./benchmark/*`)

