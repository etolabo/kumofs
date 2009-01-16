# -*- coding: utf-8; mode: cperl -*-
use Test::Base;
use t::TestUtil;

plan tests => 1 * blocks;
#filters { kv => 'eval' };

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

