#!/usr/bin/env ruby
require 'common'
include Chukan::Test

LOOP_RESTART = (ARGV[0] ||  30).to_i
SLEEP        = (ARGV[1] ||   5).to_i
NUM_STORE    = (ARGV[2] || 100).to_i
NUM_THREAD   = (ARGV[3] ||   1).to_i

class Ref
	def initialize(obj = nil)
		@obj = obj
	end
	def get
		@obj
	end
	def set(obj)
		@obj = obj
	end
end

def ref(obj = nil)
	Ref.new(obj)
end

mgr1, mgr2, gw, srv1, srv2, srv3 = init_cluster(true, 3)

mgrs = [ref(mgr1), ref(mgr2)]
srvs = [ref(srv1), ref(srv2), ref(srv3)]

test "run normally" do

	tester = RandomTester.start_threads(gw, NUM_THREAD, NUM_STORE)

	LOOP_RESTART.times {
		sleep SLEEP

		k1, k2 = srvs.shuffle[0, 2]
		mgr = mgrs.choice.get

		mgr.stdout_join("lost node") do
			k1.get.kill.join
		end
		mgr.stdout_join("new node") do
			k1.set Server.new(k1.get.index, mgr1, mgr2)
		end

		ctl = nil
		mgr.stdout_join("start replace copy") do
			ctl = mgr.attach
		end

		k2.get.kill.join
		mgr.stdout_join("new node") do
			k2.set Server.new(k2.get.index, mgr1, mgr2)
		end

		ctl.join
		mgr.stdout_join("replace finished") do
			mgr.attach.join
		end
	}

	true
end
term_daemons *(mgrs + srvs).map {|r| r.get }

