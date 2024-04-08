CC = cc
CFLAGS = -D_POSIX_PTHREAD_SEMANTICS

# Add alarm_utils.o if you're using alarm_utils.c
OBJS = New_Alarm_Cond.o alarm_utils.o

all: New_Alarm_Cond

# Link the object files to create the final executable
New_Alarm_Cond: $(OBJS)
    $(CC) -o $@ $^ $(CFLAGS) -lpthread

# Add a rule for compiling New_Alarm_Cond.c to an object file
New_Alarm_Cond.o: New_Alarm_Cond.c errors.h alarm_utils.h
    $(CC) -c $< -o $@ $(CFLAGS)

# Add a rule for compiling alarm_utils.c to an object file if you're using it
alarm_utils.o: alarm_utils.c alarm_utils.h
    $(CC) -c $< -o $@ $(CFLAGS)

clean:
    rm -f New_Alarm_Cond $(OBJS)
