
# zrp

zrp是一个用modern c++实现的tcp内网穿透工具。一种典型的使用场景是，可以将运行在本地的服务器通过一台拥有公网ip的服务器暴露至公网。

## 使用方法

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

首先需要在公网服务器上运行zserver。使用以下命令可以将示例配置写入到文件：

```
$ zserver dump_config
{
    "server_host" : "0.0.0.0",
    "server_port" : 11433
}
$ ZRP_NOCOLOR=1 zserver dump_config > zserver.json
```

按照需要编辑好配置文件后，即可运行服务器：

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

和zserver差不多，需要根据需求编辑配置：

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

示例配置的意思是将本地22端口的服务器暴露在公网ip的9022端口，将8080端口的服务器暴露在公网ip的8080端口，可以按照自己的需求修改配置。

运行：

```
$ zclient run ./zserver.json
```

此时，访问zserver所在ip的指定端口的请求，将被穿透到zclient所在内网。

## license

BSL-1.0

## 依赖

 * cmake
 * 较新版本的编译器，支持c++20 coroutines和concepts
 * boost 1.75+ asio、json
 * {fmt}

## 鸣谢

本项目受到[frp](https://github.com/fatedier/frp)的启发。

