CC=gcc
SRC=src/
INC=include/
BLD=build/
LOG=logs/

CFLAGS=-I$(INC) -g -Wall -Werror -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -D_POSIX_C_SOURCE=202504L

# ! MAKE SURE ALL C FILES WITH A MAIN ARE LISTED HERE
# otherwise the makefile will attempt to link those C files causing linker errors
DRIVERS= \
	$(SRC)client/TUI/client.c \
	$(SRC)server/poker_server.c \
	$(SRC)client/automated.c \
	$(SRC)test/file_comparison_test.cpp \

# * for building client code
CLIENT_SRC=$(shell find $(SRC)client/ -type f -name *.c)
CLIENT_OSRC=$(filter-out $(DRIVERS), $(CLIENT_SRC))
CLIENT_OBJS=$(patsubst $(SRC)%,$(BLD)%,$(CLIENT_OSRC:.c=.o))

# * for building server
SERVER_SRC=$(shell find $(SRC)server/ -type f -name *.c)
SERVER_OSRC=$(filter-out $(DRIVERS), $(SERVER_SRC))
SERVER_OBJS=$(patsubst $(SRC)%,$(BLD)%,$(SERVER_OSRC:.c=.o))

# * for building shared objects
SHARED_SRC=$(shell find $(SRC)shared/ -type f -name *.c)
SHARED_OSRC=$(filter-out $(DRIVERS), $(SHARED_SRC))
SHARED_OBJS=$(patsubst $(SRC)%,$(BLD)%,$(SHARED_OSRC:.c=.o))


FUNC_OBJS= $(SRC)shared/utility.c

# ! HOW TO COMPILE !
# to compile src/%.c, run
# 	make compile.%
# this will put the program called prog into the build directory that you can run
#
# to compile a client, run
# 	make client.%
# this will put a program called client.% into the build directory that is run

client.%: $(SRC)client/%.c $(CLIENT_OBJS) $(SHARED_OBJS) $(LOG)
	$(CC) $(CLIENT_OBJS) $(SHARED_OBJS) $(CFLAGS) $< -o $(BLD)$@
	@if [ $$? -eq 0 ]; then \
		echo "\e[32mSuccessfully built executable $(BLD)$@\e[0m"; \
	fi

# ! requires libncurses-dev to be installed
tui.%: $(SRC)client/TUI/%.c $(CLIENT_OBJS) $(SHARED_OBJS) $(LOG)
	$(CC) $(CLIENT_OBJS) $(SHARED_OBJS) $(CFLAGS) $< -lncursesw -o $(BLD)$@
	@if [ $$? -eq 0 ]; then \
		echo "\e[32mSuccessfully built executable $(BLD)$@\e[0m"; \
	fi
 
server.%: $(SRC)server/%.c $(SERVER_OBJS) $(SHARED_OBJS) $(LOG)
	$(CC) $(SERVER_OBJS) $(SHARED_OBJS) $(CFLAGS) $< -o $(BLD)$@
	@if [ $$? -eq 0 ]; then \
		echo "\e[32mSuccessfully built executable $(BLD)$@\e[0m"; \
	fi

# make is trying to be cheeky and is deleting intermediate files
# but this causes the file to be recompiled each time even if the file did not change
# this should prevent the deletion of these intermediate files
.PRECIOUS: $(BLD)client/%.o
$(BLD)client/%.o: $(SRC)/client/%.c $(BLD)client/
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(BLD)client/TUI/%.o
$(BLD)client/TUI/%.o: $(SRC)/client/TUI/%.c $(BLD)client/TUI/
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(BLD)server/%.o
$(BLD)server/%.o: $(SRC)/server/%.c $(BLD)server/
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(BLD)shared/%.o
$(BLD)shared/%.o: $(SRC)/shared/%.c $(BLD)shared/
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(BLD)%/
$(BLD)%/: $(BLD)
	mkdir -p $@

$(BLD):
	mkdir -p $(BLD)

$(LOG):
	mkdir -p $(LOG)

# Add test target for file comparison test
test.file_comparison: $(SRC)/../file_comparison_test.cpp $(BLD)
	$(CXX) $(CFLAGS) $< -lgtest -lgtest_main -pthread -o $(BLD)$@
	@if [ $$? -eq 0 ]; then \
		echo "\e[32mSuccessfully built test $(BLD)$@\e[0m"; \
	fi

untrack:
	@echo "\e[?1003l"

clean:
	@rm -rfv $(BLD)
	@rm -rfv $(LOG)

clean_logs:
	@rm -fv $(LOG)*