//
// CPU Schedule Simulator Homework
// Student Number : B511180
// Name : Jeong Yeonho
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>
#define DEBUG
#define SEED 10
// process states
#define S_IDLE 0			
#define S_READY 1
#define S_BLOCKED 2
#define S_RUNNING 3
#define S_TERMINATE 4

int NPROC, NIOREQ, QUANTUM;

struct ioDoneEvent {
	int procid;
	int doneTime;
	int len;
	struct ioDoneEvent* prev;
	struct ioDoneEvent* next;
} ioDoneEventQueue, * ioDoneEvent;

struct process {
	int id;
	int len;		// for queue
	int targetServiceTime;
	int serviceTime;
	int startTime;
	int endTime;
	char state;
	int priority;
	int saveReg0, saveReg1;
	struct process* prev;
	struct process* next;
} *procTable;

struct process idleProc;
struct process readyQueue;
struct process blockedQueue;
struct process* runningProc;

int cpuReg0, cpuReg1;
int currentTime = 0;
int* procIntArrTime, * procServTime, * ioReqIntArrTime, * ioServTime;

void compute() {
	// DO NOT CHANGE THIS FUNCTION
	cpuReg0 = cpuReg0 + runningProc->id;
	cpuReg1 = cpuReg1 + runningProc->id;
	//printf("In computer proc %d cpuReg0 %d\n",runningProc->id,cpuReg0);
}

void initProcTable() {
	int i;
	for (i = 0; i < NPROC; i++) {
		procTable[i].id = i;
		procTable[i].len = 0;
		procTable[i].targetServiceTime = procServTime[i];
		procTable[i].serviceTime = 0;
		procTable[i].startTime = 0;
		procTable[i].endTime = 0;
		procTable[i].state = S_IDLE;
		procTable[i].priority = 0;
		procTable[i].saveReg0 = 0;
		procTable[i].saveReg1 = 0;
		procTable[i].prev = NULL;
		procTable[i].next = NULL;
	}
}

void readyPush(struct process* goReady) {
	struct process* Ready = &readyQueue;
	if (Ready == Ready->next) {
		Ready->next = goReady;
		goReady->next = NULL;
		goReady->prev = Ready;
	}
	else {
		goReady->next = Ready->next;
		Ready->next->prev = goReady;
		goReady->prev = Ready;
		Ready->next = goReady;
	}
	goReady->state = S_READY;
}

