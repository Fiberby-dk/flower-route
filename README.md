Flower route
============

flower-route syncronizes a routing table toa TC-flower offload-capable NIC

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

Supported hardware
------------------

Currently we have only tested with Connect-X 5 and 6, but other might work too. YMMV.

Every device have a different way to enable TC offload, a script for doing it for `mlx5` devices, can be found in [prepare-mlx5.sh](scripts/prepare-mlx5.sh).


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

Please check out our [TODO](TODO.md) list, patches are welcome.

License
-------

flower-route is licensed under GPLv2 or later.
