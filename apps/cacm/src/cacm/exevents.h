#ifndef EXEVENTS_H_
#define EXEVENTS_H_
#include "cacm_tools.h"

// 事件处理
struct class_exevents
{
    int (* cacm_register_failed)(struct s_cacm *cacm, eXosip_event_t *ev);
    int (* cacm_register_success)(struct s_cacm *cacm, eXosip_event_t *ev);
    int (* cacm_other_request)(struct s_cacm *cacm, eXosip_event_t *ev);
    int (* cacm_parse_event)(struct s_cacm *cacm, eXosip_event_t *ev);
    int (* cacm_parse_status_code)(eXosip_event_t *ev);
};

extern struct class_exevents exevents;

#endif
