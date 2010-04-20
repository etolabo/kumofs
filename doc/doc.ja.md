# kumofs

## kumofsとは？

kumofsは、実用性を重視した分散データストアです。レプリケーション機能を備え、一部のサーバーに障害が発生しても正常に動作し続けます。単体でも高い性能を持ちながら、サーバーを追加することで読み・書き両方の性能が向上する特徴を持ち、低コストで極めて高速なストレージシステムを構築・運用できます。サーバーの追加や復旧はシステムを動かしたまま行うことができ、アプリケーションには一切影響を与えません。

  - データは複数のサーバーにレプリケーションされます
  - データは複数のサーバーに分散して保存されます
  - 単体のサーバーでも非常に高速です
  - サーバーを追加すると読み・書き両方の性能が向上します
  - サーバーを追加してもアプリケーションには影響しません
  - 一部のサーバーがダウンしても正常に動き続けます
  - システムを止めずにサーバーを追加できます
  - システムを止めずにダウンしたサーバーを復旧できます
  - 2台から60台程度までスケールします（60台以上はまだ検証されていません）
  - 小さなデータを大量に保存するのに適しています
  - memcachedプロトコルをサポートしています
    - サポートしているのはget, set, delete のみです。expireには必ず0を指定する必要があります。flagsを保存するにはkumo-gatewayに-Fオプションが必要です。


## データモデル

kumofsには *key* と *value* だけで表されるシンプルなデータを保存できます。keyとvalueは任意のバイト列です。

kumofsは以下の3つの操作をサポートしています：

**Set(key, value)**
keyとvalueのペアを保存します。１つのkey-valueペアは合計３台のサーバーにコピーされます。
Set操作が失敗すると（ネットワーク障害などの理由で）、そのkeyに対応するvalueは不定になります。そのkeyは再度Setするか、Deleteするか、Getしないようにしてください。

**value = Get(key)**
keyに対応するvalueを取得します。
Set中にGetした場合に古いvalueが取得されるか新しいvalueが取得されるかは不定ですが、新旧が混ざったvalueにはなることはありません。

**Delete(key, value)**
keyに対応するvalueを削除します。
Delete操作は実際には「削除済みフラグをSetする」操作なので、Setと同じ挙動になります。削除済みフラグは一定の時間が経過すると本当に削除されます。


## インストール

