
# zrp

[中文](README_zh_CN.md)

zrp is a nat-passthrough reverse proxy written in modern c++. A major use case is to expose a local server via a remote server with public IP.

## usage

### zserver

```
usage : zserver run/dump_config/help [...]

    run [path/to/config.json]    run the program
    dump_config [--full]         dump the example config
    help                         show this message

supported envs:
    ZRP_NOCOLOR                  not to add color or styling escape sequences
    ZRP_TRACE                    show logs at level TRAC
    ZRP_DEBUG                    show logs at level DEBG and TRAC

```

First, run zserver on the remote server. Use these commands to export the config to a file:

```
$ zserver dump_config
{
    "server_host" : "0.0.0.0",
    "server_port" : 11433
}
$ ZRP_NOCOLOR=1 zserver dump_config > zserver.json
```

Alter the config to your likings, and run the server:

```
$ zserver run ./zserver.json
```

### zclient

```
usage : zclient run/dump_config/help [...]

    run [path/to/config.json]    run the program
    dump_config [--full]         dump the example config
    help                         show this message

supported envs:
    ZRP_NOCOLOR                  not to add color or styling escape sequences
    ZRP_TRACE                    show logs at level TRAC
    ZRP_DEBUG                    show logs at level DEBG and TRAC

```

Like with zserver, export and edit the config:

```
$ zclient dump_config
{
    "server_host" : "192.168.0.33",
    "tcp_shares" : {
        "ssh" : {
            "local_port" : 22,
            "remote_port" : 9022
        },
        "http" : {
            "local_port" : 8080,
            "remote_port" : 8080
        }
    }
}
$ ZRP_NOCOLOR=1 zclient dump_config > zclient.json
```

The example config expose local port 22 to remote port 9022, and export local port 8080 to remote port 8080. You may have to change that.

Run the client:

```
$ zclient run ./zserver.json
```

Now access the public IP with configured port, the requests would be sent pass the NAT (if any), and to the local network zclient is running at.

## license

BSL-1.0

## dependencies

 * cmake
 * a newer c++ compiler, with support for c++20 coroutines & concepts
 * boost 1.75+ asio、json
 * {fmt}

## thanks

This project is greatly inspired by [frp](https://github.com/fatedier/frp)

