#compiler
CC = gcc
#Flags for compile
CFLAGS = -g -Wall -Wextra -pedantic -O0
SRCS = client.c
OBJS = $(SRCS:.c=.o)
MAIN = client
# dependencies 
all: $(MAIN) 
#dependencies:
#	apt-get update
#	apt-get install -y libssl-dev
#	apt-get install -y libxml2 libxml2-dev

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LIBS) -lcurl 
		@echo New User Client Generated	 
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
clean:
	$(RM) *.o *~ $(MAIN)
