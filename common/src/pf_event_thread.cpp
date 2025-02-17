#include <sys/prctl.h>
#include <string.h>

#include "pf_event_thread.h"
#include "pf_app_ctx.h"

void* thread_proc_spdkr(void* arg);
void* thread_proc_eventq(void* arg);

PfEventThread::PfEventThread() {
	inited = false;
}
int PfEventThread::init(const char* name, int qd)
{
	int rc;
	strncpy(this->name, name, sizeof(this->name));

	if (spdk_engine_used()) {
		event_queue = new PfSpdkQueue();
		rc = ((PfSpdkQueue *)event_queue)->init(name, qd, SPDK_RING_TYPE_MP_SC);
		thread_proc = thread_proc_spdkr;
	} else {
		event_queue = new PfEventQueue();
		rc = ((PfEventQueue *)event_queue)->init(name, qd, 0);
		thread_proc = thread_proc_eventq;
	}

	if(rc) {
		S5LOG_ERROR("Failed to init PfEventThread, rc:%d", rc);
		return rc;
	}

	inited = true;
	return 0;
}
void PfEventThread::destroy()
{
	if(inited) {
		if(tid)
			stop();
		event_queue->destroy();
		inited = false;
	}
}
PfEventThread::~PfEventThread()
{
	destroy();
}

struct pt_thread_context {
    bool spdk_engine;
    void *(*f)(void *);
    void *arg;
    pthread_attr_t *a;
    pthread_t *t;
    bool affinitize;
    uint32_t core;
    int ret;
	pthread_t *tid;
};

void* pt_thread_run(void *_arg)
{
    struct pt_thread_context *arg = (struct pt_thread_context *)_arg;
    int rc;

    rc = pthread_create(arg->tid, NULL, arg->f, arg->arg);	if(rc)
	{
		S5LOG_FATAL("Failed create pthread, rc:%d", rc);
	}
	
	return NULL;
}

int PfEventThread::start()
{
	/*
	struct pt_thread_context pt;
	pt.f = thread_proc;
	pt.arg = this;
	pt.tid = &tid;
	spdk_call_unaffinitized(pt_thread_run, &pt);
	*/
	int rc = pthread_create(&tid, NULL, thread_proc, this);
	if(rc)
	{
		S5LOG_ERROR("Failed create thread:%s, rc:%d", name, rc);
		return rc;
	}
	struct sched_param sp;
	memset(&sp, 0, sizeof(sp));
	sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_setschedparam(tid, SCHED_FIFO, &sp);
	return 0;
}
void PfEventThread::stop()
{
	exiting=true;
	event_queue->post_event(EVT_THREAD_EXIT, 0, NULL);
	int rc = pthread_join(tid, NULL);
	if(rc) {
		S5LOG_ERROR("Failed call pthread_join on thread:%s, rc:%d", name, rc);
	}
	tid=0;

}

void * thread_proc_eventq(void* arg)
{
	PfEventThread* pThis = (PfEventThread*)arg;
	prctl(PR_SET_NAME, pThis->name);
	PfFixedSizeQueue<S5Event>* q;
	PfEventQueue *eq = (PfEventQueue *)pThis->event_queue;
	int rc = 0;
	while ((rc = eq->get_events(&q)) == 0)
	{
		while(!q->is_empty())
		{
			S5Event* t = &q->data[q->head];
			q->head = (q->head + 1) % q->queue_depth;
			switch(t->type){
				case EVT_SYNC_INVOKE:
				{
					SyncInvokeArg* arg = (SyncInvokeArg*)t->arg_p;
					arg->rc = arg->func();
					sem_post(&arg->sem);
					break;
				}
				case EVT_THREAD_EXIT:
				{
					S5LOG_INFO("exit thread:%s", pThis->name);
					return NULL;
				}
				default:
					pThis->process_event(t->type, t->arg_i, t->arg_p, t->arg_q);
			}
		}
		pThis->commit_batch();
	}
	return NULL;
}

#define BATH_PROCESS 8
void* thread_proc_spdkr(void* arg)
{
	PfEventThread* pThis = (PfEventThread*)arg;
	prctl(PR_SET_NAME, pThis->name);
	int rc = 0, i = 0;
	void *events[BATH_PROCESS];
	PfSpdkQueue *eq = (PfSpdkQueue *)pThis->event_queue;
	eq->set_thread_queue();

	while((rc = eq->get_events(BATH_PROCESS, events)) >= 0) {
		for (i = 0; i < rc; i++) {
			struct pf_spdk_msg *event = (struct pf_spdk_msg *)events[i];
			switch (event->event.type)
			{
				case EVT_SYNC_INVOKE:
				{
					SyncInvokeArg* arg = (SyncInvokeArg*)event->event.arg_p;
					arg->rc = arg->func();
					sem_post(&arg->sem);
					break;
				}
				case EVT_THREAD_EXIT:
				{
					S5LOG_INFO("exit thread:%s", pThis->name);
					return NULL;
				}
				default:
				{	
					pThis->process_event(event->event.type, event->event.arg_i, event->event.arg_p, event->event.arg_q);
					break;
				}
			}
			eq->put_event(events[i]);
		}
		if (pThis->func_priv) {
			int com = 0;
			pThis->func_priv(&com, pThis->arg_v);
		}
	}

	return NULL;
}

int PfEventThread::sync_invoke(std::function<int(void)> _f)
{
	SyncInvokeArg arg;
	sem_init(&arg.sem, 0, 0);
	arg.func = std::move(_f);
	this->event_queue->post_event(EVT_SYNC_INVOKE, 0, &arg);
	sem_wait(&arg.sem);
	return arg.rc;
}

