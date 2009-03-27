
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
		cmd += " -p #{partner}" if partner

		super(cmd)
	end
	attr_reader :host, :port, :index

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

