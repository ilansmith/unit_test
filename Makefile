CC=gcc
AR=ar

CFLAGS+=-Werror -Wall -g \
	$(CONFIGS_Y:%=-D%=y) \
	$(INCLUDES:%=-I%)

OBJS=unit_test.o

%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

all: libcore

libcore: $(OBJS)
	$(AR) -ru $@.a $^

clean:
	@rm -f $(OBJS)

cleanall: clean
	@rm -rf tags
	@rm -f libcore.a

