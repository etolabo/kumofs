package t::ManyData;

use strict;
use warnings;
use utf8;
use Exporter  qw(import);
our @EXPORT = qw(set_many_data get_many_data wait_user_operation);
use Carp;
use t::TestUtil;

# set_many_data(number_of_entries_to_set)
sub set_many_data($) {
    my $mc = create_memcache_client();
    for(my $i=0; $i < $_[0]; ++$i) {
        $mc->set($i, $i);
    }
    undef $mc;
}

# get_many_data(number_of_entries_to_get)
sub get_many_data($) {
    my $mc = create_memcache_client();
    for(my $i=0; $i < $_[0]; ++$i) {
        if($mc->get($i) != $i) {
            return -1;
        }
    }
    undef $mc;
    return 0;
}

# wait_user_operation("message")
sub wait_user_operation($) {
    print STDERR $_[0], " and press Enter key: ";
    my $str = <STDIN>;
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
