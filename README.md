# NDN Hello World

![Language](https://img.shields.io/badge/C%2B%2B-17-blue)

The NDN Hello World tool was designed to help learning and start experimenting with NDN.


## Prerequisites

Compiling and running ndn-helloworld requires the following dependencies:

1. [ndn-cxx and its dependencies](https://docs.named-data.net/ndn-cxx/current/INSTALL.html)
2. [NDN Forwarding Daemon (NFD)](https://docs.named-data.net/NFD/current/INSTALL.html)

## Compilation & Installation

```shell
./waf configure
./waf
sudo ./waf install
```

## Command Line Options

### `ndn-helloworld-server`

    Usage: ndn-helloworld-server [options]

    Respond to Hello World Interests.

    Options:
      -h [ --help ]                 print this help message and exit
      -c [ --count ] arg            maximum number of Interests to respond to
      -d [ --delay ] arg (=0)       wait this amount of milliseconds before responding to each Interest
      -q [ --quiet ]                turn off logging of Interest reception and Data generation

### `ndn-helloworld-client`

    Usage: ndn-hellworld-client [options]

    Generate Interest for Hello World.
    Interests are continuously generated unless a total number is specified.

    Options:
      -h [ --help ]                 print this help message and exit
      -c [ --count ] arg            total number of Interests to be generated
      -i [ --interval ] arg (=1000) Interest generation interval in milliseconds
      -q [ --quiet ]                turn off logging of Interest generation and Data reception
      -v [ --verbose ]              log additional per-packet information

* These tools need not be used together and can be used individually as well.

## Example

#### ON MACHINE #1

(NFD must be running)

Start the hello world server:

```shell
ndn-helloworld-server
```

#### ON MACHINE #2

(NFD must be running)

Start the hello world client:

```shell
ndn-helloworld-client
```

## License

ndn-helloworld is free software distributed under the GNU General Public License version 3.
See [`COPYING.md`](COPYING.md) for details.
