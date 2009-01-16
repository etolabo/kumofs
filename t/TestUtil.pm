package t::TestUtil;

use strict;
use warnings;
use utf8;
use Exporter  qw(import);
our @EXPORT = qw(y d create_memcache_client);
use Carp;
use Data::Dumper;
$Data::Dumper::Indent   = 1;
$Data::Dumper::Deepcopy = 1;
$Data::Dumper::Sortkeys = 1;
use YAML::Syck;
$YAML::Syck::ImplicitTyping = 1;
$YAML::Syck::SingleQuote    = 1;

use Cache::Memcached::Fast;

sub y(@) {
    print YAML::Syck::Dump(\@_);
}
sub d(@) {
    my $d = Dumper(\@_);
    $d =~ s/\\x{([0-9a-z]+)}/chr(hex($1))/ge;
    print $d;
}

# create_memcache_client({ servers => [ '127.0.0.1:11211' ] })
sub create_memcache_client {
    my $opt = shift;
    $opt->{servers} = [ '127.0.0.1:11211' ] unless exists $opt->{servers};
    return Cache::Memcached::Fast->new($opt);
}


__END__

# for Emacsen
# Local Variables:
# mode: cperl
# cperl-indent-level: 4
# indent-tabs-mode: nil
# coding: utf-8
# End:

# vi: set ts=4 sw=4 sts=0 :
