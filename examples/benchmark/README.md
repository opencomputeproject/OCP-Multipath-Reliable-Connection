# MRC RDMA Write Benchmark

RDMA write performance benchmark using LIB MRC API that sweeps across multiple message sizes to measure bandwidth and message rate characteristics.


## Prerequisites

* **libibverbs** headers/libs (e.g., via `rdma-core`)
* **MRC headers** available and pointed to by `MRC_H_PATH`
  * Default assumption: headers live at parent dir
* **MRC library** (`lib_mrc.so`) in the same directory as headers
  * Default assumption: headers live at parent dir
* **RDMA-capable NICs** (e.g., ConnectX adapters)
* **Network connectivity** between client and server nodes

---

## Build 

```bash
make clean && make
```

This produces the `mrc_example` binary.

---

## Run

The binary uses standard command-line options for flexible configuration:

```
Usage:
  ./mrc_example            start a server and wait for connection
  ./mrc_example <host>     connect to server at <host>

Options:
  -p, --port=<port>      listen on/connect to port <port> (default 18515)
  -d, --ib-dev=<dev>     use IB device <dev> (default first device found)
  -i, --ib-port=<port>   use port <port> of IB device (default 1)
  -g, --gid-idx=<idx>    GID index (default 3)
  -m, --mtu=<mtu>        path MTU (default 4096 bytes)
  -n, --iters=<iters>    number of exchanges per message size (default 1000)
  -s, --size=<size>      maximum message size in bytes (default 67108864)
      --min-size=<size>  minimum message size for sweep (default 1)
      --step-factor=<n>  message size step factor for sweep (default 2)
  -w, --warmup-iters=<n> number of warmup iterations (default 1)
  -h, --help             display usage information

Note: Benchmark sweeps message sizes from min-size up to size (multiply by step-factor each iteration)

MTU values: Valid values are 256, 512, 1024, 2048, or 4096 bytes.
            Different hardware vendors may have different recommended MTUs.
```

**Key Features:**
* Options can be specified in any order
* Both short (`-p`) and long (`--port=`) option formats supported
* Server mode: run without hostname argument
* Client mode: provide server hostname/IP as positional argument
* Use `ibv_devices` to list available RDMA devices

### Examples

**Server mode** (listen on default port 18515, use first available device):

```bash
./mrc_example
```

**Server mode** (custom port and device, 2000 iterations, sweep to 128MB):

```bash
./mrc_example -p 18181 -d mlx5_0 -n 2000 -s 134217728
```

**Server mode** (with custom MTU for specific hardware):

```bash
./mrc_example -m 2048 -d mlx5_0
```

**Server mode** (custom sweep range - 1KB to 16MB with x4 steps):

```bash
./mrc_example --min-size=1024 --size=16777216 --step-factor=4
```

**Server mode** (custom IB port and GID index):

```bash
./mrc_example -i 2 -g 5 -d mlx5_1
```

**Client mode** (connect to server at `10.0.4.123` on default port):

```bash
./mrc_example 10.0.4.123
```

**Client mode** (full options specified):

```bash
./mrc_example -p 18181 -d mlx5_0 -n 2000 -s 134217728 10.0.4.123
```

**Using long options:**

```bash
# Server
./mrc_example --port=18181 --ib-dev=mlx5_0 --iters=2000 --size=134217728

# Client
./mrc_example --port=18181 --ib-dev=mlx5_0 --iters=2000 --size=134217728 10.0.4.123
```

The benchmark will automatically sweep message sizes from min-size to size, multiplying by step-factor each iteration (default: 1, 2, 4, 8, 16, ...).


### Quick Test (default settings)

**Server:**
```bash
numactl -N 0 ./mrc_example
```

**Client:**
```bash
numactl -N 0 ./mrc_example 10.0.4.123
```

This will run 1000 iterations per message size, sweeping from 1 byte to 64MB on port 18515.

## Logging

Set the log level via environment variable:

```bash
export MRC_LOG_LEVEL=DEBUG  # DEBUG, INFO, WARN, ERROR (0-3)
./mrc_example server 18181 mlx5_0
```

Levels:
- **ERROR (0)**: Only errors
- **WARN (1)**: Warnings and errors
- **INFO (2)**: General information (recommended)
- **DEBUG (3)**: Detailed operation logging


### Example Script for build and run with INFO log level
```bash
#!/bin/bash

# Configuration
is_server=${1:-0}
server_ip=${2:-10.0.4.123}
port_num=${3:-18515}
nic_dev=${4:-mlx5_0}
iterations=${5:-1000}
msg_size=${6:-67108864}

# Build
make clean && make

# Run with options
export MRC_LOG_LEVEL=INFO

if [[ "$is_server" == "1" ]]; then
    # Server mode
    ./mrc_example -p $port_num -d $nic_dev -n $iterations -s $msg_size
else
    # Client mode
    ./mrc_example -p $port_num -d $nic_dev -n $iterations -s $msg_size $server_ip
fi
```

Usage:
```bash
# Run as server
./script.sh 1

# Run as client connecting to 10.0.4.123
./script.sh 0 10.0.4.123

# Run as client with custom settings
./script.sh 0 192.168.1.100 18181 mlx5_1 2000 134217728
```

### Output Columns

* **MsgSize**: Message size in bytes for this test iteration
* **TotalOps**: Total number of RDMA write operations completed (= iterations)
* **BW_avg(Gbps)**: Average bandwidth in Gigabits per second
* **MsgRate(Mpps)**: Message rate in Millions of messages per second

The sweep demonstrates bandwidth scaling from small messages (limited by message rate) to large messages.
