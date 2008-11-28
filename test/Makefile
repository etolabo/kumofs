CXXFLAGS = -Wall -g -O0

LDFLAGS = -lpthread -lm -Wl,-Bstatic -lmemcached -Wl,-Bdynamic

TESTS = test_get test_delete test_multi test_nowrite test_set test_set_get_delete test_bench

%: %.c
	$(CC) $< $(CXXFLAGS) $(LDFLAGS) -o $@

all: $(TESTS)

.PHONY: clean
clean:
	$(RM) $(TESTS)

