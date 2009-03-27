#!/usr/bin/env ruby

require 'common'
include Chukan::Test

LOOP_RESTART = (ARGV[0] ||  10).to_i
SLEEP        = (ARGV[1] ||   1).to_i
NUM_STORE    = (ARGV[2] || 100).to_i
NUM_THREAD   = (ARGV[3] ||   4).to_i

mgr = Manager.new

srv1 = Server.new(1, mgr)
srv2 = Server.new(2, mgr)
srv3 = Server.new(3, mgr)
mgr.stdout_join("new node")
mgr.stdout_join("new node")
mgr.stdout_join("new node")

mgr.attach.join
mgr.stdout_join("replace finished")

gw  = Gateway.new(0, mgr)
gw.stdout_join("connect success")

end_flag = false

threads = (1..NUM_THREAD).to_a.map do
	Thread.start(gw.client) {|client|
		until end_flag

			source = (1..NUM_STORE).to_a.shuffle.map {|x| ["key#{x}", "val#{x}"] }

			test "set value" do
				source.each {|k, v|
					begin
						client.set(k, v)
					rescue
						raise "set failed '#{k}' => '#{v}': #{$!.inspect}"
					end
				}
				true
			end

			test "get value" do
				source.shuffle.each {|k, v|
					begin
						unless client.get(k) == v
							raise "get failed '#{k}' != '#{v}': #{$!.inspect}"
						end
					rescue
						raise "get failed '#{k}': #{$!.inspect}"
					end
				}
				true
			end

		end
	}
end

def restart_srv(mgr, srv)
	srv.kill
	srv.join
	srv = Server.new(srv.index, mgr)
	mgr.stdout_join("new node")
	mgr.attach.join
	mgr.stdout_join("replace finished")
	srv
end

LOOP_RESTART.times {
	sleep SLEEP
	case rand(3)
	when 0:
		srv1 = restart_srv(mgr, srv1)
	when 1:
		srv2 = restart_srv(mgr, srv2)
	when 2:
		srv3 = restart_srv(mgr, srv3)
	end
}

end_flag = true
threads.each {|th| th.join }

srv1.term
srv2.term
srv3.term
gw.term
mgr.term

srv1.join
srv2.join
srv3.join
gw.join
mgr.join

