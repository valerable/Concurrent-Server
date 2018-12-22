CC := gcc
SRCD := src
TSTD := tests
BLDD := build
BIND := bin
INCD := include
LIBD := lib
UTILD := util

MAIN  := $(BLDD)/main.o
AUX  := $(BLDD)/client.o
LIB := $(LIBD)/xacto.a

ALL_SRCF := $(shell find $(SRCD) -type f -name *.c)
ALL_LIBF := $(shell find $(LIBD) -type f -name *.o)
ALL_OBJF := $(patsubst $(SRCD)/%,$(BLDD)/%,$(ALL_SRCF:.c=.o))
ALL_FUNCF := $(filter-out $(MAIN) $(AUX), $(ALL_OBJF))

TEST_SRC := $(shell find $(TSTD) -type f -name *.c)

INC := -I $(INCD)

CFLAGS := -Wall -Werror -Wno-unused-function -MMD
COLORF := -DCOLOR
DFLAGS := -g -DDEBUG -DCOLOR
PRINT_STAMENTS := -DERROR -DSUCCESS -DWARN -DINFO

STD := -std=gnu11
TEST_LIB := -lcriterion
LIBS := $(LIB) -lpthread

CFLAGS += $(STD)

EXEC := xacto
TEST_EXEC := $(EXEC)_tests
AUX_EXEC := client

.PHONY: clean all setup debug

all: setup $(BIND)/$(EXEC) $(BIND)/$(TEST_EXEC) $(UTILD)/$(AUX_EXEC)

debug: CFLAGS += $(DFLAGS) $(PRINT_STAMENTS) $(COLORF)
debug: all

setup: $(BIND) $(BLDD) $(LIBD)
$(BIND):
	mkdir -p $(BIND)
$(BLDD):
	mkdir -p $(BLDD)
$(LIBD):
	mkdir -p $(LIBD)

$(BIND)/$(EXEC): $(MAIN) $(ALL_FUNCF) $(ALL_LIBF)
	$(CC) $^ -o $@ $(LIBS)

$(BIND)/$(TEST_EXEC): $(ALL_FUNCF) $(TEST_SRC) $(ALL_LIBF)
	$(CC) $(CFLAGS) $(INC) $(ALL_FUNCF) $(TEST_SRC) $(ALL_LIBF) $(TEST_LIB) $(LIBS) -o $@

$(BLDD)/%.o: $(SRCD)/%.c
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	rm -rf $(BLDD) $(BIND)

.PRECIOUS: $(BLDD)/*.d
-include $(BLDD)/*.d
