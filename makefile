
CC = gcc
CFLAGS = -Wextra -Wall -std=gnu99 -Iinclude -Wno-unused-parameter -Wno-unused-variable -Wno-duplicate-decl-specifier

MQTT_C_SOURCES = src/mqtt.c src/mqtt_pal.c src/parse.c
MQTT_C_EXAMPLES = bin/simple_subscriber 

BINDIR = bin

all: $(BINDIR) $(MQTT_C_EXAMPLES)

bin/simple_subscriber: src/simple_subscriber.c $(MQTT_C_SOURCES)
	$(CC) $(CFLAGS) $^ -lpthread -o $@


$(BINDIR):
	mkdir -p $(BINDIR)


clean:
	rm -rf $(BINDIR)
