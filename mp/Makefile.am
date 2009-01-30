
NEED_PREPROCESS = \
		object_callback.h
		wavy/core.h \
		wavy/singleton.h

MOSTLYCLEANFILES = $(NEED_PREPROCESS)

EXTRA_DIST = $(NEED_PREPROCESS)

noinst_HEADERS = \
	exception.h \
	functional.h \
	memory.h \
	object_callback.h \
	pp.h \
	pthread.h \
	pthread_impl.h \
	stream_buffer.h \
	stream_buffer_impl.h \
	utility.h \
	wavy/core.h \
	wavy/output.h \
	wavy/singleton.h \
	wavy.h

# FIXME GNU make extension
%.h: %.pre.h
	$(RUBY) -e '\
		def args(n, &block) ;\
			Array.new(n) {|i| yield i+1} .join(", ") ;\
		end ;\
		src = ARGF.read ;\
		src.gsub!(/^MP_ARGS_BEGIN$$(.*?)^MP_ARGS_END$$/m) {|code| ;\
			result = [] ;\
			1.upto(15) {|n| ;\
				line = code.split("\n")[1..-2].join("\n") ;\
				line.gsub!(/MP_ARGS_TEMPLATE/,   args(n) {|i| "typename A#{i}" }) ;\
				line.gsub!(/MP_ARGS_PARAMS_PTR/, args(n) {|i| "A#{i}* a#{i}"   }) ;\
				line.gsub!(/MP_ARGS_PARAMS_REF/, args(n) {|i| "A#{i}& a#{i}"   }) ;\
				line.gsub!(/MP_ARGS_PARAMS/,     args(n) {|i| "A#{i} a#{i}"    }) ;\
				line.gsub!(/MP_ARGS_FUNC/,       args(n) {|i| "a#{i}"          }) ;\
				line.gsub!(/MP_ARGS_TYPES_PTR/,  args(n) {|i| "A#{i}*"    }) ;\
				line.gsub!(/MP_ARGS_TYPES_REF/,  args(n) {|i| "A#{i}&"    }) ;\
				line.gsub!(/MP_ARGS_TYPES/,      args(n) {|i| "A#{i}"     }) ;\
				line.gsub!(/MP_ARGS_ITERATOR_BEGIN$$(.*?)^MP_ARGS_ITERATOR_END$$/m) {|sub| ;\
					subresult = [] ;\
					subline = sub.split("\n")[1..-2].join("\n") ;\
					1.upto(n) {|it| ;\
						sublineit = subline.dup ;\
						sublineit.gsub!(/MP_ARGS_ITERATOR_PARAM/,    "a#{it}") ;\
						sublineit.gsub!(/MP_ARGS_ITERATOR_TYPE_PTR/, "A#{it}*") ;\
						sublineit.gsub!(/MP_ARGS_ITERATOR_TYPE_REF/, "A#{it}&") ;\
						sublineit.gsub!(/MP_ARGS_ITERATOR_TYPE/,     "A#{it}") ;\
						sublineit.gsub!(/\[MP_ARGS_ITERATOR\]/,      "#{it}" ) ;\
						if it == 1 ;\
							sublineit.gsub!(/MP_ARGS_ITERATOR_FIRST_COLON/, " " ) ;\
						else ;\
							sublineit.gsub!(/MP_ARGS_ITERATOR_FIRST_COLON/, "," ) ;\
						end ;\
						if it == n ;\
							sublineit.gsub!(/MP_ARGS_ITERATOR_LAST_COLON/, " " ) ;\
						else ;\
							sublineit.gsub!(/MP_ARGS_ITERATOR_LAST_COLON/, "," ) ;\
						end ;\
						subresult << sublineit ;\
					} ;\
					subresult.join("\n") ;\
				} ;\
				result << line ;\
			} ;\
			result.join("\n") ;\
		} ;\
		puts src' \
	$< > $@.tmp
	mv $@.tmp $@


