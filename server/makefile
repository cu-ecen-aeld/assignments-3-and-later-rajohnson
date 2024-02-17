CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -pthread

TARGET = aesdsocket

all: $(TARGET)

valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./$(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) $(TARGET).c -o $(TARGET) 

clean:
	$(RM) $(TARGET) valgrind-out.txt
