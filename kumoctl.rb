require 'rubygems'
require 'msgpack'
require 'socket'


class KumoManager
	def initialize(host, port)
		@sock = TCPSocket.open(host, port)
		@pk = MessagePack::Unpacker.new
		@buffer = ''
		@nread = 0
		@seqid = rand(1<<32)
		@callback = {}
	end


	private
	def send_request(seq, cmd, param)
		@sock.write [true, seq, cmd, param].to_msgpack
		@sock.flush
	rescue
		@sock.close
		throw
	end

	def receive_message
		while true
			if @buffer.length > @nread
				@nread = @pk.execute(@buffer, @nread)
				if @pk.finished?
					msg = @pk.data
					@pk.reset
					@buffer.slice!(0, @nread)
					@nread = 0
					if msg[0]
						process_request(msg[1], msg[2], msg[3])
					else
						process_response(msg[1], msg[3], msg[2])
					end
					return msg[1]
				end
			end
			@buffer << @sock.sysread(1024)
		end
	end

	def process_request(seqid, cmd, param)
		throw "request received, excpect response"
	end

	def process_response(seqid, res, err)
		if cb = @callback[seqid]
			cb.call(res, err)
		end
	end

	def synchronize_response(seqid)
		while receive_message != seqid; end
	end

	def send_request_async(cmd, param, &callback)
		seqid = @seqid
		@seqid += 1; if @seqid >= 1<<32 then @seqid = 0 end
		@callback[seqid] = callback if callback
		send_request(seqid, cmd, param)
		seqid
	end

	def send_request_sync(cmd, param)
		res = nil
		err = nil
		seqid = send_request_async(cmd, param) {|rres, rerr|
			res = rres
			err = rerr
		}
		synchronize_response(seqid)
		return [res, err]
	end

	def send_request_sync_ex(cmd, param)
		res, err = send_request_sync(cmd, param)
		throw "error #{err}" if err
		res
	end


	public
	def GetStatus
		send_request_sync_ex(84, [])
	end

	def StartReplace
		send_request_sync_ex(85, [])
	end

	def CreateBackup
		res = send_request_sync(86, [])
	end

end

mgr = KumoManager.new('127.0.0.1', 19799)
p mgr.GetStatus
p mgr.StartReplace


