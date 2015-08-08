CC=gcc
AR=ar

CONFIGS= \
	 CONFIG_KUT_COLOURS=y

CFLAGS+=-Werror -Wall -g \
	$(patsubst %,-I%,$(INCLUDES)) \
	$(patsubst %=y,-D%,$(filter %=y,$(strip $(CONFIGS))))

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

