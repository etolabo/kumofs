kumofs
======

## Overview

kumofs is a distributed key-value storage system.

  - Data is replicated over multiple servers.
  - Data is partitioned over multiple servers.
  - Single node performance is good; comparable with memcached.
  - Both read and write performance got improved as servers added.
  - Servers can be added without stopping the system.
  - Servers can be added without changing the client applications.
  - The system does not stop even if one or two servers crashed.
  - The system does not stop to recover crashed servers.
  - Scalable from 2 to 60 servers. (more than 60 servers has not be tested yet)
  - Optimized for storing a large amount of small data.
  - memcached protocol support. (get, set and delete only; expiration time and flags must be 0)

See following URLs for more details:

  - http://etolabo.github.com/...  (documents)
  - http://etolabo.github.com/...  (documents; japanese)
  - http://etolabo.github.com/...  (source code)


## Installation

Following libraries are required to build kumofs:

  - linux >= 2.6.18
  - g++ >= 4.1
  - ruby >= 1.8.7
  - zlib
  - libcrypto (openssl)
  - Tokyo Cabinet >= 1.4.10
      http://tokyocabinet.sourceforge.net/spex-en.html#installation
  - MessagePack for Ruby >= 0.3.1
      http://msgpack.sourceforge.jp/ruby:install
  - MessagePack for C++ >= 0.3.1
      http://msgpack.sourceforge.jp/cpp:install
  - Ragel >= 6.3
      http://www.complang.org/ragel/


Configure and install in the usual way:

    $ ./bootstrap  # if needed
    $ ./configure
    $ make
    $ sudo make install


## Example

    [on mgr1]$ kumo-manager -v -l mgr1 -p mgr2
    [on mgr2]$ kumo-manager -v -l mgr2 -p mgr1
    [on svr1]$ kumo-server  -v -l svr1 -m mgr1 -p mgr2 -s /var/kumodb.tch
    [on svr2]$ kumo-server  -v -l svr2 -m mgr1 -p mgr2 -s /var/kumodb.tch
    [on svr3]$ kumo-server  -v -l svr3 -m mgr1 -p mgr2 -s /var/kumodb.tch
    [       ]$ kumoctl mgr1 attach
    [on cli1]$ kumo-gateway -v -m mgr1 -p mgr2 -t 11211
    [on cli1]$ # have fun with memcached client on 11211/tcp

### Run on single host

    [localhost]$ kumo-manager -v -l localhost
    [localhost]$ kumo-server  -v -m localhost -l localhost:19801 -L 19901 -s ./kumodb1.tch
    [localhost]$ kumo-server  -v -m localhost -l localhost:19802 -L 19902 -s ./kumodb2.tch
    [localhost]$ kumo-server  -v -m localhost -l localhost:19803 -L 19902 -s ./kumodb3.tch
    [localhost]$ kumoctl localhost attach
    [localhost]$ kumo-gateway -v -m localhost -t 11211
    [localhost]$ # have fun with memcached client on 11211/tcp



## License

    Copyright (C) 2009 Etolabo Corp.
    
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

