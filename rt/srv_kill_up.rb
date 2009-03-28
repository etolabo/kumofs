#!/usr/bin/env ruby
require 'common'
include Chukan::Test

LOOP_RESTART = (ARGV[0] ||  10).to_i
SLEEP        = (ARGV[1] ||   1).to_i
NUM_STORE    = (ARGV[2] || 100).to_i
NUM_THREAD   = (ARGV[3] ||   4).to_i

mgr, gw, srv1, srv2, srv3 = init_cluster(false, 3)
begin

	tester = RandomTester.start_threads(gw, NUM_THREAD, NUM_STORE)

	LOOP_RESTART.times {
		sleep SLEEP
		case rand(3)
		when 0
			srv1 = restart_srv(srv1, mgr)
		when 1
			srv2 = restart_srv(srv2, mgr)
		when 2
			srv3 = restart_srv(srv3, mgr)
		end
	}

	tester.each {|ra| ra.stop }
	tester.each {|ra| ra.join }

ensure
	term_daemons(srv1, srv2, srv3, gw, mgr)
end

