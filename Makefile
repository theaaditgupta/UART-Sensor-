CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -Iinclude -g
SRCDIR  = src
TESTDIR = tests
OUTDIR  = build

SRCS    = $(SRCDIR)/ring_buffer.c $(SRCDIR)/flash_store.c $(SRCDIR)/power.c
MAIN    = $(SRCDIR)/main.c

.PHONY: all run test clean

all: $(OUTDIR)/sensor_node

$(OUTDIR)/sensor_node: $(SRCS) $(MAIN) | $(OUTDIR)
	$(CC) $(CFLAGS) $^ -o $@

run: $(OUTDIR)/sensor_node
	./$(OUTDIR)/sensor_node

test: $(OUTDIR)/test_ring_buffer
	./$(OUTDIR)/test_ring_buffer

$(OUTDIR)/test_ring_buffer: $(SRCDIR)/ring_buffer.c $(TESTDIR)/test_ring_buffer.c | $(OUTDIR)
	$(CC) $(CFLAGS) $^ -o $@

validate:
	python3 scripts/validate_firmware.py

$(OUTDIR):
	mkdir -p $(OUTDIR)

clean:
	rm -rf $(OUTDIR)
