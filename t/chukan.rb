#
# Chukan  automation library for distributed systems
#
# Copyright (C) 2009 FURUHASHI Sadayuki
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#


=begin

require 'chukan'
include Chukan

srv = spawn("server -arg1 -arg2")  # run 'server' command
                                   # with '-arg1 -arg2' arguments
srv.stdout_join("started")         # wait until the server puts "started"

cli = spawn("client -arg1 -arg2")  # run "client" command with some arguments
src.stdout_join("connected")       # wait until the server outputs "connected"

cli.kill                           # send SIGKILL signal to the client
srv.stderr_join(/disconnected/)    # stderr and regexp are also usable

srv.stdin.write "status\n"         # input "status\n" to the server
srv.stdout_join("done")            # wait until the server outputs "done"

if srv.stdout.read =~ /^client:/   # read output of the server
	puts "** TEST FAILED **"         # this library is usable for tests
end

=end


require 'stringio'
require 'strscan'
require 'monitor'


module Chukan

	class LocalProcess
		def initialize(*cmdline)
			@cmdline = cmdline
			@status = nil
			@shortname = File.basename(cmdline.first.split(/\s/,2).first)[0, 12]
			start
		end

		attr_reader :cmdline
		attr_reader :stdin, :stdout, :stderr
		attr_reader :pid
		attr_reader :status

		def join
			@status = Process.waitpid2(@pid)[1]
			@stdout_reader.join
			@stderr_reader.join
			@killer.killed
			@status
		end

		def stdout_join(pattern)
			io_join(@stdout, @stdout_cond, @stdout_scan, pattern)
		end

		def stderr_join(pattern)
			io_join(@stderr, @stderr_cond, @stderr_scan, pattern)
		end

		def signal(sig)
			Process.kill(sig, @pid)
			self
		end

		def kill
			signal(:SIGKILL)
		end

		def term
			signal(:SIGTERM)
		end

		def hup
			signal(:SIGHUP)
		end

		private
		def io_join(io, cond, scan, pattern)
			if pattern.is_a?(String)
				pattern = Regexp.new(Regexp.escape(pattern))
			end
			match = nil
			io.synchronize {
				until match = scan.scan_until(pattern)
					break if io.closed_write?
					cond.wait
				end
			}
			match
		end

		private
		def start
			stdin, @stdin = IO.pipe
			@pout, pout = IO.pipe
			@perr, perr = IO.pipe
			@pid = fork
			unless @pid
				@stdin.close
				@pout.close
				@perr.close
				$stdin.reopen(stdin)
				$stdout.reopen(pout)
				$stderr.reopen(perr)
				exec *cmdline
				exit 127
			end
			stdin.close
			pout.close
			perr.close
			@stdout = StringIO.new.extend(MonitorMixin)
			@stderr = StringIO.new.extend(MonitorMixin)
			@stdout_cond = @stdout.new_cond
			@stderr_cond = @stderr.new_cond
			@stdout_scan = StringScanner.new(@stdout.string)
			@stderr_scan = StringScanner.new(@stderr.string)
			@stdout_reader = Thread.start(@pout, @stdout, @stdout_cond,
																		$stdout, &method(:reader_thread))
			@stderr_reader = Thread.start(@perr, @stderr, @stderr_cond,
																		$stderr, &method(:reader_thread))
			@killer = ZombieKiller.define_finalizer(self, @pid)
		end

		def reader_thread(src, dst, cond, msgout)
			buf = ""
			begin
				while true
					src.sysread(1024, buf)
					dst.synchronize {
						dst.string << buf
						cond.signal
					}
					buf.split(/\n/).each {|line|
						msgout.puts "[%-12s %6d] #{line}" % [@shortname, @pid]
					}
				end
			rescue
				nil
			ensure
				src.close
				dst.synchronize {
					dst.close_write
					cond.signal
				}
			end
		end
	end


	class ZombieKiller
		def initialize(pid)
			@pid = pid
		end
		def killed
			@pid = nil
		end
		attr_reader :pid

		def self.define_finalizer(obj, pid)
			killer = self.new(pid)
			ObjectSpace.define_finalizer(obj, self.finalizer(killer))
			killer
		end

		def self.finalizer(killer)
			proc {
				return unless pid = killer.pid
				[:SIGTERM, :SIGKILL].each {|sig|
					Process.kill(sig, pid)
					break if 10.times {
						begin
							if Process.waitpid(pid, Process::WNOHANG)
								break true
							end
							sleep 0.1
						rescue
							break true
						end
						nil
					}
				}
			}
		end
	end


	def spawn(*cmdline, &block)
		LocalProcess.new(*cmdline, &block)
	end
end

