require 'common'

mgr = Manager.new
gw  = Gateway.new(0, mgr)

srv = Server.new(0, mgr)
mgr.stdout_join("new node")

mgr.attach.join
mgr.stdout_join("replace finished")

gw.stdout_join("connect success")

client = gw.client
client.set("key0", "val0")
client.set("key1", "val1")
client.set("key2", "val2")

srv.term
gw.term
mgr.term

srv.join
gw.join
mgr.join

