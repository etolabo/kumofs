# -*- coding: utf-8; mode: cperl -*-
use Test::Base;
use t::TestUtil;
use t::ManyData;

=pod
== サーバー復旧時の再配置 1 ==
  1. Managerを起動する
  2. Serverを起動する
  3. kumoctl manager attachを実行する
  4. Gatewayを起動する
  5. このテストを実行する
  6.1. Serverを1台落とす
  6.3. 落としたServerを再起動する
  6.4. kumoctl localhost attachを実行

== サーバー復旧時の再配置 2 ==
  ...
  6.1. Serverを2台落とす
  ...
=cut

plan tests => 1 * blocks;
filters { num => 'eval' };

foreach my $block (blocks()) {
    set_many_data($block->num);
}

wait_user_operation("Serverを落とす");
wait_user_operation("Serverを復旧させてkumoctl localhost attachを実行");

run {
    my $block = shift;
    is get_many_data($block->num), 0, $block->name;
}

__END__
=== 1000entries
--- num: 1000

