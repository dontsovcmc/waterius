#include "mqtt_command.h"

#include <string.h>

#define SET_SUFFIX "/set"

bool parse_mqtt_command(const char *topic, const size_t payload_len,
                        size_t *name_pos, size_t *name_len)
{
    if (payload_len == 0)
    {
        return false;
    }

    const size_t suffix_len = strlen(SET_SUFFIX);
    const size_t topic_len = strlen(topic);
    if (topic_len <= suffix_len
        || strcmp(topic + topic_len - suffix_len, SET_SUFFIX) != 0)
    {
        return false;
    }

    const size_t end = topic_len - suffix_len;
    size_t start = end;
    while (start > 0 && topic[start - 1] != '/')
    {
        start--;
    }

    if (start == end)   // <корень>//set
    {
        return false;
    }

    *name_pos = start;
    *name_len = end - start;
    return true;
}
