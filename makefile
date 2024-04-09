CC = gcc
CFLAGS = -D_POSIX_PTHREAD_SEMANTICS
OBJS = New_Alarm_Cond.o alarm_utils.o

all: New_Alarm_Cond

New_Alarm_Cond: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) -lpthread

New_Alarm_Cond.o: New_Alarm_Cond.c errors.h alarm_utils.h
	$(CC) -c -o $@ New_Alarm_Cond.c $(CFLAGS)

alarm_utils.o: alarm_utils.c alarm_utils.h
	$(CC) -c -o $@ alarm_utils.c $(CFLAGS)

clean:
	rm -f New_Alarm_Cond $(OBJS)
