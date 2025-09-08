CC := $(CROSS_COMPILE)gcc #Set compiler
CFLAGS := -Wall -Werror #Compiler flags (enable all warnings and treat warnings as errors)
TARGET := writer # Name of generated executable
SRC := writer.c # Source file

all: $(TARGET) #default target for 'make'

$(TARGET): #Compile from source
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean: #Clean target
	rm -f $(TARGET)

