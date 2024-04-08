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
#include "errors.h"
#include "alarm_utils.h"

#define MAX_MESSAGE_LENGTH 128
#define CIRCULAR_BUFFER_SIZE 4

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */
typedef struct alarm_tag {
    int                 id;
    struct alarm_tag    *link;
    int                 seconds;
    time_t              time;   /* seconds from EPOCH */
    char message        [MAX_MESSAGE_LENGTH + 1];
} alarm_t;

typedef struct {
    alarm_t *buffer[CIRCULAR_BUFFER_SIZE];
    int insert_at;
    int remove_at;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} circular_buffer_t;

// the alarm mutex is to control access to the alarm_list
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;

// communicates the state of the alarm thread -- this is the conditional variable used to signify if mutex is available or not
// it controls access to mutex basically
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;

// data structure used to communicate between main and alarm thread
alarm_t *alarm_list = NULL;

// conditional variable to signal alarm thread(this avoid busy waiting and delegation from alarm thread to let other thread run)
time_t current_alarm = 0;

void alarm_insert(alarm_t *alarm);
void *alarm_thread(void *arg);
void *consumer_thread(void *arg);


int main (int argc, char *argv[])
{
    int status;
    char line[129]; // increased to handle null terminator
    alarm_t *alarm;
    pthread_t alarm_thread_id, consumer_thread_id;
    
    // create alarm thread
    status = pthread_create (&alarm_thread_id, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");
    
    status = pthread_create (&consumer_thread_id, NULL, consumer_thread, NULL);
    if (status != 0)
        err_abort (status, "Create consumer thread");
    
    
    while (1) {
        printf ("Alarm> ");
        if (fgets (line, sizeof (line), stdin) == NULL) exit (0);
        if (strlen (line) <= 1) continue;
        
        alarm = (alarm_t*)malloc (sizeof (alarm_t));
        if (alarm == NULL)
            errno_abort ("Allocate alarm");
        
        // pare the input line according to alarm request type
        alarm_request_type request_type = get_request_type(line);
        switch (request_type) {
            case START_ALARM:
            {
                if (sscanf(line, "Start_Alarm(%d): %d %128[^\n]", &alarm->id, &alarm->seconds, alarm->message) == 3) {
                    if (alarm->id <= 0 || alarm->seconds < 0) {
                        fprintf(stderr, "Alarm ID and Time must be positive\n");
                        free(alarm);
                        continue;  // Skip to the next iteration
                    }
                    
                    // Trunctuate Message
                    alarm->message[MAX_MESSAGE_LENGTH] = '\0';
                    
                    // lock mutex in order to insert the alarm
                    status = pthread_mutex_lock (&alarm_mutex);
                    if (status != 0) err_abort (status, "Lock mutex");
                    
                    alarm->time = time (NULL) + alarm->seconds;

                    
                    alarm_insert(alarm);
                    
                    // unlock mutex
                    status = pthread_mutex_unlock (&alarm_mutex);
                    if (status != 0) err_abort (status, "Unlock mutex");
                    
                    printf("Main Thread has Inserted Start_Alarm Request(%d) at %ld: Time = %d Message = %s into Alarm List\n",
                           alarm->id, time(NULL), alarm->seconds, alarm->message);
                } else {
                    fprintf (stderr, "Bad command\n");
                    free (alarm);
                }
                break;
            }
            case CHANGE_ALARM:
            {
                if (sscanf(line, "Change_Alarm(%d): %d %128[^\n]", &alarm->id, &alarm->seconds, alarm->message) == 3) {
                    if (alarm->id <= 0 || alarm->seconds < 0) {
                        fprintf(stderr, "Alarm ID and Time must be positive\n");
                        free(alarm);
                        continue;  // Skip to the next iteration
                    }
                    
                    // Trunctuate Message
                    alarm->message[MAX_MESSAGE_LENGTH] = '\0';
                    
                    // lock mutex in order to insert the alarm
                    status = pthread_mutex_lock (&alarm_mutex);
                    if (status != 0) err_abort (status, "Lock mutex");
                    
                    alarm->time = time (NULL) + alarm->seconds;

                    
                    alarm_insert(alarm);
                    
                    // unlock mutex
                    status = pthread_mutex_unlock (&alarm_mutex);
                    if (status != 0) err_abort (status, "Unlock mutex");
                    
                    printf("Main Thread has Inserted Change_Alarm Request(%d) at %ld: Time = %d Message = %s into Alarm List\n",
                           alarm->id, time(NULL), alarm->seconds, alarm->message);
                } else {
                    fprintf (stderr, "Bad command\n");
                    free (alarm);
                }
                break;
            }
            case CANCEL_ALARM:
            {
                int alarm_id;
                if(sscanf(line, "Cancel_Alarm(%d)", &alarm_id) == 1) {
                    if (alarm->id <= 0 || alarm->seconds < 0) {
                        fprintf(stderr, "Alarm ID and Time must be positive\n");
                        free(alarm);
                        continue;  // Skip to the next iteration
                    }
                    
                    // Trunctuate Message
                    alarm->message[MAX_MESSAGE_LENGTH] = '\0';
                    
                    // lock mutex in order to insert the alarm
                    status = pthread_mutex_lock (&alarm_mutex);
                    if (status != 0) err_abort (status, "Lock mutex");
                    
                    alarm->time = time (NULL) + alarm->seconds;

                    
                    alarm_insert(alarm);
                    
                    // unlock mutex
                    status = pthread_mutex_unlock (&alarm_mutex);
                    if (status != 0) err_abort (status, "Unlock mutex");
                    
                    printf("Main Thread has Inserted Cancel_Alarm Request(%d) at %ld: Time = %d into Alarm List\n",
                           alarm->id, time(NULL), alarm->seconds);
                } else {
                    fprintf (stderr, "Bad command\n");
                    free (alarm);
                }
                break;
            }
            case INVALID_REQUEST:
                fprintf(stderr, "Invalid Command\n");
                break;
        }
    }
}


/*
 * Insert alarm entry on list, in order(ascending order).
 Sorted from earliest time to latest time
 */
void alarm_insert (alarm_t *alarm)
{
    
    // TODO: might have to move the function below to consumer thread
    // Check for duplicate ID
    for (alarm_t *ptr = alarm_list; ptr != NULL; ptr = ptr->link) {
        if (ptr->id == alarm->id) {
            fprintf(stderr, "Alarm ID already exists.\n");
            free(alarm);
            alarm = NULL;
            break;
        }
    }
    
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
    while (next != NULL) {
        if (next->time >= alarm->time) {
            // insert new alarm before the next
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
    if (next == NULL) {
        *last = alarm;
        alarm->link = NULL;
    }
#ifdef DEBUG
    printf ("[list: ");
    for (next = alarm_list; next != NULL; next = next->link)
        printf ("%d(%d)[\"%s\"] ", next->time,
            next->time - time (NULL), next->message);
    printf ("]\n");
#endif
    /*
     * Wake the alarm thread if it is not busy (that is, if
     * current_alarm is 0, signifying that it's waiting for
     * work), or if the new alarm comes before the one on
     * which the alarm thread is waiting.
     */
    if (current_alarm == 0 || alarm->time < current_alarm) {
        current_alarm = alarm->time;
        status = pthread_cond_signal (&alarm_cond);
        if (status != 0)
            err_abort (status, "Signal cond");
    }
}

/*
 * The alarm thread's start routine.
 */
void *alarm_thread (void *arg)
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
        err_abort (status, "Lock mutex");
    
    while (1) {
        /*
         * ONLY waits(goes to sleep) when alarm list is empty
         * If the alarm list is empty, wait until an alarm is
         * added. Setting current_alarm to 0 informs the insert
         * routine that the thread is not busy.
         */
        current_alarm = 0;
        while (alarm_list == NULL) {
            // the below line makes the thread sleeps, until the alamr_cond is broadcasted by another thread
            status = pthread_cond_wait (&alarm_cond, &alarm_mutex);
            if (status != 0)
                err_abort (status, "Wait on cond");
        }
        
        alarm = alarm_list;
        alarm_list = alarm->link;
        now = time (NULL);
        expired = 0;
        if (alarm->time > now) {
#ifdef DEBUG
            printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                alarm->time - time (NULL), alarm->message);
#endif
            cond_time.tv_sec = alarm->time;
            cond_time.tv_nsec = 0;
            current_alarm = alarm->time;
            while (current_alarm == alarm->time) {
                // process the time in the alarm below until the cond_time expires or the alarm_cond is signaled(in this case a new alarm has been added)
                status = pthread_cond_timedwait (&alarm_cond, &alarm_mutex, &cond_time);
                
                // checks if the time exipred
                if (status == ETIMEDOUT) {
                    expired = 1;
                    break;
                }
                
                // error check from call
                if (status != 0)
                    err_abort (status, "Cond timedwait");
            }
            
            // if the alarm didnt expire and an alarm got processed in place of it, reinsert the alarm
            if (!expired)
                alarm_insert (alarm);
        } else {
            // alarm already expired
            expired = 1;
        }
        
        // print the expired alarm
        if (expired) {
            printf ("Expired: (%d) %s\n", alarm->seconds, alarm->message);
            free (alarm);
        }
    }
}

void *consumer_thread(void *arg) {
    // This thread consumes data from the circular buffer
    // Placeholder for logic based on the requirements
    return NULL;
}
