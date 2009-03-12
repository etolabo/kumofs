# -*- coding: utf-8; mode: cperl -*-
use Test::Base;
use t::TestUtil;
use t::ManyData;
use Data::Dumper;

=pod
== Serverの追加 1 ==
  1. Managerを起動する
  2. Serverを起動する
  3. kumoctl manager attachを実行する
  4. Gatewayを起動する
  5. このテストを実行する
  6.1. Serverを1台起動する
  6.2. kumoctl manager attachを実行する

== Serverの追加 2 ==
  ...
  6.1. Serverを2台起動する
  ...

== Serverの追加 N ==
  ...
  6.1. ServerをN台起動する
  ...
=cut

plan tests => 1 * blocks;
filters { num => 'eval' };

foreach my $block (blocks()) {
    set_many_data($block->num);
}

wait_user_operation("Serverを追加してkumoctl localhost attachを実行");

run {
    my $block = shift;
    is get_many_data($block->num), 0, $block->name;
}

__END__
=== 1000entries
--- num: 1000

