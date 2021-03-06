INC=-Iinclude
CFLAGS=$(INC) -g

PARSERDIR=input_parse

SRCS=utils.c execute.c my_parse.c mshell.c builtins.c
OBJS:=$(SRCS:.c=.o)

all: mshell 

mshell: $(OBJS) siparse.a
	cc $(CFLAGS) $(OBJS) siparse.a -o $@ 

%.o: %.c
	cc $(CFLAGS) -c $<

siparse.a:
	$(MAKE) -C $(PARSERDIR)

clean:
	rm -f mshell *.o
