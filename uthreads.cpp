#include "uthreads.h"




static std::unordered_map<int,TCB> threads_map = std::unordered_map<int,TCB>(MAX_THREAD_NUM);
static std::priority_queue<int,std::vector<int>,std::greater<int>> empty_ids;
static std::queue<int> ready_threads;
static std::unordered_set<int> blocked_threads;
static int running_thread;
static struct itimerval timer;
static struct sigaction sa = {0};
static unsigned long total_quantoms =0;

void filter_queue(std::queue<int>& qu,int tid){
    for(int i = 0; i < qu.size();i++){
        if(qu.front() != tid){
            qu.push(qu.front());
        }
        qu.pop();
    }
}

/**
 * @brief A translation is required when using an address of a variable. 
 * Use this as a black box in your code.
 * @return address_t the os location in the real memory
*/
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}
void time_hanlder(int sig){
    if(!ready_threads.empty())
    {
            if(sigsetjmp(threads_map[running_thread]._sigjmp_buf, 1) == 0){
            total_quantoms++;
            ready_threads.push(running_thread);
            threads_map[running_thread].state = READY;
            running_thread = ready_threads.front();
            threads_map[running_thread].state = RUNNING;
            threads_map[running_thread].quantums_count++;
            ready_threads.pop();

            for(const int tid : blocked_threads){
                if(threads_map[tid].blocked_quantums_count > -1){
                    if(threads_map[tid].blocked_quantums_count == 0){
                        std::cout << tid << std::endl;
                        uthread_resume(tid);
                    }
                    else{
                        threads_map[tid].blocked_quantums_count--;    
                        std::cout << "thread " << tid << " sleep more " << threads_map[tid].blocked_quantums_count << std::endl;
                    }
                }
            }

            siglongjmp(threads_map[running_thread]._sigjmp_buf, 1);
            }
    }
}

void busy_main(){
    while(true){
        //std::cout << "hello from main" << std::endl;
    }
}

void func1(){
    while(true){
        //std::cout << "hello from 1" << std::endl;
    }
}
void func2(){
    while(true){
    std::cout << "hello I am func 2 now I am going to sleep" << std::endl;
    uthread_sleep(10);
    }
}

int uthread_init(int quantum_usecs){
    if(quantum_usecs <= 0 ) return -1; //error because time cant be negetive

    threads_map[0].quantums_count = 0;
    threads_map[0].state = RUNNING;
    threads_map[0].tid = 0;


    running_thread = 0;

    for(int i=1;i< MAX_THREAD_NUM; i++){
        empty_ids.push(i);
    }

    uthread_spawn(&func1);
    uthread_spawn(&func2);
    
    sa.sa_handler = &time_hanlder;
    if(sigaction(SIGVTALRM,&sa,NULL)<0) std::cout << "sigaction error";

    //setting the timer 
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = quantum_usecs;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = quantum_usecs;


    if(setitimer(ITIMER_VIRTUAL,&timer,NULL)){
        std::cout << "set timer error";
    }

    sigsetjmp(threads_map[0]._sigjmp_buf, 1);
    threads_map[0]._sigjmp_buf->__jmpbuf[JB_PC] = translate_address((address_t) &busy_main);
    siglongjmp(threads_map[0]._sigjmp_buf,1);
    return 0;
}

int uthread_spawn(thread_entry_point entry_point){
    if(empty_ids.empty()){
        std::cerr << "thread library error: too much threads active" << std::endl;
        return -1;
    }
    int tid = empty_ids.top();
    threads_map[tid].blocked_quantums_count = -1;
    threads_map[tid].sp = (char*) malloc(STACK_SIZE);
    if(threads_map[tid].sp == NULL){
        std::cerr << "system error: faild allocate new stack" << std::endl;
        exit(1);
    }
    address_t sp = (address_t) threads_map[tid].sp + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(threads_map[tid]._sigjmp_buf, 1);
    (threads_map[tid]._sigjmp_buf->__jmpbuf)[JB_SP] = translate_address(sp);
    (threads_map[tid]._sigjmp_buf->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&threads_map[tid]._sigjmp_buf->__saved_mask);
    threads_map[tid].tid = tid;
    threads_map[tid].quantums_count = 0;
    threads_map[tid].state = READY;
    ready_threads.push(tid);
    empty_ids.pop();
    return 0;
}