void blockPush(struct process* goBlock, int cnt) {
	struct process* block = &blockedQueue; int i;
	if (block->next == block) {
		block->next = goBlock;
		goBlock->next = NULL;
		goBlock->prev = block;
	}
	else {
		for (i = 0; i < cnt; i++)
			block = block->next;
		if (block->next == NULL) {
			block->next = goBlock;
			goBlock->next = NULL;
			goBlock->prev = block;
		}
		else {
			goBlock->next = block->next;
			goBlock->prev = block;
			block->next = goBlock;
			goBlock->next->prev = goBlock;
		}

	}
	goBlock->state = S_BLOCKED;
}
void qprint(struct process* had){
	struct process* head = had;
	if(head->next ==head) printf("not\n");
	else{ 
		while(head->next != NULL){
			printf("%d %d -> ",head->next->id,head->next->priority);
			head = head->next;
			}
	printf("\n");
	}
}
void procExecSim(struct process* (*scheduler)()) {
	int pid, qTime = 0, cpuUseTime = 0, nproc = 0, termProc = 0, nioreq = 0; int iocome, isterm;
	char schedule = 0, nextState = S_IDLE;
	int nextForkTime, nextIOReqTime;
	nextForkTime = procIntArrTime[nproc];
	nextIOReqTime = ioReqIntArrTime[nioreq];
	runningProc = &idleProc;
	while (1) {
		currentTime++;
		qTime++;
		runningProc->serviceTime++;
		if (runningProc != &idleProc) {
			cpuUseTime++;
		}
		// MUST CALL compute() Inside While loop
		compute();

		if (runningProc->serviceTime == runningProc->targetServiceTime) { /* CASE 4 : the process job done and terminates */
			if (runningProc->state == S_RUNNING) {
				runningProc->endTime = currentTime;
				runningProc->saveReg0 = cpuReg0;
				runningProc->saveReg1 = cpuReg1;
				runningProc->state = S_TERMINATE;
				printf("FIN %d %d %d\n", runningProc->id, runningProc->state, runningProc->endTime);
				termProc++;
				if (termProc == NPROC) break;
			}
		}

		if (currentTime == nextForkTime && nproc < NPROC) { /* CASE 2 : a new process created */
			readyPush(&procTable[nproc]);
			procTable[nproc].startTime = currentTime;
			printf("New %d %d %d\n", procTable[nproc].id, procTable[nproc].targetServiceTime, currentTime);
			nextForkTime += procIntArrTime[++nproc];
			if (runningProc != &idleProc && runningProc->state == S_RUNNING) {
				readyPush(runningProc);
				printf("Ready %d %d %d\n", runningProc->id, runningProc->state, currentTime);
				runningProc->saveReg0 = cpuReg0;
				runningProc->saveReg1 = cpuReg1;
			}
		}

		if (cpuUseTime == nextIOReqTime) { /* CASE 5: reqest IO operations (only when the process does not terminate) */
			if (runningProc != &idleProc) {
				if (runningProc->state == S_TERMINATE) isterm = 1;
				ioDoneEvent[nioreq].procid = runningProc->id;
				ioDoneEvent[nioreq].doneTime = currentTime + ioServTime[nioreq];
				struct ioDoneEvent* head = &ioDoneEventQueue;
				int cnt = 0;
				if (head == head->next) {
					ioDoneEvent[nioreq].next = NULL;
					head->next = &ioDoneEvent[nioreq];
					ioDoneEvent[nioreq].prev = head;
				}
				else {
					while (1) {
						if (head->next->doneTime <= ioDoneEvent[nioreq].doneTime) {
							head = head->next; cnt++;
						}
						else {
							ioDoneEvent[nioreq].next = head->next;
							ioDoneEvent[nioreq].prev = head;
							head->next->prev = &ioDoneEvent[nioreq];
							head->next = &ioDoneEvent[nioreq];
							break;
						}

						if (head->next == NULL) {
							head->next = &ioDoneEvent[nioreq];
							ioDoneEvent[nioreq].prev = head;
							ioDoneEvent[nioreq].next = NULL;
							break;
						}
					}
				}
					if (runningProc->state == S_READY) {
						struct process* head = &readyQueue;
						if (head->next->next == NULL){
							head->next = head;
						}
						else {
							struct process* tmp = head->next;
							head->next = tmp->next;
							head->next->prev = head;
							tmp->next = NULL;
							tmp->prev = NULL;
						}
					}
					blockPush(runningProc, cnt); cnt = 0;
					printf("Block %d %d %d\n", runningProc->id, currentTime, ioDoneEvent[nioreq].doneTime);
					runningProc->saveReg0 = cpuReg0;
					runningProc->saveReg1 = cpuReg1;
					runningProc->priority++;
					if (isterm == 1) {
						runningProc->state = S_TERMINATE;
						isterm = 0;
					}
				}
			if (nioreq + 1 < NIOREQ)
				nextIOReqTime += ioReqIntArrTime[++nioreq];
			else
				nextIOReqTime = INT_MAX;
		}
		while (ioDoneEventQueue.next->doneTime == currentTime) { /* CASE 3 : IO Done Event */
			struct ioDoneEvent* come = &ioDoneEventQueue; iocome = 1;
			struct process* bq = &blockedQueue;
			if (runningProc->state == S_READY) {
				struct process* rq = readyQueue.next;
				readyQueue.next = rq->next;
				rq->next->prev = &readyQueue;
				runningProc->state = S_RUNNING;
			}
				
			if (bq->next->next == NULL) {
				if (bq->next->state != S_TERMINATE) {
					readyPush(bq->next);
					printf("B To R %d %d \n", bq->next->id, currentTime);
				}
				bq->next = bq;
			}
			else {
				struct process* tmp = bq->next;
				bq->next = tmp->next;
				tmp->next->prev = bq;
				if (tmp->state != S_TERMINATE) {
					
					readyPush(tmp);
					printf("B To R %d %d\n", tmp->id, currentTime);
				}
			}

			if (come->next->next == NULL) {
				come->next = come;
			}
			else {
				come->next = come->next->next;
				come->next->prev = come;
			}
		}
		if (iocome == 1) {
			if (runningProc->state == S_RUNNING) {
				readyPush(runningProc);
				runningProc->saveReg0 = cpuReg0;
				runningProc->saveReg1 = cpuReg1;
				printf("Ready %d %d\n", runningProc->id, currentTime);
			}
			iocome = 0;
		}

		if (qTime == QUANTUM) { /* CASE 1 : The quantum expires */
			if (runningProc != &idleProc && runningProc->state == S_RUNNING) {
				readyPush(runningProc);
				printf("Ready %d %d %d\n", runningProc->id, runningProc->state, currentTime);
				runningProc->saveReg0 = cpuReg0;
				runningProc->saveReg1 = cpuReg1;
				runningProc->priority--;
				runningProc = scheduler();
			}
			if (runningProc->state == S_BLOCKED)
				runningProc->priority -= 2;
			else if (runningProc->state == S_READY)
				runningProc->priority--;
			qTime = 0;
		}

		if (runningProc->state != S_RUNNING) {
			runningProc = scheduler();
			qTime = 0;
		}
		// call scheduler() if needed

	} // while loop
}

