TARGET=mkzte

CFLAGS=-O2 -Wall -s

ifeq ($(OS),Windows_NT)
    CFLAGS += -l ws2_32
endif

$(TARGET):

clean:
	$(RM) $(TARGET)
