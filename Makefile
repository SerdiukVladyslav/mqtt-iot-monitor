CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -Isrc
LDFLAGS = -lmosquitto -lcjson -lm

SRCS = src/main.c src/device.c src/parser.c src/logger.c
OBJS = $(SRCS:.c=.o)
TARGET = iot_monitor

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
