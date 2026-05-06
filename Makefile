CC     = gcc
CFLAGS = -Wall -std=c11 -lm
TARGET = gaz_sim

all: $(TARGET)
$(TARGET): src/detection_gaz.c
	$(CC) $(CFLAGS) -o $@ $^
clean:
	rm -f $(TARGET)
run: all
	./$(TARGET)
.PHONY: all clean run
