#ifndef SCRIPT_RUNNER_H
#define SCRIPT_RUNNER_H

#include <stdint.h>
#include <stdbool.h>

void script_runner_init(void);
bool script_runner_is_running(void);
void script_runner_execute(void);
void script_runner_abort(void);
uint32_t script_runner_completed_at(void);

#endif