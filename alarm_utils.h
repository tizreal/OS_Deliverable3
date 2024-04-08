//
//  alarm_utils.h
//  OS_ASSIGNMENT
//
//  Created by Anjolaoluwa, Adeyemi on 2024-04-08.
//

#ifndef alarm_utils_h
#define alarm_utils_h

#include <stdio.h>
#include <string.h>

typedef enum {
    START_ALARM,
    CHANGE_ALARM,
    CANCEL_ALARM,
    INVALID_REQUEST
} alarm_request_type;

alarm_request_type get_request_type(const char *request_type);

#endif /* alarm_utils_h */

