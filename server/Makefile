CC := $(CROSS_COMPILE)gcc #Set compiler
CFLAGS := -Wall -Werror #Flags to enable all warnings and treat warning as errors
TARGET := aesdsocket # Name of executable generated
SRC := aesdsocket.c # Name of file to compile
OBJ := $(SRC:.c=.o) #Replace .c with .o for object files

all: $(TARGET) #default taget for make

$(TARGET): $(OBJ) #Link obj files into executable
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC)

%.o: %.c #Compile .c source files into .o files ($< = the source file and $@ = the target obj file)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)
