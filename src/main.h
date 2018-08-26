#ifndef MTMSG_MAIN_H
#define MTMSG_MAIN_H

#include "util.h"

extern Mutex*        mtmsg_global_lock;
extern AtomicCounter mtmsg_id_counter;
extern bool          mtmsg_abort_flag;

DLL_PUBLIC int luaopen_mtmsg(lua_State* L);

#endif /* MTMSG_MAIN_H */
