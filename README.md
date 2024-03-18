Flower route
============

Flower-route synchronizes a routing table to a `TC-flower` offload-capable NIC.

Example architecture:
```
*----------*   *-----------*
| e-Switch *---* Linux box |
*-*--------*   *-----------*
  | Nvidia ConnectX-5+ (ASAP²-capable)
  |
  | (tagged VLANs)
  |
*-*---------*   *---------------*
| L2 switch *---*  internal L3  |
*-*-*-*-----*   | routing cloud |
  | | |         *---------------*
  | | *-- PNI
  | |
  | *-- IX
  |
  *-- Transit
```

Supported hardware and software
------------------

Currently, we've only tested with NVIDIA Mellanox Connect-X 5 and 6, and should work with ASAP²-capable NICs,
but others might work too.
Every device have a different way to enable TC offload, a script for doing it for `mlx5` devices, can be found in [prepare-mlx5.sh](scripts/prepare-mlx5.sh).

The flower route have initially been build for [BIRD2](https://bird.network.cz/), but should support other BGP daemons that interact with IPRoute2 (the `ip` tool).

Build
-----

Required packages for building
- make
- libev
- clang

Required packages for development
- check
- gdb
- valgrind
- libc6-dbg / glibc-debuginfo

**Debian**
```shell
apt install build-essential make libev-dev clang check gdb valgrind libc6-dbg
```

**Arch Linux**
```shell
pacman install build-essential make libev clang check gdb valgrind
```

Run the following to build flower-route
```shell
make build
```


Options
-------

```
$ ./flower-routed --help
usage: ./flower-routed [OPTIONS]

flower-route syncronizes a routing table toa TC-flower offload-capable NIC

The following NIC's are currently Known to work:
- Mellanox ConnectX-4 onwards (those that have "ASAP²")
     (Currently only tested on Connect-X 5 and 6 Dx)

Options:
        -i, --iface <iface>               install offload rules on interface
        -t, --table <table>               routing table to syncronize with
        -p, --add-prefix <list> <prefix>  add static prefix
        -P, --load-prefix <list> <file>   load static prefixes from file
        -s, --scan-interval <secs>        time between netlink scans (dft: 10s)
        -T, --timeout <secs>              run for <n> seconds, and then exit
        -1, --one-off                     just sync once, and then exit
            --skip-hw                     for testing without hardware
            --dry-run                     don't make any changes to TC
        -v, --verbose                     increase verbosity
            --version                     show version
        -h, --help                        show this help text
```

TODO
----

Please check out the [TODO](TODO.md) list, patches are welcome.

Resources
---------

- The talk on flower-route at [FOSDEM 2024](https://fosdem.org/2024/schedule/event/fosdem-2024-3337-flying-higher-hardware-offloading-with-bird/), by Asbjørn Sloth Tønnesen, gives some more insight into how the flower route works and its intents. ([slides](https://fosdem.org/2024/events/attachments/fosdem-2024-3337-flying-higher-hardware-offloading-with-bird/slides/22273/flower-routed-fosdem24_gUABpPa.pdf))
- Linux Traffic Control: [`man tc`](https://www.man7.org/linux/man-pages/man8/tc.8.html) and [Arch wiki](https://wiki.archlinux.org/title/Advanced_traffic_control)
- Linux flow based traffic control filter: [`tc-flower`](https://www.man7.org/linux/man-pages/man8/tc-flower.8.html) and [`tc-flow`](https://www.man7.org/linux/man-pages/man8/tc-flow.8.html)

License
-------

flower-route is licensed under GPLv2 or later.