int uthread_terminate(int tid){
    if(tid == 0){
        for(const std::pair<int,TCB> item : threads_map){
            if(item.second.sp != NULL){
                free(item.second.sp);
            }
        }
        exit(0);
    }
    if(tid >= MAX_THREAD_NUM){
        std::cerr << "thread library error: the tid not fit the MAX_THREAD_NUM" << std::endl;
        return -1;
    }
    if(threads_map[tid].sp == NULL){
        std::cerr << "thread library error: thread not active" << std::endl;
        return -1;
    }
    free(threads_map[tid].sp);
    threads_map[tid].sp = NULL;
    empty_ids.push(tid);
    return 0;
}

int _uthread_block(int tid,int quantums){
    if(tid == 0){
        std::cerr << "thread library error: can't block the main thread" << std::endl;
        return -1;
    }
    if(tid >= MAX_THREAD_NUM){
        std::cerr << "thread library error: the tid not fit the MAX_THREAD_NUM"<< std::endl;
        return -1;
    }
    if(running_thread == tid){
        if(sigsetjmp(threads_map[tid]._sigjmp_buf, 1) == 0){
            total_quantoms++;
            blocked_threads.insert(running_thread);
            threads_map[running_thread].state = BLOCKED;
            threads_map[running_thread].blocked_quantums_count = quantums;
            running_thread = ready_threads.front();
            threads_map[running_thread].state = RUNNING;
            threads_map[running_thread].quantums_count++;
            ready_threads.pop();
            siglongjmp(threads_map[running_thread]._sigjmp_buf, 1);
            return 0;
        }
        else{
            std::cerr << "can't switch threads" << std::endl;
            return -1;
        }
    }
    if(threads_map[tid].sp == NULL){
        std::cerr << "thread library error: thread not active" << std::endl;
        return -1;
    }
    if(threads_map[tid].state == READY){
        threads_map[tid].state = BLOCKED;
        threads_map[tid].blocked_quantums_count = quantums;
        filter_queue(ready_threads,tid);
        blocked_threads.insert(tid);
    }
    return 0;
}


int uthread_block(int tid){
    return _uthread_block(tid,-1);
}

int uthread_resume(int tid){
    if(tid == 0){
        std::cerr << "thread library error: can't block the main thread" << std::endl;
        return -1;
    }
    if(tid >= MAX_THREAD_NUM){
        std::cerr << "thread library error: the tid not fit the MAX_THREAD_NUM " << tid << std::endl;
        return -1;
    }
    if(threads_map[tid].sp == NULL){
        std::cerr << "thread library error: thread not active " << tid << std::endl;
        return -1;
    }
    if(threads_map[tid].state == BLOCKED && threads_map[tid].blocked_quantums_count <= 0){
        threads_map[tid].state == READY;
        ready_threads.push(tid);
        blocked_threads.erase(tid);
    }
    return 0;
}

int uthread_sleep(int num_quantums){
    if(running_thread == 0){
        std::cerr << "thread library error: main thread cant call sleep" << std::endl;
        return -1;
    }
    return _uthread_block(running_thread,num_quantums);
}

int uthread_get_tid(){
    return running_thread;
}

int uthread_get_total_quantums(){
    return total_quantoms;
}

int uthread_get_quantums(int tid){
    if(tid >= MAX_THREAD_NUM){
        std::cerr << "thread library error: the tid not fit the MAX_THREAD_NUM" << std::endl;
        return -1;
    }
    if(threads_map[tid].sp == NULL && tid != 0){
        std::cerr << "thread library error: thread not active" << std::endl;
        return -1;
    }
    return threads_map[tid].quantums_count;
}

int main(){
    uthread_init(10000);
    return EXIT_SUCCESS;
}






