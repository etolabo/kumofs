#!/usr/bin/env ruby
require 'common'
include Chukan::Test

LOOP_RESTART = (ARGV[0] ||  30).to_i
SLEEP        = (ARGV[1] ||   2).to_i
NUM_STORE    = (ARGV[2] || 100).to_i
NUM_THREAD   = (ARGV[3] ||   1).to_i

mgr1, mgr2, gw, srv1, srv2, srv3 = init_cluster(true, 3)
test "run normally" do

	tester = RandomTester.start_threads(gw, NUM_THREAD, NUM_STORE)

	LOOP_RESTART.times {
		sleep SLEEP
		case rand(3)
		when 0
			mgr1 = restart_mgr(mgr1, mgr2)
		when 1
			mgr2 = restart_mgr(mgr2, mgr1)
		when 2
			case rand(3)
			when 0
				srv1 = restart_srv(srv1, mgr1, mgr2)
			when 1
				srv2 = restart_srv(srv2, mgr1, mgr2)
			when 2
				srv3 = restart_srv(srv3, mgr1, mgr2)
			end
		end
	}

	tester.each {|ra| ra.stop }
	tester.each {|ra| ra.join }

	true
end
term_daemons(srv1, srv2, srv3, gw, mgr1, mgr2)