//RR,SJF(Modified),SRTN,Guaranteed Scheduling(modified),Simple Feed Back Scheduling
struct process* RRschedule() {
	struct process* first = &readyQueue;
	while (first->next != NULL && first->next != first) {
		first = first->next;
	}
	if (first->prev != readyQueue.prev) {
		first->prev->next = NULL;
		first->state = S_RUNNING;
		cpuReg0 = first->saveReg0;
		cpuReg1 = first->saveReg1;
		printf("scheduler %d\n", first->id);
		first->next = NULL;
		first->prev = NULL;
		return first;
	}
	else if (readyQueue.prev != readyQueue.next) {
		readyQueue.next = &readyQueue;
		first->state = S_RUNNING;
		cpuReg0 = first->saveReg0;
		cpuReg1 = first->saveReg1;
		printf("scheduler %d\n", first->id);
		first->next = NULL;
		first->prev = NULL;
		return first;
	}
	else {
		return &idleProc;
	}
}
struct process* SJFschedule() {
	struct process* rq = &readyQueue;
	if(rq->next == rq) return &idleProc;
	rq = rq->next;	
	struct process* shortestProc = rq;
	int shortestTime = rq->targetServiceTime;
	while (rq->next != NULL) {
		if (shortestTime >= rq->next->targetServiceTime) {
			shortestProc = rq->next;
			shortestTime = rq->next->targetServiceTime;
		}
		rq = rq->next;
	}

	if (shortestProc->next == NULL){
		if (shortestProc->prev == &readyQueue) 
			readyQueue.next = &readyQueue;
		else
			shortestProc->prev->next = NULL;
	}
	else {
		struct process* tmp = shortestProc->prev;
		tmp->next = shortestProc->next;
		shortestProc->next->prev = tmp;
	}
	shortestProc->state = S_RUNNING;
	cpuReg0 = shortestProc->saveReg0;
	cpuReg1 = shortestProc->saveReg1;
	shortestProc->next = NULL;
	shortestProc->prev = NULL;
printf("scheduler %d\n",shortestProc->id);
	return shortestProc;
}

struct process* SRTNschedule() {
	struct process* rq = &readyQueue;
	if (rq->next == rq) return &idleProc;
	rq = rq->next;
	struct process* shortestProc = rq;
	int shortestTime = rq->targetServiceTime - rq->serviceTime;
	while (rq->next != NULL) {
		if (shortestTime >= rq->next->targetServiceTime - rq->next->serviceTime) {
			shortestProc = rq->next;
			shortestTime = rq->next->targetServiceTime - rq->next->serviceTime;
		}
		rq = rq->next;
	}

	if (shortestProc->next == NULL) {
		if (shortestProc->prev == &readyQueue)
			readyQueue.next = &readyQueue;
		else
			shortestProc->prev->next = NULL;
	}
	else {
		struct process* tmp = shortestProc->prev;
		tmp->next = shortestProc->next;
		shortestProc->next->prev = tmp;
	}
	shortestProc->state = S_RUNNING;
	cpuReg0 = shortestProc->saveReg0;
	cpuReg1 = shortestProc->saveReg1;
	shortestProc->next = NULL;
	shortestProc->prev = NULL;
	printf("scheduler %d\n", shortestProc->id);
	return shortestProc;

}

struct process* GSschedule() {
	struct process* rq = &readyQueue;
	if (rq->next == rq) return &idleProc;
	rq = rq->next;
	struct process* shortestProc = rq;
	double shortestTime = (double)rq->serviceTime / (double)rq->targetServiceTime;
	while (rq->next != NULL) {
		double temp = (double)rq->next->serviceTime / (double)rq->next->targetServiceTime;
		if (shortestTime >= temp) {
			shortestProc = rq->next;
			shortestTime = temp; 
		}
		rq = rq->next;
	}

