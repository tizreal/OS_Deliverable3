/*
 * alarm_cond.c
 *
 * This is an enhancement to the alarm_mutex.c program, which
 * used only a mutex to synchronize access to the shared alarm
 * list. This version adds a condition variable. The alarm
 * thread waits on this condition variable, with a timeout that
 * corresponds to the earliest timer request. If the main thread
 * enters an earlier timeout, it signals the condition variable
 * so that the alarm thread will wake up and process the earlier
 * timeout first, requeueing the later request.
 */
#include <pthread.h>
#include <time.h>
#include "errors.h" // for handling errors
#include <string.h>
#include <stdlib.h>

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */
typedef struct alarm_tag
{
    int alarm_id;           // identifier for the alarm
    struct alarm_tag *link; // pointer to the next alarm
    int seconds;            // time in seconds for periodic alarms
    time_t scheduled_time;  // time for the scheduled alarms
    char message[100];
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;
alarm_t *alarm_list = NULL;
alarm_t *Alarm_Display_List = NULL;
pthread_mutex_t alarm_display_list_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex for guiding access for the alarm display list
time_t current_alarm = 0;
void *periodic_display_thread(void *arg);

// Alarm system initialization
void initialize_alarm_system()
{
    int status;

    // Alarm Display List initialization
    Alarm_Display_List = NULL; // Start empty
    // then
    status = pthread_mutex_init(&alram_display_list_mutex, NULL); // initialize the mutex
    if (status != 0)
    {
        err_abort(status, "Initializing alarm display list mutex");
    }
}

/*
 * Insert alarm entry on list, in order.
 */
void alarm_insert(alarm_t *alarm)
{
    int status;
    alarm_t **last, *next;

    /*
     * LOCKING PROTOCOL:
     *
     * This routine requires that the caller have locked the
     * alarm_mutex!
     */
    last = &alarm_list;
    next = *last;
    while (next != NULL)
    {
        if (next->scheduled_time >= alarm->scheduled_time)
        {
            alarm->link = next;
            *last = alarm;
            break;
        }
        last = &next->link;
        next = next->link;
    }
    /*
     * If we reached the end of the list, insert the new alarm
     * there.  ("next" is NULL, and "last" points to the link
     * field of the last item, or to the list header.)
     */
    if (next == NULL)
    {
        *last = alarm;
        alarm->link = NULL;
    }
#ifdef DEBUG
    printf("[list: ");
    for (next = alarm_list; next != NULL; next = next->link)
        printf("%d(%d)[\"%s\"] ", next->time,
               next->time - time(NULL), next->message);
    printf("]\n");
#endif
    /*
     * Wake the alarm thread if it is not busy (that is, if
     * current_alarm is 0, signifying that it's waiting for
     * work), or if the new alarm comes before the one on
     * which the alarm thread is waiting.
     */
    if (current_alarm == 0 || alarm->scheduled_time < current_alarm)
    {
        current_alarm = alarm->scheduled_time;
        status = pthread_cond_signal(&alarm_cond);
        if (status != 0)
            err_abort(status, "Signal cond");
    }
}

/*
 * The alarm thread's start routine.
 */
void *alarm_thread(void *arg)
{
    alarm_t *alarm;
    struct timespec cond_time;
    time_t now;
    int status, expired;

    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the process exits. Lock the mutex
     * at the start -- it will be unlocked during condition
     * waits, so the main thread can insert alarms.
     */
    status = pthread_mutex_lock(&alarm_mutex);
    if (status != 0)
        err_abort(status, "Lock mutex");
    while (1)
    {
        /*
         * If the alarm list is empty, wait until an alarm is
         * added. Setting current_alarm to 0 informs the insert
         * routine that the thread is not busy.
         */
        current_alarm = 0;
        while (alarm_list == NULL)
        {
            status = pthread_cond_wait(&alarm_cond, &alarm_mutex);
            if (status != 0)
                err_abort(status, "Wait on cond");
        }
        alarm = alarm_list;
        alarm_list = alarm->link;
        now = time(NULL);
        expired = 0;
        if (alarm->scheduled_time > now)
        {
#ifdef DEBUG
            printf("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                   alarm->time - time(NULL), alarm->message);
#endif
            cond_time.tv_sec = alarm->scheduled_time;
            cond_time.tv_nsec = 0;
            current_alarm = alarm->scheduled_time;
            while (current_alarm == alarm->scheduled_time)
            {
                status = pthread_cond_timedwait(
                    &alarm_cond, &alarm_mutex, &cond_time);
                if (status == ETIMEDOUT)
                {
                    expired = 1;
                    break;
                }
                if (status != 0)
                    err_abort(status, "Cond timedwait");
            }
            if (!expired)
                alarm_insert(alarm);
        }
        else
            expired = 1;
        if (expired)
        {
            printf("(%d) %s\n", alarm->seconds, alarm->message);
            free(alarm);
        }
    }
}

int main(int argc, char *argv[])
{
    int status;
    char line[128];
    alarm_t *alarm;
    pthread_t thread;

    status = pthread_create(
        &thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort(status, "Create alarm thread");
    while (1)
    {
        printf("Alarm> ");
        if (fgets(line, sizeof(line), stdin) == NULL)
            exit(0);
        if (strlen(line) <= 1)
            continue;
        alarm = (alarm_t *)malloc(sizeof(alarm_t));
        if (alarm == NULL)
            errno_abort("Allocate alarm");

        /*
         * Parse input line into seconds (%d) and a message
         * (%64[^\n]), consisting of up to 64 characters
         * separated from the seconds by whitespace.
         */
        if (sscanf(line, "%d %64[^\n]",
                   &alarm->seconds, alarm->message) < 2)
        {
            fprintf(stderr, "Bad command\n");
            free(alarm);
        }
        else
        {
            status = pthread_mutex_lock(&alarm_mutex);
            if (status != 0)
                err_abort(status, "Lock mutex");
            alarm->scheduled_time = time(NULL) + alarm->seconds;
            /*
             * Insert the new alarm into the list of alarms,
             * sorted by expiration time.
             */
            alarm_insert(alarm);
            status = pthread_mutex_unlock(&alarm_mutex);
            if (status != 0)
                err_abort(status, "Unlock mutex");
        }
    }
}

/// IMPLEMENTATION TODO

// IMPLEMENT CONSUMER THREAD HERE
void *consumer_thread(void *arg)
{
}

extern alarm_t *Alarm_Display_List;
extern pthread_mutex_t alram_display_list_mutex;

// IMPLEMENT PERDIODIC DISPLAY THREAD HERE
void *periodic_display_thread(void *arg)
{
}

// IMPLEMENT CIRCULAR BUFFER: DATA STRUCTURE BETWEEN THE ALARM THREAD AND CONSUMER THREAD
typedef struct circular_buffer()
{
}

// IMPLEMENT Alarm_Display_List, DATASTRUCTURE BETWEEN THE CONSUMER THREAD AND PERIODIC DISPLAY THREAD
typedef struct alram_display_list()
{
}

// IMPLEMENT A FUNCTION TO PROCESS THE ALARM REQUEST
