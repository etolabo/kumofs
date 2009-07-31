CXXFLAGS = -Wall -g -O0

LDFLAGS = -lpthread -lm -Wl,-Bstatic -lmemcached -Wl,-Bdynamic

TESTS = \
		test_set \
		test_get \
		test_delete \
		test_multi \
		test_get_noset \
		test_set_get_delete \
		check_set_get_delete \
		memstrike

%: %.c
	$(CC) $< $(CXXFLAGS) $(LDFLAGS) -o $@

all: $(TESTS)

.PHONY: clean
clean:
	$(RM) $(TESTS)

