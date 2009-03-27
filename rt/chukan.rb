#
# Chukan  automation library for distributed systems
#
# Copyright (c) 2009 FURUHASHI Sadayuki
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#


####
## Basic usage
##
=begin
#!/usr/bin/env ruby
require 'chukan'
include Chukan                     # include Chukan

srv = spawn("server -arg1 -arg2")  # run 'server' command
                                   # with '-arg1 -arg2' arguments
srv.stdout_join("started")         # wait until the server outputs "started"

cli = spawn("client -arg1 -arg2")  # run 'client' command with some arguments
src.stdout_join("connected")       # wait until the server outputs "connected"

cli.kill                           # send SIGKILL signal to the client
cli.join                           # wait until the client is really dead
srv.stderr_join(/disconnected/)    # stderr and regexp are also usable

srv.stdin.write "status\n"         # input "status\n" to the server
srv.stdout_join("done")            # wait until the server outputs "done"

if srv.stdout.read =~ /^client:/   # read output of the server
	puts "** TEST FAILED **"         # this library is usable for tests
                                   # see also "Unit test" example below
end
=end


####
## Remote process execution
##
=begin
#!/usr/bin/env ruby
require 'chukan'
include Chukan                     # include Chukan

mac = remote("mymac.local")        # login to the remote host using ssh and run
                                   # commands on the host
                                   # use ssh-agent if your key is encrypted
mac.cd("work/myproject")           # run on "work/myproject" directory

linux = remote("192.168.10.2", "myname", ".id_rsa_linux")
                                   # user name and path of the key is optional

cli_on_mac   = mac.spawn("client -arg1")   # run cilent on the remote host
cli_on_linux = linux.spawn("client -arg1")

cli_on_mac.stdout_join("started")  # signals and I/Os are also available
=end


####
## Unit test
##
=begin
#!/usr/bin/env ruby
require 'chukan'
include Chukan::Test               # include Chukan::Test

test "load mylibrary"  do          # Chukan::Test provides 'test' and 'run' methods
	require "mylibrary"              # test will fail if the block returns nil or false,
                                   # or an exception is raised
end

run {|b|                           # 'run' iterates yaml documents written after
                                   # __END__ line
	test "score <= 100", :TODO  do   # second argument of 'test' is :TODO or :SKIP
		b.score <= 100                 # which is useful for Test Anything Protocol
	end                              # (TAP) processor like prove
}

__END__
---                                # yaml documents are here
name:  test A
user:  a-san
score: 10
---
name:  test B
user:  b-san
score: 100
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
				line = ''
				while true
					src.sysread(1024, buf)
					dst.synchronize {
						dst.string << buf
						cond.signal
					}
					line << buf
					line.gsub!(/.*\n/) {|l|
						msgout.puts "[%-12s %6d] #{l}" % [@shortname, @pid]
						""
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


	class RemoteProcess < LocalProcess
		def initialize(remote, *cmdline, &block)
			@remote = remote

			cmdline_real = ["echo","$$","&&","exec"] + cmdline
			super(*remote.command(*cmdline_real), &block)

			@shortname = File.basename(cmdline.first.split(/\s/,2).first)[0, 7] +
				"@"+remote.host[0,5]

			stdout_join("\n")
			@rpid = stdout.gets.to_i
		end
		attr_reader :rpid

		def signal(sig)
			system(*@remote.command("kill", "-#{sig}", @rpid))
			self
		end
	end


	class Remote
		def initialize(host, user = nil, key = nil)
			@host = host
			@user = user
			@key  = key
			@dir  = nil
		end
		attr_reader :host

		def cd(dir = nil)
			@dir = dir
			self
		end

		def command(*cmdline)
			ssh = ENV["SSH"] || "ssh"
			cmd = [ssh, "-o", "Batchmode yes"]
			cmd += ["-i", @key] if @key
			if @user
				cmd.push "#{@user}:#{@host}"
			else
				cmd.push @host
			end
			if @dir
				cmd += ["cd", @dir, "&&"]
			end
			cmd + cmdline
		end

		def spawn(*cmdline, &block)
			RemoteProcess.new(self, *cmdline, &block)
		end
	end


	def spawn(*cmdline, &block)
		LocalProcess.new(*cmdline, &block)
	end

	def remote(host, user = nil, key = nil)
		Remote.new(host, user, key)
	end


	module Test
		@@start = nil
		@@cases = Hash.new {|hash,key| hash[key] = [0,0,0] }
		@@count = 0
		@@data  = nil

		if ENV["TERM"] =~ /color/i && $stdout.stat.chardev?
			module Color
				SUCCESS = "\e[0;32m"
				FAIL    = "\e[1;33m"
				ERROR   = "\e[0;31m"
				NORMAL  = "\e[00m"
			end
		else
			module Color
				SUCCESS = ""
				FAIL    = ""
				ERROR   = ""
				NORMAL  = ""
			end
		end
	
		def self.report
			proc {
				finish = Time.now
				puts "\n1..#{@@count}"
				$stderr.puts "Finished in #{finish - @@start} seconds."
				$stderr.puts ""
				succes  = @@cases.to_a.inject(0) {|r,(n,c)| r + c[0] }
				failure = @@cases.to_a.inject(0) {|r,(n,c)| r + c[1] }
				error   = @@cases.to_a.inject(0) {|r,(n,c)| r + c[2] }
				$stderr.puts "#{@@cases.size} tests, " +
					"#{succes+failure+error} assertions, " +
					"#{Color::FAIL}#{failure} failures, " +
					"#{Color::ERROR}#{error} errors" +
					"#{Color::NORMAL}"
			}
		end
	
		def self.included(*mod)
			unless @@start
				ObjectSpace.define_finalizer(mod, report)
				@@start = Time.now
			end
		end
	
		def test(name = nil, directive = nil, &block)
			if yield
				tap(Color::SUCCESS, "ok", @@count+=1, name, directive)
				@@cases[name][0] += 1
			else
				tap(Color::FAIL, "not ok", @@count+=1, name, directive)
				print_backtrace(caller, "test failed")
				@@cases[name][1] += 1
			end
		rescue Exception
			tap(Color::ERROR, "not ok", @@count+=1, name, directive)
			print_backtrace($!.backtrace, "#{$!} (#{$!.class})")
			@@cases[name][2] += 1
		ensure
			print Color::NORMAL
		end

		def data
			require 'yaml'
			@@data ||= YAML.load_stream(DATA.read.gsub(/(^\t+)/) {
				'  ' * $+.length
			}).documents.map {|obj|
				obj.each_pair {|k,v|
					(class<<obj; self; end).instance_eval do
						define_method(k) { v }
					end rescue nil
				} rescue obj
			}
		end

		def run(&block)
			data.each {|d|
				yield d
			}
		end

		private
		def tap(color, stat, count, name, directive = nil)
			if directive
				directive = "  # #{directive.to_s.upcase}"
			end
			puts "#{color}\n#{stat} #{count} - #{name}#{directive}"
		end

		def print_backtrace(trace, msg)
			$stderr.puts "#{trace.shift}: #{msg}"
			trace.each {|c|
				unless c.to_s.include?(__FILE__)
					$stderr.puts "\tfrom  #{c}"
				end
			}
		end
	end

end

