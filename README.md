# kumofs

## Overview

kumofs is a high-performance distributed key-value storage system.


## Features and Data Model

  - Data is replicated over multiple servers
  - Run normally even if some servers are crashed
  - Both write and read performance raises as servers are added
  - Servers can be added without stopping the system
  - Scale from 2 to 60 servers (more than 60 servers has not be tested yet)
  - Suitable for storing a large amount of small data
  - memcached protocol support

kumofs supports following 3 operations:

**Set(key, value)**
Store the key-value pair.

**value = Get(key)**
Retrive the associated value of the key.

**Delete(key)**
Delete the key and the associated value.


## Installation

Following environment is required to build kumofs:

  - linux >= 2.6.18
  - [Tokyo Cabinet](http://tokyocabinet.sourceforge.net/) >= 1.4.10
  - [MessagePack for Ruby](http://msgpack.sourceforge.jp/ruby:install.ja) >= 0.3.1
  - [MessagePack for C++](http://msgpack.sourceforge.jp/cpp:install.ja) >= 0.3.1
  - [Ragel](http://www.complang.org/ragel/) >= 6.3
  - g++ >= 4.1
  - ruby >= 1.8.7
  - libcrypto (openssl)
  - zlib

Configure and install in the usual way:

    $ ./configure
    $ make
    $ sudo make install


## Tutorial

kumofs system consists of following 3 daemons:

**kumo-server** this node stores data. Run this daemon on least one hosts. You can add kumo-servers after building the cluster.

**kumo-manager** this node manages kumo-servers. Run this daemon on one or two hosts.

**kumo-gateway** this daemon receives requests from applications and relays it to the kumo-servers. Run this daemon on the hosts that runs applications that uses kumofs.

This tutorial explains how to build kumofs cluster using 3 hosts named *svr1,svr2,svr3.* Run kumo-manager on *svr1* and *svr2* and run kumo-server on *svr1,svr2,svr3.* Then use the kumofs cluster from client host named *cli1.*


### Run kumo-manager

First, run kumo-manager *svr1* and *svr2.*
kumo-manager requires IP address of its host and IP address of the another kumo-manager.

    [svr1]$ kumo-manager -v -l svr1 -p svr2
    [svr2]$ kumo-manager -v -l svr2 -p svr1

kumo-manager listens on 19700/tcp by default.


### Run kumo-server

Second, run kumo-server on *svr1,svr2,svr3.*
kumo-server requires IP address of its host, IP address of the kumo-managers and path to the database file.

    [svr1]$ kumo-server -v -l svr1 -m svr1 -p svr2 -s /var/kumodb.tch
    [svr2]$ kumo-server -v -l svr2 -m svr1 -p svr2 -s /var/kumodb.tch
    [svr3]$ kumo-server -v -l svr3 -m svr1 -p svr2 -s /var/kumodb.tch


### Attach kumo-server

Third, attach kumo-servers into the cluster using **kumoctl** command.
Confirm that the kumo-servers are recognized by the kumo-manager first:

    $ kumoctl svr1 status
    hash space timestamp:
      Wed Dec 03 22:15:55 +0900 2008 clock 62
    attached node:
    not attached node:
      192.168.0.101:19800
      192.168.0.102:19800
      192.168.0.103:19800

Recognized kumo-servers are listed at **not attached node.** Then, attach them as follows:

    $ kumoctl svr1 attach

Attached kumo-servers are listed at **attached node.** Confirm it as follows:

    $ kumoctl svr1 status
    hash space timestamp:
      Wed Dec 03 22:16:00 +0900 2008 clock 72
    attached node:
      192.168.0.101:19800  (active)
      192.168.0.102:19800  (active)
      192.168.0.103:19800  (active)
    not attached node:


### Run kumo-gateway

Finally, run kumo-gateway on the hosts that runs applications that uses kumofs.
kumo-gateway requres IP address of the kumo-managers and port number that accepts memcached protocol client.

    [cli1]$ kumo-gateway -v -m svr1 -p svr2 -t 11211

It is ready to use kumofs cluster.  Connect **localhost:11211** using memcached client on *cli1*.


## HowTo

### Add new server into the running system

To add a server into the system that is built in the tutorial, run kumo-server on another host, and then attach it using **kumoctl** command.

    [svr4]$ kumo-server -v -l svr4 -m svr1 -p svr2 -s /var/kumodb.tch
    $ kumoctl svr1 attach

### Run all daemons on the one server

    [localhost]$ kumo-manager -v -l localhost
    [localhost]$ kumo-server  -v -m localhost -l localhost:19801 -L 19901 -s ./database1.tch
    [localhost]$ kumo-server  -v -m localhost -l localhost:19802 -L 19902 -s ./database2.tch
    [localhost]$ kumo-server  -v -m localhost -l localhost:19803 -L 19902 -s ./database3.tch
    [localhost]$ kumo-gateway -v -m localhost -t 11211

### Show status of the nodes

You can get status of the cluster from from kumo-manager.
Use **kumoctl** command to get the status. Specify IP address of the kumo-manager for first argument and 'status' for second argument.

    $ kumoctl svr1 status
    hash space timestamp:
      Wed Dec 03 22:15:45 +0900 2008 clock 58
    attached node:
      192.168.0.101:19800  (active)
      192.168.0.102:19800  (active)
      192.168.0.103:19800  (active)
      192.168.0.104:19800  (fault)
    not attached node:
      192.168.0.104:19800

**hash space timestamp** The time that the list of attached kumo-servers is updated. It is updated when new kumo-server is added or existing kumo-server is down.
**attached node** The list of attached kumo-servers. **(active)** is normal node and **(fault)** is fault node or recoverd but not re-attached node.
**not attached node** The list of recognized but not-attached nodes.

### Show load of the nodes

kumo-server is the node that stores data. You can get load information such as the number of items that is stored on the node using **kumostat** command.

    $ kumostat svr3 items
    $ kumostat svr3 cmd_get

**uptime** uptime of the kumo-server
**version** version of the kumo-server
**cmd_get** total number of processed get requests
**cmd_set** total number of processed set requests
**cmd_delete** total number of processed delete requests
**items** number of stored items

    $ kumotop -m svr1


### Look up which node stores the key

kumofs distributes key-value pairs into the multiple servers. **kumohash** command look up which node stores the key.

    $ kumohash -m svr1 assign "the-key"


## Troubleshooting

### one or two kumo-servers are down

First, confirm that which of kumo-servers are down:

    [xx]$ kumoctl m1 status    # m1 is address kumo-manager
    hash space timestamp:
      Wed Dec 03 22:15:35 +0900 2008 clock 50
    attached node:
      192.168.0.101:19800  (active)
      192.168.0.102:19800  (active)
      192.168.0.103:19800  (active)
      192.168.0.104:19800  (fault)
    not attached node:

**(fault)** nodes are down.

Second, reboot the host and run kumo-server. Before running kumo-server, remove database file. Otherwise deleted keys may be restored.
The status of cluster will be as following:

    [xx]$ kumoctl m1 status
    hash space timestamp:
      Wed Dec 03 22:15:45 +0900 2008 clock 58
    attached node:
      192.168.0.101:19800  (active)
      192.168.0.102:19800  (active)
      192.168.0.103:19800  (active)
      192.168.0.104:19800  (fault)
    not attached node:
      192.168.0.104:19800

Finally, *attach* the kumo-server:

    [xx]$ kumoctl m1 attach
    [xx]$ kumoctl m1 status
    hash space timestamp:
      Wed Dec 03 22:15:55 +0900 2008 clock 62
    attached node:
      192.168.0.101:19800  (active)
      192.168.0.102:19800  (active)
      192.168.0.103:19800  (active)
      192.168.0.104:19800  (active)
    not attached node:


## Performance and Tuning
To get best performance, create database file using *tchmgr* command before starting kumo-server. See [document of Tokyo Cabinet](http://tokyocabinet.sourceforge.net/spex-en.html) for details.

Example:

    [svr1]$ tchmgr create /var/kumodb.tch 1310710
    [svr1]$ kumo-server -v -l svr1 -m mgr1 -p mgr2 -s /var/kumodb.tch

