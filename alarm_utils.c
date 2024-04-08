//
//  alarm_utils.c
//  OS_ASSIGNMENT
//
//  Created by Anjolaoluwa, Adeyemi on 2024-04-08.
//

#include "alarm_utils.h"

alarm_request_type get_request_type(const char *request_type) {
    if (strncmp(request_type, "Start_Alarm", 11) == 0) {
        return START_ALARM;
    } else if (strncmp(request_type, "Change_Alarm", 12) == 0) {
        return CHANGE_ALARM;
    } else if (strncmp(request_type, "Cancel_Alarm", 12) == 0) {
        return CANCEL_ALARM;
    } else {
        return INVALID_REQUEST;
    }
}
