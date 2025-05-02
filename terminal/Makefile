#compiler
CC = gcc
#Flags for compile
CFLAGS = -g -Wall -Wextra -pedantic -O0
CJSON_DIR = Libs
SRCS = client.c $(CJSON_DIR)/cJSON.c
OBJS = $(SRCS:.c=.o)
MAIN = client

CJSON_SRC = $(CJSON_DIR)/cJSON.c
CJSON_OBJ = $(CJSON_DIR)/cJSON.o
# dependencies 
all: $(MAIN) 
#dependencies:
#	apt-get update
#	apt-get install -y libssl-dev
#	apt-get install -y libxml2 libxml2-dev

parse: $(CJSON_OBJ)
	@echo cJSON library built

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LIBS) -lcurl 
		@echo New User Client Generated	 
$(CJSON_OBJ): $(CJSON_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
clean:
	$(RM) *.o *~ $(MAIN)
