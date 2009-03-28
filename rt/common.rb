
COMMAND_BASE = ENV["COMMAND_BASE"] || File.dirname(__FILE__) + "/../src/command"
KUMOSTAT = COMMAND_BASE + "/kumostat"
KUMOCTL  = COMMAND_BASE + "/kumoctl"

LOGIC_BASE = ENV["LOGIC_BASE"] || File.dirname(__FILE__) + "/../src/logic"
KUMO_SERVER  = LOGIC_BASE + "/kumo-server"
KUMO_MANAGER = LOGIC_BASE + "/kumo-manager"
KUMO_GATEWAY = LOGIC_BASE + "/kumo-gateway"

MANAGER_PORT   = (ENV["MANAGER_PORT"]  || 49700).to_i
SERVER_PORT    = (ENV["SERVER_PORT"]   || 49800).to_i
MEMCACHE_PORT  = (ENV["MEMCACHE_PORT"] || 49900).to_i
STORAGE_FORMAT = (ENV["STORAGE_DIR"]   || Dir.pwd) + "/test-%s.tch"

begin
	require 'rubygems'
rescue LoadError
end
require 'chukan'

include Chukan

class Manager < Chukan::LocalProcess
	def initialize(index = 0, partner = nil)
		@index = index
		@host = "127.0.0.1"
		@port = MANAGER_PORT + index

		cmd = "#{KUMO_MANAGER} -v -l #{@host}:#{@port}"
		if partner.is_a?(Manager)
			cmd += " -p #{partner.host}:#{partner.port}"
		elsif partner
			cmd += " -p #{partner}"
		end

		super(cmd)
	end
	attr_reader :host, :port, :index

	def self.redundantly
		mgr2 = self.new(1, "127.0.0.1:#{MANAGER_PORT}")
		mgr1 = self.new(0, mgr2)
		[mgr1, mgr2]
	end

	def attach
		spawn("#{KUMOCTL} #{@host}:#{@port} attach")
	end

	def detach
		spawn("#{KUMOCTL} #{@host}:#{@port} detach")
	end

	def stat
		spawn("#{KUMOCTL} #{@host}:#{@port} stat")
	end
end


class Server < Chukan::LocalProcess
	def initialize(index, mgr1, mgr2 = nil)
		@index = index
		@host = "127.0.0.1"
		@port = SERVER_PORT + index*2
		@stream_port = @port + 1

		@storage = STORAGE_FORMAT % index
		File.rename(@storage, @storage+".bak") rescue nil

		cmd = "#{KUMO_SERVER} -v -l #{@host}:#{@port} -L #{@stream_port}" +
					" -s #{@storage} -m #{mgr1.host}:#{mgr1.port}"
		cmd += " -p #{mgr2.host}:#{mgr2.port}" if mgr2

		super(cmd)
	end
	attr_reader :index
end


class Gateway < Chukan::LocalProcess
	def initialize(index, mgr1, mgr2 = nil)
		@index = index
		@port = MEMCACHE_PORT + index
		cmd = "#{KUMO_GATEWAY} -v -m #{mgr1.host}:#{mgr1.port} -t #{@port}"
		cmd += " -p #{mgr2.host}:#{mgr2.port}" if mgr2

		super(cmd)
	end
	attr_reader :index

	def client
		require 'memcache'  # gem install Ruby-MemCache
		MemCache.new("127.0.0.1:#{@port}", {:urlencode => false, :compression => false})
	end
end


def init_cluster(manager2, num_srv)
	if manager2
		mgrs = Manager.redundantly
		mgrs.each {|mgr|
			mgr.stdout_join("partner connected")
		}

		srvs = (1..num_srv).map {|i|
			Server.new(i, *mgrs)
		}
		gw = Gateway.new(0, *mgrs)

		num_srv.times {
			mgrs.each {|mgr|
				mgr.stdout_join("new node")
			}
		}

		mgrs[0].attach.join
		mgrs[0].stdout_join("replace finished")

		gw.stdout_join("connect success")

		mgrs + [gw] + srvs

	else
		mgr = Manager.new

		srvs = (1..num_srv).map {|i|
			Server.new(i, mgr)
		}
		gw = Gateway.new(0, mgr)

		num_srv.times {
			mgr.stdout_join("new node")
		}

		mgr.attach.join
		mgr.stdout_join("replace finished")

		gw.stdout_join("connect success")

		[mgr] + [gw] + srvs
	end
end


def restart_mgr(mgr, partner)
	mgr.kill.join
	mgr = Manager.new(mgr.index, partner)
	mgr.stdout_join("partner connected")
	partner.stdout_join("partner connected")
	mgr
end


def restart_srv(srv, *mgrs)
	srv.kill.join
	srv = Server.new(srv.index, *mgrs)
	mgrs[0].stdout_join("new node")
	mgrs[0].attach.join
	mgrs[0].stdout_join("replace finished")
	srv
end


def term_daemons(*ds)
	ds.each {|d| d.term rescue p($!) }
	ds.each {|d| d.join rescue p($!) }
end


class RandomTester
	def initialize(gw, num_store)
		@client = gw.client
		@end_flag = false
		@thread = Thread.start(num_store, &method(:run))
	end

	def self.start_threads(gw, num_thread, num_store)
		(1..num_thread).map {
			self.new(gw, num_store)
		}
	end

	def stop
		@end_flag = true
	end

	def join
		@thread.join
	end

	private
	def run(num_store)
		until @end_flag
			source = (1..num_store).to_a.shuffle.map {|x| ["key#{x}", "val#{x}"] }

			test "set value" do
				source.each {|k, v|
					begin
						@client.set(k, v)
					rescue
						raise "set failed '#{k}' => '#{v}': #{$!.inspect}"
					end
				}
				true
			end

			test "get value" do
				source.shuffle.each {|k, v|
					begin
						r = @client.get(k)
					rescue
						raise "get failed '#{k}': #{$!.inspect}"
					end
					r = r[0] if r.is_a?(Array)  # Ruby 1.9
					unless r == v
						raise "get '#{k}' expects '#{v}' but '#{r}'"
					end
				}
				true
			end
		end
	end
end

