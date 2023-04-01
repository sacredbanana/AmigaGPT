#pragma once

#ifdef __cplusplus
	extern "C" {
#endif

struct IOStdReq* CreateStdIO(CONST struct MsgPort *port);
struct MsgPort* CreatePort(CONST_STRPTR, LONG);
void DeletePort(struct MsgPort *port);
struct IORequest* CreateExtIO(CONST struct MsgPort *port,LONG iosize);
void DeleteExtIO(struct IORequest *ioreq);

#ifdef __cplusplus
	} 
#endif