HOST_SRCS := $(wildcard *.c)
HOST_OBJS := $(addprefix $(BUILD_DIR)/,$(HOST_SRCS:.c=.o))

include $(EXTBLD_TARGETS)

$(BUILD) : $(HOST_OBJS)
	@touch $@

CC := $(EMBOX_CROSS_COMPILE)gcc
CFLAGS := $(filter -g% -f% -m% -O% -G% -E%,$(EMBOX_CFLAGS))
CPPFLAGS := -I../include/kernel/
$(BUILD_DIR)/%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
