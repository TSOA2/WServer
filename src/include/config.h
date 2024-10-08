#ifndef _CONFIG_HEADER_GUARD
#define _CONFIG_HEADER_GUARD

/*** The port the server will listen on ***/
#define WSERVER_PORT     "8080"

/*** The max amount of pending connections ***/
#define WSERVER_MAX_CON  (500)

/*** The max amount of buffer the server will allocate for a    ***/
/*** request (in kilobytes). If undefined, infinite (dangerous) ***/
#define WSERVER_MAX_BUF  (10)

/*** Set to one to enable logging, 0 to disable it. ***/
#define WSERVER_ENABLE_LOG (1)

/*** Log file WServer will output to. Leave undefined for stdout ***/
//#define WSERVER_LOG_FILE "test.log"

#endif // _CONFIG_HEADER_GUARD
