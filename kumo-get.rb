require 'rubygems'
require 'msgpack'
require 'socket'

sock = TCPSocket.new('127.0.0.1', 6001)

# Get
sock.write [true, 0, 96, ["kye0"]].to_msgpack

# Set
sock.write [true, 0, 97, ["key0", "\0"*16 + "val0"]].to_msgpack

pk = MessagePack::Unpacker.new
buffer = ''
nread = 0
while true
	if buffer.length > nread
		nread = pk.execute(buffer, nread)
		if pk.finished?
			msg = pk.data
			pk.reset
			buffer.slice!(0, nread)
			nread = 0
			p msg
		end
	end
	buffer << sock.sysread(1024)
end