kumofsをコンパイルして実行するには、以下の環境が必要です：

  - linux &gt;= 2.6.18
  - g++ &gt;= 4.1
  - ruby &gt;= 1.8.6
  - [Tokyo Cabinet](http://1978th.net/tokyocabinet/) &gt;= 1.4.10
  - [MessagePack for C++](http://msgpack.sourceforge.jp/c:install.ja) &gt;= 0.3.1
  - [MessagePack for Ruby](http://msgpack.sourceforge.jp/ruby:install.ja) &gt;= 0.3.1
  - libcrypto (openssl)
  - zlib


Tokyo Cabinet をインストールするには、[Tokyo CabinetのWebサイト](http://1978th.net/tokyocabinet/)から最新のソースパッケージを入手して、./configure && make && make install してください。

MessagePack をインストールするには、[MessagePackのWebサイト](http://msgpack.sourceforge.jp/)から、C/C++向けの最新のソースパッケージを入手して、./configure && make && make install してください。

管理ツールは、rubyで実装されています。"msgpack"パッケージを利用しているので、RubyGemsを使ってインストールしてください。

    $ sudo gem install msgpack

kumofs のソースパッケージは、[Downloads](http://github.com/etolabo/kumofs/downloads) にあります。ダウンロードしたら、./configure && make && make install でインストールしてください。

    $ ./bootstrap  # 必要な場合
    $ ./configure
    $ make
    $ sudo make install

MessagePack や Tokyo Cabinet をインストールしたディレクトリによっては、./configure に --with-msgpack オプションや --with-tokyocabinet オプションを追加する必要があるかも知れません。

    $ ./bootstrap
    $ ./configure --with-msgpack=/usr/local --with-tokyocabinet=/opt/local
    $ make
    $ sudo make install

MessagePack や Tokyo Cabinet をインストールしたディレクトリによっては、実行時に LD_LIBRARY_PATH 環境変数を設定する必要があるかも知れません。

    $ export LD_LIBRARY_PATH=/usr/local/lib:/opt/local/lib
    $ /usr/local/bin/kumo-manager --help

## チュートリアル

kumofsクラスタは３種類のデーモンで構成されます：

**kumo-server** 実際にデータを保存するノード。少なくとも1台必要です。後から追加できます。

**kumo-manager** kumo-server群を管理するノード。1台または2台で動かします。

**kumo-gateway** アプリケーションからのリクエストをkumo-serverに中継するプロキシ。アプリケーションを動かすホスト上で、１つずつ起動しておきます。

ここでは３台のサーバー *svr1,svr2,svr3* を使ってkumofsのクラスタを構築する方法を紹介します。*svr1* と *svr2* でkumo-managerを起動し、*svr1,svr2,svr3* でkumo-serverを起動します。それから別のクライアント*cli1*からデータを保存･取得してみます。


### kumo-managerを起動する

まず最初に、*svr1* と *svr2* の２台のサーバーで、kumo-managerを起動します。
kumo-managerの引数には、自分のホスト名かIPアドレスと、もう１台のkumo-managerのホスト名かIPアドレスを指定します：

    [on svr1]$ kumo-manager -v -l svr1 -p svr2
    [on svr2]$ kumo-manager -v -l svr2 -p svr1

kumo-managerは19700/tcpをlistenします。


### kumo-serverを起動する

次に *svr1,svr2,svr3* の３台のサーバーで、kumo-serverを起動します。
kumo-serverの引数には、自分のアドレス、kumo-managerのアドレス、データベースのパスを指定します：

    [on svr1]$ kumo-server -v -l svr1 -m svr1 -p svr2 -s /var/kumodb.tch
    [on svr2]$ kumo-server -v -l svr2 -m svr1 -p svr2 -s /var/kumodb.tch
    [on svr3]$ kumo-server -v -l svr3 -m svr1 -p svr2 -s /var/kumodb.tch

kumo-serverは19800/tcpと19900/tcpをlistenします。


### kumo-serverを登録する

kumo-serverは起動しただけでは追加されません。**kumoctl** コマンドを使って登録します。
まずはkumo-serverがkumo-managerから認識されているかどうかを確認してみます：

    [       ]$ kumoctl svr1 status
    hash space timestamp:
      Wed Dec 03 22:15:55 +0900 2008 clock 62
    attached node:
    not attached node:
      192.168.0.101:19800
      192.168.0.102:19800
      192.168.0.103:19800

**not attached node** のところに３台のサーバーが認識されていることを確認したら、実際に登録します：

    [       ]$ kumoctl svr1 attach

最後に本当に登録されたか確認します：

    [       ]$ kumoctl svr1 status
    hash space timestamp:
      Wed Dec 03 22:16:00 +0900 2008 clock 72
    attached node:
      192.168.0.101:19800  (active)
      192.168.0.102:19800  (active)
      192.168.0.103:19800  (active)
    not attached node:

これでkumo-serverが登録されました。


### kumo-gatewayの起動する

最後に、kumofsを利用するアプリケーションを動かすホスト上で、kumo-gatewayを起動します。kumo-gatewayはmemcachedプロトコルを実装しているので、アプリケーションからは **localhostで動作しているmemcached** のように見えます。

kumo-gatewayの引数には、kumo-managerのアドレスと、memcachedクライアントを待ち受けるポート番号を指定します：

    [on cli1]$ kumo-gateway -v -m svr1 -p svr2 -t 11211

これでkumofsを利用する準備が整いました。kumo-gateway を起動したホスト上で、memcachedクライアントを使って **localhost:11211** に接続してみてください。


### 注意点

すべてのサーバーのシステム時刻は、できる限り同じになるようにしてください。5秒以上ずれていると、正しく動作しません。NTPを使って合わせるのがベストです。

また、memcachedクライアントの「タイムアウト時間」は、必ず長めに設定してください。元々キャッシュを前提としているライブラリがほとんどなので、デフォルトのタイムアウト時間は非常に短くなっていることが多いです。最適な値はユースケースに依存しますが、60秒くらい確保しておくと良いです。


## HowTo

### 新しいkumo-serverを追加する

チュートリアルで構築したシステムに新しいサーバー *svr4* を追加するには、まずkumo-serverを起動し、次にkumoctlコマンドを使って登録します：

    [on svr4]$ kumo-server -v -l svr4 -m svr1 -p svr2 -s /var/kumodb.tch
    [       ]$ kumoctl svr1 attach

これで新しいサーバーが追加されました。


### すべてのプロセスをlocalhostで動かす

すべてのプロセスを１台のホストで動かして試してみるには以下のようにします：

    [localhost]$ kumo-manager -v -l localhost
    [localhost]$ kumo-server  -v -m localhost -l localhost:19801 -L 19901 -s ./database1.tch
    [localhost]$ kumo-server  -v -m localhost -l localhost:19802 -L 19902 -s ./database2.tch
    [localhost]$ kumo-server  -v -m localhost -l localhost:19803 -L 19902 -s ./database3.tch
    [localhost]$ kumo-gateway -v -m localhost -t 11211


### 全ノードを再起動する

メンテナンスなどで、すべてのサーバーを落とす場合は、以下の手順で行ってください。

  - kumo-gateway をすべて落とす
  - kumo-server と kumo-manager をすべて落とす（順番は自由）
  - 落とす前と同じ構成（IPアドレス、ポート番号、台数）で、kumo-server と kumo-manager を再起動する
  - kumoctl コマンドで attach サブコマンドを発行する
  - kumo-gateway を起動する


### ノードの死活状態を表示する

kumo-managerはクラスタ全体を管理しているノードです。kumo-managerからはクラスタ全体の状態を取得できます。
クラスタの状態を取得するには、**kumoctl** コマンドを使います。第一引数にはkumo-manager（複数いる場合はどれか１台）のアドレスを指定し、第二引数には 'status' と指定してください。

    [       ]$ kumoctl svr1 status
    hash space timestamp:
      Wed Dec 03 22:15:45 +0900 2008 clock 58
    attached node:
      192.168.0.101:19800  (active)
      192.168.0.102:19800  (active)
      192.168.0.103:19800  (active)
      192.168.0.104:19800  (fault)
    not attached node:
      192.168.0.104:19800

**hash space timestamp** このkumo-managerに登録されているkumo-serverの一覧が最後に更新された時刻。サーバーを追加したときと、サーバーがダウンしたときに更新されます。

**attached node** 登録されているkumo-serverの一覧。**(active)** と表示されているノードは正常に動作しているノードで、**(fault)** と表示されているノードは障害が発生しているか、復旧してからまだ再登録されていないノードです。

**not attached node** 認識されているが登録されていないkumo-serverの一覧。


kumoctlコマンドの詳しい使い方は、リファレンスを参照してください。


### ノードの負荷を表示する

kumo-serverは実際にデータを保存しているノードです。**kumostat** コマンドを使うと、そのkumo-serverに保存されているデータの件数や、今までに処理されたGet操作の累計回数などを取得できます。

    $ kumostat svr3 items
    $ kumostat svr3 cmd_get

**uptime** kumo-serverプロセスの起動時間（単位は秒）

**version** kumo-serverのバージョン

**cmd_get** Get操作を処理した累計回数

**cmd_set** Set操作を処理した累計回数

**cmd_delete** Delete操作を処理した累計回数

**items** データベースに保存されているデータの件数

kumostatコマンドの詳しい使い方はリファレンスを参照してください。


また **kumotop** コマンドを使うと、*top* コマンドのようにkumo-serverの負荷をモニタリングできます。
**-m** に続いてkumo-managerのアドレスを指定するか、モニタしたいkumo-serverのアドレスを列挙してください：

    $ kumotop -m svr1


### あるデータがどのkumo-serverに保存されているか調べる

kumofsは、データを複数のkumo-serverに分散して保存します。あるデータが実際にどのkumo-serverに保存されているかを調べるには、**kumohash** コマンドを使います：

    $ kumohash -m svr1 assign "the-key"


### バックアップから復旧する

コールドバックアップが役に立つのは、以下のような場面です：

  - 3台以上のkumo-serverのHDDが同時に壊れた
  - そのほか何らかの原因でデータが消失した

バックアップを作成するには、*kumoctl* コマンドの **backup** サブコマンドを使います。backupを実行すると、それぞれのkumo-serverでデータベースファイルの複製が作成されるので、これをscpコマンドなどを使って１台のホストに集めます。最後に **kumomergedb** コマンドを使って１つのデータベースファイルに結合します。

バックアップから復旧するときは、バックアップしておいたデータベースファイルをすべてのサーバーに配り、kumo-serverを起動します。このとき、すべてのkumo-serverは同じデータを持っており、そのkumo-serverが持っている必要が無いデータまで持っています。その後に *kumoctl* コマンドの **attach** サブコマンドを使ってkumo-serverを登録すると、不要なデータが削除され、整合性のある状態に回復します。


## トラブルと復旧

### kumo-serverが1台か2台ダウンした

kumo-serverが1台ダウンすると、一部のkey-valueペアのレプリカが1つ減った状態のまま動作し続けます。2台ダウンすると、1つか2つ減った状態のままになります。
この状態からレプリカの数を3つに戻すには、kumo-serverを復旧させたあとkumoctlコマンドを使って再度登録するか、ダウンしたkumo-serverを完全に切り離します。

まずは、kumoctlコマンドを使ってどのkumo-serverに障害が発生しているのかを確認します：

    [       ]$ kumoctl m1 status    # m1はkumo-managerのアドレス
    hash space timestamp:
      Wed Dec 03 22:15:35 +0900 2008 clock 50
    attached node:
      192.168.0.101:19800  (active)
      192.168.0.102:19800  (active)
      192.168.0.103:19800  (active)
      192.168.0.104:19800  (fault)
    not attached node:

**(fault)** と表示されているkumo-serverに障害が発生しています。

kumo-serverを復旧させて元の台数に戻すときは、**まず古いデータベースファイルを削除するか移動させておき、その後でkumo-serverプロセスを再起動します**。古いデータベースファイルが残っていると、そのサーバーに障害が発生している間に削除したデータが復活してしまう可能性があります。
kumo-serverを再起動すると、kumoctlの表示は以下のようになります：

    [       ]$ kumoctl m1 status
    hash space timestamp:
      Wed Dec 03 22:15:45 +0900 2008 clock 58
    attached node:
      192.168.0.101:19800  (active)
      192.168.0.102:19800  (active)
      192.168.0.103:19800  (active)
      192.168.0.104:19800  (fault)
    not attached node:
      192.168.0.104:19800

**not attached node** のところに表示されているkumo-serverは、kumo-managerから認識されているが、まだ登録されていないkumo-serverの一覧です。

ここで **attach** サブコマンドを発行すると、復旧したkumo-serverが実際に登録され、データの複製が3つになるようにコピーされます：

    [       ]$ kumoctl m1 attach    # 復旧したkumo-serverを再登録
    [       ]$ kumoctl m1 status
    hash space timestamp:
      Wed Dec 03 22:15:55 +0900 2008 clock 62
    attached node:
      192.168.0.101:19800  (active)
      192.168.0.102:19800  (active)
      192.168.0.103:19800  (active)
      192.168.0.104:19800  (active)
    not attached node:

ここでkumo-serverを登録するときに、データをコピーするために比較的大規模なネットワークトラフィックが発生することに注意してください。

kumoctlコマンドで *attach* サブコマンドの代わりに **detach** サブコマンドを発行すると、fault状態のkumo-serverが完全に切り離されます。このときも複製の数が3つになるようにコピーが行われます。

    [       ]$ kumoctl m1 detach    # 落ちたkumo-serverを完全に切り離し
    [       ]$ kumoctl m1 status
    hash space timestamp:
      Wed Dec 03 22:15:55 +0900 2008 clock 62
    attached node:
      192.168.0.101:19800  (active)
      192.168.0.102:19800  (active)
      192.168.0.103:19800  (active)
    not attached node:


### kumo-serverが3台以上ダウンした

kumo-serverが３台以上ダウンすると、一部のデータにアクセスできなくなります。しかしデータベースファイルが残っていれば、データを復旧することができます。
復旧するには、データベースファイルを削除せずにkumo-serverを再起動します。その後 *kumoctl* コマンドの **attach** サブコマンドを使って、再起動したkumo-serverを登録します。その後さらに **full-replace** サブコマンドを発行してください。
このとき障害が発生している間に削除されたデータは復活してしまう可能性があることに注意してください。


### kumo-managerがダウンした

2台で冗長構成をとっているときに片方のkumo-managerがダウンしまった場合は、まずダウンしたkumo-managerを再起動してください。再起動するkumo-managerのIPアドレスは、障害発生前と同じにしておく必要があります。次に、残っていた方のkumo-managerに対して*kumoctl*コマンドを使って**replace**サブコマンドを発行してください。これで２台のkumo-managerの情報が同期されます。

kumo-managerがすべてダウンしてしまっても、システムを止めることなく復旧できます。この場合は、まずダウンしているkumo-serverがいるときは再起動してください。次に、kumo-managerをすべて再起動させてください。最後に*kumoctl*コマンドの**attach**サブコマンドを使ってkumo-serverを登録します。これで整合性のある状態に回復します。


### Setが必ず失敗してしまう

何かデータをSetしようとしたときに、何度やっても必ず失敗してしまう場合は、以下の点を調べてみてください：

**expireに0以外の値がセットされていないか** memcachedプロトコルを使ってkumofsにデータをsetするには、expireが0である必要があります。

**flagsに0以外の値がセットされていないか** memcachedプロトコルを使ってkumofsにデータをsetするには、kumo-gatewayの引数に **-F** オプションを付加する必要があります。memcachedのクライアントライブラリによっては、自動的にflagをセットしてしまうものがあります。kumo-gatewayの代わりに、本物のmemcachedに**-vv**オプションを付けて起動すると、memcachedに発行されたコマンドのログがmemcachedの標準出力に表示されるようになります。これを利用して、クライアントライブラリが自動的にexpireに0以外の値をセットしていないか確認してください。

**kumo-serverは登録されているか**  *kumoctl*コマンドの**status**サブコマンドを使ってkumo-managerの状態を調べ、kumo-serverが*attached node*として登録されており、*active)*と表示されていることを確認してください。

**サーバーによってルーティング表が食い違っていないか** kumo-serverは、それぞれノードの一覧表を保持しています。その内容が食い違っていると、Set操作が成功しません。*kumostat -m MANAGER whs*と実行して、すべてのkumo-serverのノード一覧表が同じ（hash space timestampの値が同じ）であるかどうか確認してください。食い違っていた場合は、*kumoctl*コマンドの**replace**コマンドを発行してください。また、サーバーのシステム時刻が同じになっていることを確認してください。時刻が食い違っていると正しく動作しません。


### Setがたまに失敗してしまう

memcachedのクライアントライブラリの実装によっては、デフォルトのタイムアウト時間が非常に短く設定されていることがあります。Set操作は比較的時間がかかるので、タイムアウトしてしまっている可能性があります。
memcachedのクライアントライブラリの設定で、タイムアウト時間を長くしてみてください。


### SetできるのにGetできない

kumofsは、Set/Delete操作用とGet操作用の２つのルーティング表を使っています。通常この２つは同じですが、サーバーの追加や復旧の処理中（kumoctlでreplaceサブコマンドを発行して、データのコピーを行っている最中）にエラーが発生すると、2つのルーティング表が食い違ってしまうことがあります。

*kumostat*コマンドの**hscheck**サブコマンドを使うと、それぞれのサーバーで２つのルーティング表が食い違っていないかを確認することができます。
もし食い違っていた場合は、*kumoctl*コマンドの**replace**サブコマンドを発行してください。


### 新しいデータをSetしても古い値がGetされてしまう

すべてのデータにはタイムスタンプが付けられています。タイムスタンプは、システム時刻（UNIX時刻）と「論理クロック（Lamport Clock」を組み合わせたもので、システム時刻のずれが少ない場合は論理クロックで差が吸収されますが、5秒以上ずれていると、正しく動作しません。

サーバーのシステム時刻は、できる限り同じになるようにしてください。NTPを使って合わせるのがベストです。


### データベースファイルには保存されているのにGetできないkeyがある

kumofsはデータを複数のkumo-serverに分散して保存しますが、保存するときに「所定」のkumo-serverに保存し、取得するときは「所定」のkumo-serverに保存されていることを前提とします。このため、何らかの理由でデータが「所定」のkumo-serverとは違うkumo-serverに保存されていると、そのデータがデータベースファイルには保存されていても取得できません。

この問題は、kumo-managerのノード一覧表が食い違っているときに発生することがあります。*kumoctl*コマンドの**full-replace**サブコマンドを使うと修復できます。ただし、full-replaceを使うと大きなネットワークトラフィックが発生するので、注意してください。


### 障害が発生していないのに障害と判定されてしまう

kumo-managerのタイムアウト時間が短すぎる可能性があります。kumo-managerの**-Ys**引数や**-Yn**引数の値を大きくすると、タイムアウト時間を引き延ばせます。


## 性能とチューニング

### 性能の見積もり

kumofsは AMD Athlon64 X2 5000+ を搭載したサーバーを１台使って、1秒間に約5万回のGet操作を処理できます。このスループットはkumo-serverの台数にほぼ比例して向上するので、5台のサーバーを使えば1秒間に約25万回のGet操作を処理できます。Set操作とDelete操作のスループットは、Get操作の約3分の1になります。

ただしこのスループットは、すべてのデータがキャッシュメモリに収まっている場合のスループットです。1つのデータは3つにレプリケーションされるので、合計1GBのデータを保存するには合計3GBのメモリを必要とします。また、1つのデータはアラインメントが取られて保存されるため、1つのデータのサイズは16バイトの倍数（デフォルト）に切り上げられます。ここから計算すると、例えば１つのデータのサイズが160バイトで、2GBのメモリを搭載したサーバーを5台用意すると、2GB * 5台 / 3レプリカ / 160バイト = 2236万件 までのデータがキャッシュメモリに収まります。


### データベースファイルのチューニング

Tokyo Cabinetのハッシュデータベースのチューニングすることで、性能が大きく変わります。最大の性能を得るには、kumo-serverを起動する前に必ず **tchmgr** コマンドを使ってデータベースファイルを作成するようにしてください。
最も重要なのは、バケット数のチューニングです。詳しくは [Tokyo Cabinet のドキュメント](http://1978th.net/tokyocabinet/spex-ja.html) を参照してください。

Tokyo Cabinetのパラメータのうち、拡張メモリマップのサイズ（xmsiz）とキャッシュ機構（rcnum）は、kumo-serverのコマンドライン引数で指定します。kumo-serverの **-s** オプションで、データベースファイル名の後ろに **#xmsiz=XXX** と指定すると、拡張メモリマップのサイズを指定できます。同様に **#rcnum=XXX** と指定すると、キャッシュ機構を有効化できます。

    [on svr1]$ tchmgr create database.tch 1000000
    [on svr1]$ kumo-server -v -m mgr -l svr1 -s "database.tch#xmsiz=600m#rcnum=4k"


### スレッド数のチューニング

CPUのコア数が多い場合は、kumo-serverやkumo-gatewayのワーカースレッドの数（-TR引数）を増やすと性能が向上します。CPUのスレッド数+2 くらいが目安です。デフォルトは8です。
１つのデータのサイズが大きい場合は、kumo-serverやkumo-gatewayの送信用スレッドの数（-TW引数）を増やすと性能が向上する可能性があります。デフォルトは2です。


## コマンドリファレンス

### configureフラグ

**--with-tokyocabinet=DIR** Tokyo Cabinetがインストールされているディレクトリを指定する

**--with-msgpack=DIR** MessagePackがインストールされているディレクトリを指定する

**--enable-trace** 画面を埋め尽くすほど冗長なデバッグ用のメッセージを出力するようにする


### 共通のコマンドライン引数
**-o &lt;path.log&gt;** ログを標準出力ではなく指定されたファイルに出力する。**-**を指定すると標準出力に出力する。省略するとログに色を付けて標準出力に出力する

**-v** WARNよりレベルの低いメッセージを出力する

**-g &lt;path.mpac&gt;** バイナリ形式のログを指定されたファイルに出力する

**-d &lt;path.pid&gt;** デーモンになる。指定されたファイルにpidを書き出す

**-Ci &lt;sec&gt;** タイマークロックの間隔を指定する。単位は秒で、整数か小数を定できる

**-Ys &lt;sec&gt;** connect(2)のタイムアウト時間を指定する。単位は秒で、整数か小数を指定できる

**-Yn &lt;num&gt;** connect(2)のリトライ回数を指定する

**-TR &lt;num&gt;** ワーカースレッドの数を指定する

**-TW &lt;num&gt;** 送信用スレッドの数を指定する


### kumo-manager
**-l &lt;address&gt;** 待ち受けるアドレス。**他のノードから見て**接続できるホスト名とポート番号を指定する

**-p &lt;address&gt;** もし存在するなら、もう一台のkumo-managerのホスト名とポート番号を指定する

**-c &lt;port&gt;** kumoctlからのコマンドを受け付けるポート番号を指定する

**-a** kumo-serverが追加・離脱されたときに、マニュアル操作を待たずにレプリケーションの再配置を自動的に行うようにする。実行中でもkumoctlコマンドを使って変更できる

**-Rs** 自動的な再配置が有効なときに、サーバーの追加・離脱を検出してからレプリケーションの再配置を開始するまでの待ち時間を指定する。単位は秒


### kumo-server
**-l &lt;address&gt;** 待ち受けるアドレス。**他のノードから見て**接続できるホスト名とポート番号を指定する

**-L &lt;port&gt;** kumo-serverが待ち受けるもう一つのポートのポート番号を指定する

**-m &lt;address&gt;** kumo-managerのホスト名とポート番号を指定する

**-p &lt;address&gt;** もし存在するなら、もう一台のkumo-managerのホスト名とポート番号を指定する

**-s &lt;path.tch[#xmsiz=SIZE][#rcnum=SIZE]&gt;** データを保存するデータベースファイルのパスを指定する

**-f &lt;dir&gt;** レプリケーションの再配置に使う一時ファイルを保存するディレクトリを指定する。データベースファイルのサイズに応じて十分な空き容量が必要

**-gS &lt;seconds&gt;** deleteしたエントリのクロックを保持しておくメモリ使用量の上限をKB単位で指定する

**-gN &lt;seconds&gt;** deleteしたエントリのクロックを保持しておく最小時間を指定する。メモリ使用量が上限に達していると、最大時間に満たなくても最小時間を過ぎていれば削除される。

**-gX &lt;seconds&gt;** deleteしたエントリのクロックを保持しておく最大時間を指定する


#### 削除済みフラグの回収
Delete操作は実際には削除済みフラグをSetする操作です。しかし削除済みフラグは時間が経過すると回収され、本当に削除されます。削除済みフラグが回収されると一貫性は保証されません。削除済みフラグは以下の条件で回収されます：

**Deleteしてから一定の時間が経過した** この時間はkumo-serverの**-gX**オプションで指定できます。

**Deleteフラグを記憶するメモリ使用量の上限に達し、かつDeleteしてから一定の時間が経過した** この時間はkumo-serverの**-gN**オプションで指定できます。メモリ使用量の上限は**-gS**オプションで指定できます。

削除済みフラグを記憶するメモリ使用量の上限に達したが、Deleteしてから一定の時間が経過していない場合は、削除済みフラグはデータベースファイルの中に放置されます。放置された削除済みフラグは、次に再配置操作が行われたときに回収されます。

### kumo-gateway
**-m &lt;address&gt;** kumo-managerのホスト名とポート番号を指定する

**-p &lt;address&gt;** もし存在するなら、もう一台のkumo-managerのホスト名とポート番号を指定する

**-t &lt;port&gt;** memcachedテキストプロトコルを待ち受けるポート番号を指定する

**-b &lt;port&gt;** memcachedバイナリプロトコルを待ち受けるポート番号を指定する（EXPERIMENTAL）

**-F** memcachedテキストプロトコルで、flagsを保存するようにする

**-G &lt;number&gt;** Get操作の最大リトライ回数を指定する

**-S &lt;number&gt;** Set操作の最大リトライ回数を指定する

**-D &lt;number&gt;** Delete操作の最大リトライ回数を指定する

**-As** Set操作でレプリケーションするとき、レプリケーション完了の応答を待たずに成功を返すようにする

**-Ad** Delete操作でレプリケーションするとき、レプリケーション完了の応答を待たずに成功を返すようにする


#### 非同期レプリケーション

kumofsではデータをsetしたりdeleteしたりするときレプリケーションを行いますが、デフォルトではレプリケーションが完了するまで待ってから（すべてのサーバーから応答が帰ってきてから）アプリケーションに応答が返されます。これを1台のkumo-serverに書き込みが完了した時点で応答を返すようにすると（非同期レプリケーション）、更新系の応答時間が大幅に短縮されます。

ただし非同期レプリケーションを有効にすると、成功応答が帰ってきたとしても、必ずしもレプリケーションが成功していることが保証されず、そのため複数のkumo-server間でデータの一貫性が保たれているとが保証されなくなります。

Set操作のレプリケーションを非同期にするには、kumo-gatewayのコマンドライン引数に**-As**を、Delete操作のレプリケーションを非同期にするには**-Ad**を追加してください。


### kumoctl
kumoctlコマンドはkumo-managerに様々なコマンドを発行するための管理コマンドです。
第一引数にkumo-managerのアドレスを指定し、第二引数にサブコマンドを指定します。

    Usage: kumoctl address[:port=19799] command [options]
    command:
       status                     get status
       attach                     attach all new servers and start replace
       attach-noreplace           attach all new servers
       detach                     detach all fault servers and start replace
       detach-noreplace           detach all fault servers
       replace                    start replace without attach/detach
       full-replace               start full-replace (repair consistency)
       backup  [suffix=????????]  create backup with specified suffix
       enable-auto-replace        enable auto replace
       disable-auto-replace       disable auto replace

**attach**サブコマンドは、認識しているが登録されていないkumo-serverを実際に登録します。**detach**サブコマンドは、fault状態のkumo-serverを切り離します。

**attach-noreplace**サブコマンドは、**attach**とほぼ同じですが、kumo-serverを登録した後にkey-valueペアの複製の再配置を行いません。**detach-noreplace**サブコマンドは、**detach**と同じですが、kumo-serverを切り離した後に複製の再配置を行いません。

**replace**サブコマンドは複製の再配置だけを行います。attach-noreplaceサブコマンドとdetach-noreplaceサブコマンドは、attachとdetachを同時に行いたいときのみ使用し、すぐにreplaceサブコマンドを使って再配置を行ってください。再配置を行わないまま長い間放置してはいけません。

**backup**サブコマンドは、データベースファイルのバックアップを作成します。バックアップは認識しているすべてのkumo-server上で作成されます。

バックアップファイルのファイル名は、元のデータベースファイル名に第三引数で指定したsuffixを付けたファイル名になります。suffixを省略するとその日の日付(YYMMDD)が使われます。
作成されたバックアップファイルは、**kumomergedb**コマンドを使って1つのファイルにまとめることができます。


### kumostat

kumostatコマンドを使うとkumo-serverの状態を取得することができます。
第一引数にkumo-serverのホスト名とポート番号を指定し、第二引数にコマンドを指定します：

    Usage: kumostat server-address[:port=19800] command
           kumostat -m manager-address[:port=19700] command
    command:
       pid                        get pid of server process
       uptime                     get uptime
       time                       get UNIX time
       version                    get version
       cmd_get                    get number of get requests
       cmd_set                    get number of set requests
       cmd_delete                 get number of delete requests
       items                      get number of stored items
       rhs                        get rhs (routing table for Get)
       whs                        get whs (routing table for Set/Delete)
       hscheck                    check if rhs == whs
       set_delay                  maximize throughput at the expense of latency
       unset_delay                minimize latency at the expense of throughput

**-m**に続いてkumo-managerのアドレスを指定すると、attachされていてactiveなすべてのkumo-serverの状態を表示します。


**pid** kumo-serverプロセスのpidを表示

**uptime** kumo-serverプロセスの起動時間（単位は秒）を表示

**time** kumo-serverプロセスが走っているホストのシステム時刻を表示

**version** kumo-serverのバージョンを表示

**cmd_get** Get操作を処理した回数を表示

**cmd_set** Set操作を処理した回数を表示

**cmd_delete** Delete操作を処理した回数を表示

**items** データベースに保存されているエントリ数を表示

**rhs** Get操作に使われるルーティング表を表示

**whs** Set操作とDelete操作に使われるルーティング表を表示

**hscheck** rhsとwhsが同じかどうかチェックします

**set_delay** 遅延を犠牲にして最大スループットを最大化する

**unset_delay** 最大スループットを犠牲にして遅延を最小化する


rhsとwhsが食い違っている場合は、再配置を実行中か、前回の再配置が失敗している可能性があります。


### kumotop

kumotopコマンドを使うとkumo-serverの状態を定期的に更新しながら表示することができます。
引数に監視したいkumo-serverのアドレスを指定します。kumo-serverのアドレスは複数指定できます：

    Usage: kumotop server-address[:port=19800] ...
           kumotop -m manager-address[:port=19700]

**-m** に続いてkumo-managerのアドレスを指定すると、attachされているすべてのkumo-serverの状態を表示します。


### kumomergedb

kumomergedbコマンドは複数のデータベースファイルを1つにまとめます。主にバックアップしたデータベースファイルを１つにまとめるために使います。
第一引数に出力先のファイル名を指定し、第二引数以降にまとめたいデータベースファイルを指定します。

    $ kumomergedb backup.tch-20090101 \
                  server1.tch-20090101 server2.tch-20090101 server3.tch-20090101


### kumohash

**kumohash** コマンドを使うと、あるkeyがどのkumo-serverに保存されるかを計算することができます。

    Usage: kumohash server-address[:port=19800] ... -- command [options]
           kumohash -m manager-address[:port=19700] command [options]
    command:
       hash  keys...              calculate hash of keys
       assign  keys...            calculate assign node
       dump                       dump hash space

**hash** keyのハッシュ値を計算する

**assign** keyがどのkumo-serverに保存されるかを計算する

**dump** Consistent Hashingのルーティング表を表示する


### kumolog

**kumolog** コマンドはバイナリ形式のログを人間にとって読みやすいテキストに変換して表示します。

    kumolog [options] &lt;logfile.mpac&gt;

**-f, --follow     **  *tail -f*と同じ効果

**-t, --tail       **  最後のN個のログだけ表示する（デフォルト: N=10）

**-h, --head       **  最初のN個のログだけ表示する（デフォルト: N=10）

**-n, --lines=[-]N **  Nを指定する


#### ログ

kumo-manager, kumo-server, kumo-gateway は、それぞれ2種類のログを出力します：

**テキスト形式** 行区切りのテキストフォーマットのログ。標準出力に出力される

**バイナリ形式** MessagePackでシリアライズされたバイナリ形式のログ

テキスト形式のログは、常に出力されます。**-v**オプションを付けると冗長なログも出力されるようになります。テキストログはファイルに書き出すこともできますが、ログローテーションはサポートしていません。デフォルトでは優先度によってログに色が付きますが、**-d &lt;path.pid&gt;**オプションを指定してデーモンとして起動するか、**-o "-"**オプションを指定すると色が付かなくなります。daemon tools の multilog を使ってログを取るときは、**-o "-"** オプションを付けてください。

バイナリ形式のログは、 **-g &lt;path.mpac&gt;** オプションを付けたときだけ出力されます。バイナリログはSIGHUPシグナルを受け取るとログファイルを開き直すため、logrotateなどを使ってログローテーションができます。

テキスト形式のログとバイナリ形式のログは、異なる内容が出力されます。


## FAQ

### kumofsの名前の由来は？

*kumo*は、空に浮かぶ**雲**を意味しています。雲は落ちないです。*fs*は、予想に反してfast storageの略です。


### サーバーに障害が発生したと判断される基準

メッセージを送ろうとしたところ、接続済みのすべてのTCPコネクションでエラーが発生し、再接続を試みても失敗して再接続のリトライ回数が上限に達したら、そのノードはダウンしたと判断します。

TCPコネクションが切断されただけではダウンしたとは判断せず、メッセージの送信に失敗しても制限回数以内に再接続することができたら、メッセージは再送されます。

kumo-serverとkumo-managerは常にkeepaliveメッセージをやりとりしており、いつもメッセージを送ろうとしている状態になっています。kumo-managerはkumo-serverがダウンしたらできるだけ早く検出してfaultフラグをセットし、正常なアクセスを継続させます。


### どのkumo-serverにデータを保存するかを決めるアルゴリズム

kumofsはConsistent Hashingと呼ばれるアルゴリズムを利用しています。ハッシュ関数はSHA-1で、下位の64ビットのみを使います。1台の物理ノードは128台の仮想ノードを持ちます。

データを取得するときは、kumo-gatewayがkeyにハッシュ関数を掛けてハッシュ表から担当ノードを計算し、担当ノードからデータを取得します。取得に失敗したときは、ハッシュ表上でその次に当たるノードから取得します。それでも失敗したらその次の次のノードから取得します。それでも失敗したら最初の担当ノードに戻ってリトライします。

データを変更するときは、kumo-gatewayがkeyにハッシュ関数を掛けてハッシュ表から担当ノードを計算し、担当ノードにデータを送信します。取得する場合とは異なり、次のノードにフォールバックすることはありません。

担当ノードはSetやDelete操作を受け取ると、データをハッシュ表上で次のノードと、次の次のノードにレプリケーションします。

担当ノードはkumo-gatewayからリクエストを受け取ったとき、本当に自分が担当ノードであるかどうかを自分が持っているハッシュ表を使って確認します。間違っていた場合はリクエストを拒否します。このように必ず特定の担当ノードだけがデータを変更でき、他のノードが同じタイミングで同じkey-valueを変更することがないようになっています。

担当ノードを選ぶときfaultフラグがセットされているノードはスキップします。このため一部の担当ノードがダウンしている状態でも正常なアクセスを続けられます。


### データベースファイルのフォーマット

以下のような構造で保存されています。箱の中の数字はビット数で、エンディアンはビッグエンディアンです。

    key:
    +---------+-----------------+
    |   64    |       ...       |
    +---------+-----------------+
    hash
              key
    
    active value:
    +---------+--+-----------------+
    |   64    |16|       ...       |
    +---------+--+-----------------+
    clocktime
              metadata
                 data
    
    deleted value:
    +---------+
    |   64    |
    +---------+
    clocktime
    
    clocktime:
    +----+----+
    | 32 | 32 |
    +----+----+
    UNIX time
         logical clock