	if (shortestProc->next == NULL) {
		if (shortestProc->prev == &readyQueue)
			readyQueue.next = &readyQueue;
		else
			shortestProc->prev->next = NULL;
	}
	else {
		struct process* tmp = shortestProc->prev;
		tmp->next = shortestProc->next;
		shortestProc->next->prev = tmp;
	}
	shortestProc->state = S_RUNNING;
	cpuReg0 = shortestProc->saveReg0;
	cpuReg1 = shortestProc->saveReg1;
	shortestProc->next = NULL;
	shortestProc->prev = NULL;
	printf("scheduler %d\n", shortestProc->id);
	return shortestProc;
}

struct process* SFSschedule() {
	struct process* rq = &readyQueue;
	if (rq->next == rq) return &idleProc;
	rq = rq->next;
	struct process* shortestProc = rq;
	int high = rq->priority;
	while (rq->next != NULL) {		
		if (high <= rq->next->priority) {
			shortestProc = rq->next;
			high = rq->next->priority;
		}
		rq = rq->next;
	}

	if (shortestProc->next == NULL) {
		if (shortestProc->prev == &readyQueue)
			readyQueue.next = &readyQueue;
		else
			shortestProc->prev->next = NULL;
	}
	else {
		struct process* tmp = shortestProc->prev;
		tmp->next = shortestProc->next;
		shortestProc->next->prev = tmp;
	}
	shortestProc->state = S_RUNNING;
	cpuReg0 = shortestProc->saveReg0;
	cpuReg1 = shortestProc->saveReg1;
	shortestProc->next = NULL;
	shortestProc->prev = NULL;
	printf("scheduler %d\n", shortestProc->id);
	return shortestProc;
}

void printResult() {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	long totalProcIntArrTime = 0, totalProcServTime = 0, totalIOReqIntArrTime = 0, totalIOServTime = 0;
	long totalWallTime = 0, totalRegValue = 0;
	for (i = 0; i < NPROC; i++) {
		totalWallTime += procTable[i].endTime - procTable[i].startTime;
		/*
		printf("proc %d serviceTime %d targetServiceTime %d saveReg0 %d\n",
			i,procTable[i].serviceTime,procTable[i].targetServiceTime, procTable[i].saveReg0);
		*/
		totalRegValue += procTable[i].saveReg0 + procTable[i].saveReg1;
		/* printf("reg0 %d reg1 %d totalRegValue %d\n",procTable[i].saveReg0,procTable[i].saveReg1,totalRegValue);*/
	}
	for (i = 0; i < NPROC; i++) {
		totalProcIntArrTime += procIntArrTime[i];
		totalProcServTime += procServTime[i];
	}
	for (i = 0; i < NIOREQ; i++) {
		totalIOReqIntArrTime += ioReqIntArrTime[i];
		totalIOServTime += ioServTime[i];
	}

	printf("Avg Proc Inter Arrival Time : %g \tAverage Proc Service Time : %g\n", (float)totalProcIntArrTime / NPROC, (float)totalProcServTime / NPROC);
	printf("Avg IOReq Inter Arrival Time : %g \tAverage IOReq Service Time : %g\n", (float)totalIOReqIntArrTime / NIOREQ, (float)totalIOServTime / NIOREQ);
	printf("%d Process processed with %d IO requests\n", NPROC, NIOREQ);
	printf("Average Wall Clock Service Time : %g \tAverage Two Register Sum Value %g\n", (float)totalWallTime / NPROC, (float)totalRegValue / NPROC);

}

