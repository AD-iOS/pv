/* The simplified version of the libproc.h header document used in iOS compilation */

#ifndef _LIBPROC_H_
#define _LIBPROC_H_

#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int proc_pidinfo(int pid, int flavor, uint64_t arg, 
                        void *buffer, int buffersize);
extern int proc_pidfdinfo(int pid, int fd, int flavor,
                          void *buffer, int buffersize);
extern int proc_listpidspath(uint32_t type, uint32_t typeinfo,
                             const char *path, uint32_t pathflags,
                             void *buffer, int buffersize);
extern int proc_pidpath(int pid, void *buffer, uint32_t buffersize);
extern int proc_name(int pid, void *buffer, uint32_t buffersize);

#ifdef __cplusplus
}
#endif

#endif /* _LIBPROC_H_ */