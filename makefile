GC = gcc
PING = ping.o
WATCH = watchdog.o
NEW = new_ping.o
WARN= -Wall -g

all: ping watchdog new_ping
ping:  $(PING)
	$(GC) $(WARN) $(PING) -o parta
watchdog: $(WATCH)
	$(GC) $(WARN) $(WATCH) -o watchdog
new_ping: $(NEW)
	$(GC) $(WARN) $(NEW) -o partb
ping.o: ping.c 
	$(GC) $(WARN) -c ping.c
watchdog.o: watchdog.c
	$(GC) $(WARN) -c watchdog.c
new_ping.o: new_ping.c 
	$(GC) $(WARN) -c new_ping.c
clean:
	rm -f *.o parta watchdog partb
.PHONY: clean all
