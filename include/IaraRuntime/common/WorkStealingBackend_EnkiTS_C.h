#ifndef IARA_RUNTIME_WORKSTEALINGBACKEND_ENKITS_C_H
#define IARA_RUNTIME_WORKSTEALINGBACKEND_ENKITS_C_H

// C-friendly interface for EnkiTS parallelism
// This header can be included from C files

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*iara_task_func)(void*);

void iara_submit_task_c(iara_task_func func, void* data);
void iara_task_wait_c(void);
void iara_parallelism_init_c(void);
void iara_parallelism_shutdown_c(void);

#ifdef __cplusplus
}
#endif

#endif // IARA_RUNTIME_WORKSTEALINGBACKEND_ENKITS_C_H
