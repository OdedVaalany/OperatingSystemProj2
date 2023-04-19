#include "uthreads.h"




static std::unordered_map<int,TCB> threads_map = std::unordered_map<int,TCB>(MAX_THREAD_NUM);
static std::priority_queue<int,std::vector<int>,std::greater<int>> empty_ids;
static std::queue<int> ready_threads;
static std::unordered_set<int> blocked_threads = std::unordered_set<int>();
static int running_thread;
static struct itimerval timer,remaining_time,zero_time ={0};
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

void block_signal(){
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &mask, NULL);
}

void unblock_signal(){
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGVTALRM);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}


void pause_timer(){
    if(getitimer(ITIMER_VIRTUAL,&remaining_time)){
    if(setitimer(ITIMER_VIRTUAL,&zero_time,NULL)){
        std::cout << "set timer error";
    }
    }
}

void resume_timer(){
    if(setitimer(ITIMER_VIRTUAL,&remaining_time,NULL)){
        std::cout << "set timer error";
    }
}

void reset_timer(){
    if(setitimer(ITIMER_VIRTUAL,&timer,NULL)){
        std::cout << "set timer error";
    }
}

void print_states(){
    std::cout << "================\n"; 
    for(int i=0 ;i<3 ; i++){
        std::cout << "thread number\t" << i << "\nthread state\t" << threads_map[i].state << "\nthread blocked quantums count\t" << threads_map[i].blocked_quantums_count << std::endl;
    }
}

void replace_current_thread(ThreadState state){
    if(state != RUNNING){
        if(sigsetjmp(threads_map[running_thread]._sigjmp_buf, 1) == 0){
        total_quantoms++;
        if(state == READY){
            ready_threads.push(running_thread);
            threads_map[running_thread].state = READY;
        }
        else if(state == BLOCKED){
            blocked_threads.insert(running_thread);
            threads_map[running_thread].state = BLOCKED;
        }
        running_thread = ready_threads.front();
        ready_threads.pop();
        threads_map[running_thread].state = RUNNING;
        threads_map[running_thread].quantums_count++;

        for(int tid : blocked_threads){
            if(tid > -1 && tid < MAX_THREAD_NUM && threads_map[tid].blocked_quantums_count > -1){
                if(threads_map[tid].blocked_quantums_count == 0){
                    uthread_resume(tid);
                }
                else{
                    threads_map[tid].blocked_quantums_count--;
                }
            }
        }

        //std::cout << "current running thread number " << running_thread << std::endl;
        //print_states();
        reset_timer();
        unblock_signal();
        siglongjmp(threads_map[running_thread]._sigjmp_buf, 1);
        }
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
    block_signal();
    replace_current_thread(READY);
}

void busy_main(){
    while(true){
        //std::cout << "hello from main" << std::endl;  
    }
}

void func1(){
    while(true){
        std::cout << "number of thread: " << 1 << std::endl;
        std::cout << "number of quantums: " << threads_map[1].quantums_count << std::endl;
        std::cout << "total quantom: " << total_quantoms << std::endl;
        uthread_sleep(10);
    }
}

void func2(){
    while(true){
        std::cout << "number of thread: " << 2 << std::endl;
        std::cout << "number of quantums: " << threads_map[2].quantums_count << std::endl;
        std::cout << "total quantom: " << total_quantoms << std::endl;
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
    // timer.it_interval.tv_sec = 0;
    // timer.it_interval.tv_usec = quantum_usecs;


    if(setitimer(ITIMER_VIRTUAL,&timer,NULL)){
        std::cout << "set timer error";
    }

    sigsetjmp(threads_map[0]._sigjmp_buf, 1);
    threads_map[0]._sigjmp_buf->__jmpbuf[JB_PC] = translate_address((address_t) &busy_main);
    siglongjmp(threads_map[0]._sigjmp_buf,1);
    return 0;
}

int uthread_spawn(thread_entry_point entry_point){
    block_signal();
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
    unblock_signal();
    return 0;
}

int uthread_terminate(int tid){
    block_signal();
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
        unblock_signal();
        return -1;
    }
    if(threads_map[tid].sp == NULL){
        std::cerr << "thread library error: thread not active" << std::endl;
        unblock_signal();
        return -1;
    }
    free(threads_map[tid].sp);
    threads_map[tid].sp = NULL;
    empty_ids.push(tid);
    if(threads_map[tid].state == RUNNING){
        threads_map[tid].state = DELETED;
        replace_current_thread(DELETED);
    }
    else if(threads_map[tid].state == READY){
        threads_map[tid].state = DELETED;
        filter_queue(ready_threads,tid);
    }
    else if(threads_map[tid].state == BLOCKED){
        threads_map[tid].state = DELETED;
        blocked_threads.erase(tid);
    }
    unblock_signal();
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
    if(threads_map[tid].sp == NULL){
        std::cerr << "thread library error: thread not active" << std::endl;
        return -1;
    }
    threads_map[tid].blocked_quantums_count = quantums;
    if(running_thread == tid){
        replace_current_thread(BLOCKED);
        return 0;
        
    }
    if(threads_map[tid].state == READY){
        threads_map[tid].state = BLOCKED;
        filter_queue(ready_threads,tid);
        blocked_threads.insert(tid);
    }
    return 0;
}


int uthread_block(int tid){
    block_signal();
    int ret_val = _uthread_block(tid,-1);
    unblock_signal();
    return ret_val;
}

int uthread_resume(int tid){
    block_signal();
    if(tid == 0){
        std::cerr << "thread library error: can't block the main thread" << std::endl;
        unblock_signal();
        return -1;
    }
    if(tid >= MAX_THREAD_NUM || tid < -1){
        std::cerr << "thread library error: the tid not fit the MAX_THREAD_NUM" << tid << std::endl;
        unblock_signal();
        return -1;
    }
    if(threads_map[tid].sp == NULL){
        std::cerr << "thread library error: thread not active " << tid << std::endl;
        unblock_signal();
        return -1;
    }
    if(threads_map[tid].state == BLOCKED && threads_map[tid].blocked_quantums_count <= 0){
        threads_map[tid].state = READY;
        ready_threads.push(tid);
        blocked_threads.erase(tid);
    }
    unblock_signal();
    return 0;
}

int uthread_sleep(int num_quantums){
    block_signal();
    if(running_thread == 0){
        std::cerr << "thread library error: main thread cant call sleep" << std::endl;
        unblock_signal();
        return -1;
    }
    int ret_val = _uthread_block(running_thread,num_quantums);
    unblock_signal();
    return ret_val;
}

int uthread_get_tid(){
    return running_thread;
}

int uthread_get_total_quantums(){
    block_signal();
    int ret_val = total_quantoms;
    unblock_signal();
    return ret_val;
}

int uthread_get_quantums(int tid){
    block_signal();
    if(tid >= MAX_THREAD_NUM){
        std::cerr << "thread library error: the tid not fit the MAX_THREAD_NUM" << std::endl;
        unblock_signal();
        return -1;
    }
    if(threads_map[tid].sp == NULL && tid != 0){
        std::cerr << "thread library error: thread not active" << std::endl;
        unblock_signal();
        return -1;
    }
    int ret_val = threads_map[tid].quantums_count;
    unblock_signal();
    return ret_val;
}

int main(){
    uthread_init(10000);
    return EXIT_SUCCESS;
}






