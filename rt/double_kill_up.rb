#!/usr/bin/env ruby
require 'common'
include Chukan::Test

LOOP_RESTART = (ARGV[0] ||  30).to_i
SLEEP        = (ARGV[1] ||   2).to_i
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
		k1 = (mgrs + srvs).choice
		if k1.get.is_a?(Manager)
			k2 = srvs.choice
		else
			k2 = (mgrs + srvs - [k1]).choice
		end

		k1.get.kill
		k2.get.kill
		k1.get.join
		k2.get.join

		[k1, k2].each {|k|
			if k.get.is_a?(Manager)
				k.set start_mgr(k.get, (mgrs-[k]).first.get)
			else
				k.set start_srv(k.get, *(mgrs-[k1,k2]).map{|m|m.get})
			end
		}
	}

	true
end
term_daemons *(mgrs + srvs).map {|r| r.get }

