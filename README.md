kumofs
======
[http://kumofs.sourceforge.net/](http://kumofs.sourceforge.net/)

## Overview

Kumofs is a simple and fast distributed key-value store.

You can use a memcached client library to set, get, CAS or delete values from/into kumofs.

Backend storage is Tokyo Cabinet and it will give you great performance.

  - Data is partitioned and replicated over multiple servers.
  - Extreme single node performance; comparable with memcached.
  - Both read and write performance got improved as servers added.
  - Servers can be added without stopping the system.
  - Servers can be added without modifying any configuration files.
  - The system does not stop even if one or two servers crashed.
  - The system does not stop to recover crashed servers.
  - Automatic rebalancing support with a consistency control algorithm.
  - Safe CAS operation support.
  - memcached protocol support.

![Single node performance of kumofs](http://kumofs.sourceforge.net/index/speedtest.png)

It measured performance of one server node using three client machines. Each client machine gets 12,800 of 1KB values from the server using 32 threads. The source code is available from frsyuki's repository. ([kumofs](http://github.com/frsyuki/memstrike), [voldemort](http://github.com/frsyuki/memstrike-voldemort)).

![Scalability of kumofs](http://kumofs.sourceforge.net/index/scalability.png)

It measured performance of the cluster using 50 client machines. Each client machine gets 1,024,000 entries form the cluster using 32 threads.


See following URLs for more details:

  - [Project Website](http://kumofs.sourceforge.net/)
  - [document, English](http://github.com/etolabo/kumofs/blob/master/doc/doc.en.md)
  - [document, Japanese](http://github.com/etolabo/kumofs/blob/master/doc/doc.ja.md)
  - [blog, Japanese](http://d.hatena.ne.jp/viver/20100118/p1)
  - [source code](http://github.com/etolabo/kumofs/)


## Installation

Following libraries are required to build kumofs:

  - linux >= 2.6.18
  - g++ >= 4.1
  - ruby >= 1.8.6
  - [Tokyo Cabinet](http://1978th.net/tokyocabinet/) >= 1.4.10
  - [MessagePack for C++](http://msgpack.sourceforge.jp/c:install) >= 0.3.1
  - [MessagePack for Ruby](http://msgpack.sourceforge.jp/ruby:install) >= 0.3.1
  - zlib
  - libcrypto (openssl)


Configure and install in the usual way:

    $ ./bootstrap  # if needed
    $ ./configure
    $ make
    $ sudo make install


## Example

This example runs kumofs on 6-node cluster. Run *kumo-manager* on **mgr1** and **mgr2**, *kumo-server* on **svr1**, **svr2** and **svr3**, then run *kumo-gateway* on **cli1**.

    [on mgr1]$ kumo-manager -v -l mgr1 -p mgr2
    [on mgr2]$ kumo-manager -v -l mgr2 -p mgr1
    [on svr1]$ kumo-server  -v -l svr1 -m mgr1 -p mgr2 -s /var/kumodb.tch
    [on svr2]$ kumo-server  -v -l svr2 -m mgr1 -p mgr2 -s /var/kumodb.tch
    [on svr3]$ kumo-server  -v -l svr3 -m mgr1 -p mgr2 -s /var/kumodb.tch
    [       ]$ kumoctl mgr1 attach
    [on cli1]$ kumo-gateway -v -m mgr1 -p mgr2 -t 11211
    [on cli1]$ # use memcached client on 11211/tcp

See documents for details.


### Run on single host

This example runs kumofs on single host.

    [localhost]$ kumo-manager -v -l localhost
    [localhost]$ kumo-server  -v -m localhost -l localhost:19801 -L 19901 -s ./kumodb1.tch
    [localhost]$ kumo-server  -v -m localhost -l localhost:19802 -L 19902 -s ./kumodb2.tch
    [localhost]$ kumo-server  -v -m localhost -l localhost:19803 -L 19902 -s ./kumodb3.tch
    [localhost]$ kumoctl localhost attach
    [localhost]$ kumo-gateway -v -m localhost -t 11211
    [localhost]$ # have fun with memcached client on 11211/tcp


## License

    Copyright (C) 2009-2010 FURUHASHI Sadayuki
    
       Licensed under the Apache License, Version 2.0 (the "License");
       you may not use this file except in compliance with the License.
       You may obtain a copy of the License at
    
           http://www.apache.org/licenses/LICENSE-2.0
    
       Unless required by applicable law or agreed to in writing, software
       distributed under the License is distributed on an "AS IS" BASIS,
       WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
       See the License for the specific language governing permissions and
       limitations under the License.

See also NOTICE file.

