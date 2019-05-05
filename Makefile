CFLAGS=-Wall -pedantic -O2
BINS=server client child

all: $(BINS)

%: %.c
	$(CC) $< $(CFLAGS) -o $@

.PHONY: clean
clean:
	@rm -f $(BINS)
