CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra

TARGET = aesdsocket

all: $(TARGET)

valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./$(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	$(RM) $(TARGET) valgrind-out.txt