int main(int argc, char* argv[]) {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	int totalProcServTime = 0, ioReqAvgIntArrTime;
	int SCHEDULING_METHOD, MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME, MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME;

	if (argc < 12) {
		printf("%s: SCHEDULING_METHOD NPROC NIOREQ QUANTUM MIN_INT_ARRTIME MAX_INT_ARRTIME MIN_SERVTIME MAX_SERVTIME MIN_IO_SERVTIME MAX_IO_SERVTIME MIN_IOREQ_INT_ARRTIME\n", argv[0]);
		exit(1);
	}

	SCHEDULING_METHOD = atoi(argv[1]);
	NPROC = atoi(argv[2]);
	NIOREQ = atoi(argv[3]);
	QUANTUM = atoi(argv[4]);
	MIN_INT_ARRTIME = atoi(argv[5]);
	MAX_INT_ARRTIME = atoi(argv[6]);
	MIN_SERVTIME = atoi(argv[7]);
	MAX_SERVTIME = atoi(argv[8]);
	MIN_IO_SERVTIME = atoi(argv[9]);
	MAX_IO_SERVTIME = atoi(argv[10]);
	MIN_IOREQ_INT_ARRTIME = atoi(argv[11]);

	printf("SIMULATION PARAMETERS : SCHEDULING_METHOD %d NPROC %d NIOREQ %d QUANTUM %d \n", SCHEDULING_METHOD, NPROC, NIOREQ, QUANTUM);
	printf("MIN_INT_ARRTIME %d MAX_INT_ARRTIME %d MIN_SERVTIME %d MAX_SERVTIME %d\n", MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME);
	printf("MIN_IO_SERVTIME %d MAX_IO_SERVTIME %d MIN_IOREQ_INT_ARRTIME %d\n", MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME);

	srandom(SEED);

	// allocate array structures
	procTable = (struct process*)malloc(sizeof(struct process) * NPROC);
	ioDoneEvent = (struct ioDoneEvent*)malloc(sizeof(struct ioDoneEvent) * NIOREQ);
	procIntArrTime = (int*)malloc(sizeof(int) * NPROC);
	procServTime = (int*)malloc(sizeof(int) * NPROC);
	ioReqIntArrTime = (int*)malloc(sizeof(int) * NIOREQ);
	ioServTime = (int*)malloc(sizeof(int) * NIOREQ);

	// initialize queues
	readyQueue.next = readyQueue.prev = &readyQueue;

	blockedQueue.next = blockedQueue.prev = &blockedQueue;
	ioDoneEventQueue.next = ioDoneEventQueue.prev = &ioDoneEventQueue;
	ioDoneEventQueue.doneTime = INT_MAX;
	ioDoneEventQueue.procid = -1;
	ioDoneEventQueue.len = readyQueue.len = blockedQueue.len = 0;

	// generate process interarrival times
	for (i = 0; i < NPROC; i++) {
		procIntArrTime[i] = random() % (MAX_INT_ARRTIME - MIN_INT_ARRTIME + 1) + MIN_INT_ARRTIME;
	}
	// assign service time for each process
	for (i = 0; i < NPROC; i++) {
		procServTime[i] = random() % (MAX_SERVTIME - MIN_SERVTIME + 1) + MIN_SERVTIME;
		totalProcServTime += procServTime[i];
	}
	ioReqAvgIntArrTime = totalProcServTime / (NIOREQ + 1);
	assert(ioReqAvgIntArrTime > MIN_IOREQ_INT_ARRTIME);

	// generate io request interarrival time
	for (i = 0; i < NIOREQ; i++) {
		ioReqIntArrTime[i] = random() % ((ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME) * 2 + 1) + MIN_IOREQ_INT_ARRTIME;
	}

	// generate io request service time
	for (i = 0; i < NIOREQ; i++) {
		ioServTime[i] = random() % (MAX_IO_SERVTIME - MIN_IO_SERVTIME + 1) + MIN_IO_SERVTIME;
	}

	initProcTable();
#ifdef DEBUG
	// printing process interarrival time and service time
	printf("Process Interarrival Time :\n");
	for (i = 0; i < NPROC; i++) {
		printf("%d ", procIntArrTime[i]);
	}
	printf("\n");
	printf("Process Target Service Time :\n");
	for (i = 0; i < NPROC; i++) {
		printf("%d ", procTable[i].targetServiceTime);
	}
	printf("\n");
#endif

	// printing io request interarrival time and io request service time
	printf("IO Req Average InterArrival Time %d\n", ioReqAvgIntArrTime);
	printf("IO Req InterArrival Time range : %d ~ %d\n", MIN_IOREQ_INT_ARRTIME,
		(ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME) * 2 + MIN_IOREQ_INT_ARRTIME);

#ifdef DEBUG		
	printf("IO Req Interarrival Time :\n");
	for (i = 0; i < NIOREQ; i++) {
		printf("%d ", ioReqIntArrTime[i]);
	}
	printf("\n");
	printf("IO Req Service Time :\n");
	for (i = 0; i < NIOREQ; i++) {
		printf("%d ", ioServTime[i]);
	}
	printf("\n");
#endif

	struct process* (*schFunc)();
	switch (SCHEDULING_METHOD) {
			case 1: schFunc = RRschedule; break;
			case 2: schFunc = SJFschedule; break;
			case 3: schFunc = SRTNschedule; break;
			case 4: schFunc = GSschedule; break;
			case 5: schFunc = SFSschedule; break;
	default: printf("ERROR : Unknown Scheduling Method\n"); exit(1);
	}
	procExecSim(schFunc);
	printResult();

}
