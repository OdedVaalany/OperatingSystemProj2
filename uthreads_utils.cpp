#include <setjmp.h>
#include <queue>


#define JB_SP 6
#define JB_PC 7


enum ThreadState {RUNNING,BLOCKED,READY,DELETED};
typedef unsigned long address_t;

struct TCB
{
    int tid;
    ThreadState state;
    unsigned int quantums_count;
    int blocked_quantums_count; //-1 is infinte other automaticly released
    sigjmp_buf _sigjmp_buf;
    char* sp;
};

