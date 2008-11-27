require 'rubygems'
require 'msgpack'
require 'socket'

sock = TCPSocket.new('127.0.0.1', 6001)

th = Thread.new {
	pk = MessagePack::Unpacker.new
	buffer = ''
	nread = 0
	while true
		buffer << sock.sysread(1024)
		while true
			nread = pk.execute(buffer, nread)
			if pk.finished?
				msg = pk.data
				pk.reset
				buffer.slice!(0, nread)
				nread = 0
				p msg
				next unless buffer.empty?
			end
			break
		end
	end
}


100.times {|i|

	# Set
	sock.write [true, 0, 97, ["key#{i}", "\0"*16 + "val#{i}"]].to_msgpack
	
	# Get
	sock.write [true, 0, 96, ["key#{i}"]].to_msgpack
	
	# Delete
	sock.write [true, 0, 98, ["key#{i}"]].to_msgpack
	
	# Get
	sock.write [true, 0, 96, ["key#{i}"]].to_msgpack
}

th.join

