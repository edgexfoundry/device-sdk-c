CC=gcc
CFLAGS=-Wall -g

LD=gcc
LDFLAGS=

TARGET=device-example-c

SOURCES=template.c

#####################################

OBJECTS=$(SOURCES:.c=.o)

$(TARGET): $(OBJECTS)
	$(LD) -o $@ $^ -L$(CSDK_DIR)/lib -lcsdk $(LDFLAGS)
%.o: %.c
	$(CC) $(CFLAGS) -I$(CSDK_DIR)/include -o $@ -c $<

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(TARGET)
