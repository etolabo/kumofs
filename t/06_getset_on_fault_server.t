# -*- coding: utf-8; mode: cperl -*-
use Test::Base;
use t::TestUtil;
use t::ManyData;

=pod
== サーバー障害時のgetとset 1 ==
  1. Managerを起動する
  2. Serverを起動する
  3. kumoctl manager attachを実行する
  4. Gatewayを起動する
  5. このテストを実行する
  6.1. Serverを1台落とす

== サーバー障害時のgetとset 2 ==
  ...
  6.1. Serverを2台落とす
  ...
=cut

plan tests => 1 * blocks;
#filters { kv => 'eval' };

wait_user_operation("Serverを落とす");

run {
    my $block = shift;

    my $mc0 = create_memcache_client();
    $mc0->set($block->key, $block->val);
    undef $mc0;

    my $mc = create_memcache_client();
    is $mc->get($block->key), $block->val, $block->name;
}

__END__
=== alpha
--- key: curry
--- val: daisuki

=== alnum
--- key: curry8
--- val: 1ban

=== num
--- key: 1
--- val: 2

=== zero
--- key: 0
--- val: 0

=== ja
--- key: キー
--- val: 表

