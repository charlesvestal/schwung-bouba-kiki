#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include "bouba_kiki_plugin.c"
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static void *live[8];
static atomic_int ready;
static void *worker(void *arg) {
    int id=(int)(intptr_t)arg;
    atomic_fetch_add(&ready,1);
    while(atomic_load(&ready)<8) {}
    for(int n=0;n<20000;n++) {
        void *p=create_instance(".",NULL); assert(p);
        pthread_mutex_lock(&lock);
        for(int j=0;j<8;j++) assert(live[j]!=p);
        live[id]=p;
        pthread_mutex_unlock(&lock);
        pthread_mutex_lock(&lock);
        live[id]=NULL;
        destroy_instance(p);
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}
int main(void) {
    pthread_t threads[8];
    for(int i=0;i<8;i++) assert(!pthread_create(&threads[i],NULL,worker,(void *)(intptr_t)i));
    for(int i=0;i<8;i++) pthread_join(threads[i],NULL);
    puts("PASS: concurrent instance ownership");
}
