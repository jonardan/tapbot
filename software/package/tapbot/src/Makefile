CFLAGS ?= -O2
CFLAGS += -Wall -Wextra

all: tmc2209ctl tapbot

tmc2209ctl: tmc2209ctl.c tmc2209.c tmc2209.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ tmc2209ctl.c tmc2209.c -lm

tapbot: tapbot.c tmc2209.c tmc2209.h fivebar.c fivebar.h path.c path.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ tapbot.c tmc2209.c fivebar.c path.c -lm

# Pure math, run on the PC: make test
test_fivebar: test_fivebar.c fivebar.c fivebar.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test_fivebar.c fivebar.c -lm

test_path: test_path.c path.c path.h fivebar.c fivebar.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test_path.c path.c fivebar.c -lm

test: test_fivebar test_path
	./test_fivebar
	./test_path

clean:
	rm -f tmc2209ctl tapbot test_fivebar test_path

.PHONY: all clean test
