#include "uthreads.h";
#include <stdlib.h>;
#include <stdio.h>;
#include <queue>;
#include <sys/time.h>

enum ThreadState {RUNNING,BLOCKED,READY};

struct ThreadCard
{
    int id;
    ThreadState state;
    int quantum;
    char* sp;
    size_t pc;
    thread_entry_point entry_point;
};


static int default_quantum_usecs;
static int total_quantums = 0;
static ThreadCard* running_thread;
static std::queue<ThreadCard*> ready_threads;
static std::queue<ThreadCard*> blocked_threads;
static std::priority_queue<int,std::vector<int>,std::greater<int>> empty_ids;
static std::unordered_map<int,ThreadCard*> threads;

void remove_from_ready_queue(int tid){
    ThreadCard* temp;
    for(int i = 0; i < ready_threads.size();i++){
        temp = ready_threads.front();
        ready_threads.pop();
        if(temp->id != tid){
            ready_threads.push(temp);
        }
    }
}

void remove_from_blocked_queue(int tid){
    ThreadCard* temp;
    for(int i = 0; i < blocked_threads.size();i++){
        temp = blocked_threads.front();
        blocked_threads.pop();
        if(temp->id != tid){
            blocked_threads.push(temp);
        }
    }
}

int uthread_init(int quantum_usecs){
    if(quantum_usecs <= 0){
        return -1;
    }
    default_quantum_usecs = quantum_usecs;
    
    //Init the empty ids min heap
    for(int i=1;i <= MAX_THREAD_NUM ; i++){
        empty_ids.push(i);
    }

    //init the main thread
    ThreadCard* main = (ThreadCard*) malloc(sizeof(ThreadCard));
    if(main == NULL) return -1;
    main->id = 0;
    main->state = RUNNING;
    main->pc = 0;
    main->quantum=0;
    running_thread = main;
    threads[0] = main;
    
    return 0;

}

int uthread_spawn(thread_entry_point entry_point){
    //check if the entry_point isn't null
    if(entry_point == NULL){
        return -1;
    }
    //checks if there is no more place to another thread
    if(empty_ids.empty()){
        return -1;
    }
    int id = empty_ids.top();

    //alloctating a new thread card
    ThreadCard* temp = (ThreadCard*) malloc(sizeof(ThreadCard));
    if(temp == NULL){
        return -1;
    }
    temp->pc = 0;
    temp->entry_point = entry_point;
    temp->quantum = 0;
    temp->id = id;
    temp->state = READY;
    temp->sp = (char*) malloc(STACK_SIZE);
    if(temp->sp == NULL){
        free(temp);
        return -1;
    }

    ready_threads.push(temp);
    threads[id]=temp;
    empty_ids.pop();
    return id;

}

/**
 * @brief handle when terminating the main thread
 */
void main_thread_terminate(){
    for(std::pair<int,ThreadCard*> thread : threads){
        if(thread.second->sp != NULL){
            free(thread.second->sp);
        }
        free(thread.second);
    }
    exit(0);
}

int uthread_terminate(int tid){
    if(tid == 0){
        main_thread_terminate();
    }
    if(threads[tid] == NULL){
        return -1;
    }
    if(tid == running_thread->id){
        //TODO: what to do   
    }
    if(threads[tid]->sp != NULL){
        free(threads[tid]->sp);
    }
    free(threads[tid]);
    threads[tid] = NULL;
    empty_ids.push(tid);
    return 0;
}

int uthread_block(int tid){
    if(threads[tid] == NULL || tid == 0) return -1;
    if(threads[tid]->state != BLOCKED){
        if(threads[tid]->state == READY){
            remove_from_ready_queue(tid);
        }
        else{
            //#TODO check it
            running_thread=NULL;

        }
        threads[tid]->state = BLOCKED;
        blocked_threads.push(threads[tid]);
    }
    return 0;
}

int uthread_resume(int tid){
    if(threads[tid] == NULL) return -1;
    if(threads[tid]->state == BLOCKED){
        remove_from_blocked_queue(tid);
        ready_threads.push(threads[tid]);
        threads[tid]->state = READY;
    }
    return 0;
}

int uthread_get_tid(){
    return running_thread->id;
}

int uthread_get_total_quantums(){
    return total_quantums;
}

int uthread_get_quantums(int tid){
    return threads[tid]->quantum;
}

