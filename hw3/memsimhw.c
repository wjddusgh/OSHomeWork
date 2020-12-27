//
// Virual Memory Simulator Homework
// One-level page table system with FIFO and LRU
// Two-level page table system with LRU
// Inverted page table with a hashing system 
//
// Submission Year:2020
// Student Name:정연호
// Student Number:B511180
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#define PAGESIZEBITS 12			// page size = 4Kbytes
#define VIRTUALADDRBITS 32		// virtual address space size = 4Gbytes
int numProcess;

struct pageTableEntry {
	bool valid;
	int count;
	int id;
	int pid;
	struct pageTableEntry* secondLevel;
	struct pageTableEntry* next;
	struct pageTableEntry* prev;
} **pageTable;
struct pageTableEntry* physicalTable;
struct pageTableEntry** second;
struct pageTableEntry* hashNode;

struct procEntry {
	char* traceName;			// the memory trace name
	int pid;					// process (trace) id
	int ntraces;				// the number of memory traces
	int num2ndLevelPageTable;	// The 2nd level page created(allocated);
	int numIHTConflictAccess; 	// The number of Inverted Hash Table Conflict Accesses
	int numIHTNULLAccess;		// The number of Empty Inverted Hash Table Accesses
	int numIHTNonNULLAcess;		// The number of Non Empty Inverted Hash Table Accesses
	int numPageFault;			// The number of page faults
	int numPageHit;				// The number of page hits
	struct pageTableEntry* firstLevelPageTable;
	FILE* tracefp;
} *procTable;

void oneLevelVMSim(int nf, int type) {
	int i, j, num, test, oldid, k, p;					//i,j: 반복문  num: VPN test,oldid,k,p: 페이지테이블에서 뺄것 정보
	int all = 1; int fin = 0; int cnt = 0;				//all: 2^20저장용 fin:프로세스종료시점 cnt: 페이지테이블 count
	unsigned addr;
	char rw;
	for (i = 0; i < 20; i++) all *= 2;
	physicalTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry) * nf);
	pageTable = (struct pageTableEntry**)malloc(sizeof(struct pageTableEntry) * numProcess);
	for (i = 0; i < numProcess; i++)
		pageTable[i] = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry) * all);

	while (1) {
		for (i = 0; i < numProcess; i++) {
			if (fscanf(procTable[i].tracefp, "%x %c", &addr, &rw) != EOF) {
				num = addr >> 12;
				if (pageTable[i][num].valid == true) {
					procTable[i].numPageHit++;
					if (type == 1) {
						for (j = 0; j < cnt; j++) {
							if (physicalTable[j].id == num && physicalTable[j].pid == i) {
								physicalTable[j].count = procTable[i].ntraces;
								break;
							}
						}
					}
				}
				else {
					procTable[i].numPageFault++;
					if (cnt < nf) {
						pageTable[i][num].valid = true;
						physicalTable[cnt].count = procTable[i].ntraces;
						physicalTable[cnt].id = num;
						physicalTable[cnt++].pid = i;
					}
					else {
						test = physicalTable[0].count; oldid = physicalTable[0].id; k = 0; p = physicalTable[0].pid;
						for (j = 1; j < nf; j++) {
							if (test > physicalTable[j].count) {
								test = physicalTable[j].count;
								oldid = physicalTable[j].id;
								k = j;
								p = physicalTable[j].pid;
							}
							else if (test == physicalTable[j].count) {
								if (p > physicalTable[j].pid) {
									test = physicalTable[j].count;
									oldid = physicalTable[j].id;
									k = j;
									p = physicalTable[j].pid;
								}
							}
						}
						pageTable[p][oldid].valid = false;
						pageTable[i][num].valid = true;
						physicalTable[k].count = procTable[i].ntraces;
						physicalTable[k].id = num;
						physicalTable[k].pid = i;
					}
				}
				procTable[i].ntraces++;
			}
			else {
				fin = 1;
				break;
			}
		}
		if (fin == 1) break;
	}

	for (i = 0; i < numProcess; i++) {
		printf("**** %s *****\n", procTable[i].traceName);
		printf("Proc %d Num of traces %d\n", i, procTable[i].ntraces);
		printf("Proc %d Num of Page Faults %d\n", i, procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n", i, procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
}

void newsecond(struct pageTableEntry** p, int first, int i, int fnum) {
	int m, j;
	int s = 20 - first; int k = 1;
	for (m = 0; m < s; m++) k *= 2;
	second[fnum] = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry) * k);
	p[i][fnum].secondLevel = second[fnum];
}

void twoLevelVMSim(int nf, int first) {

	int i, j, num, fnum, snum, iter, test, oldid, p, k, tf, ts, fin;
	unsigned addr;
	char rw;
	int f = 1; int all = 1; int s = 1; int cnt = 0;

	for (i = 0; i < first; i++) f *= 2;
	for (i = 0; i < 20 - first; i++) s *= 2;
	for (i = 0; i < 20; i++) all *= 2;
	physicalTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry) * nf);
	pageTable = (struct pageTableEntry**)malloc(sizeof(struct pageTableEntry) * numProcess);
	for (i = 0; i < numProcess; i++)
		pageTable[i] = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry) * f);
	second = (struct pageTableEntry**)malloc(sizeof(struct pageTableEntry) * f);

	while (1) {
		for (i = 0; i < numProcess; i++) {
			if (fscanf(procTable[i].tracefp, "%x %c", &addr, &rw) != EOF) {
				num = addr >> 12;
				fnum = addr >> (32 - first);
				snum = num - (fnum << (20 - first));

				if (pageTable[i][fnum].valid == true) {
					if ((pageTable[i][fnum].secondLevel)[snum].valid == true) {
						procTable[i].numPageHit++;
						for (j = 0; j < cnt; j++) {
							if (physicalTable[j].id == num && physicalTable[j].pid == i) {
								physicalTable[j].count = procTable[i].ntraces;
								break;
							}
						}
					}
					else {
						procTable[i].numPageFault++;
						if (cnt < nf) {
							(pageTable[i][fnum].secondLevel)[snum].valid = true;
							physicalTable[cnt].id = num;
							physicalTable[cnt].pid = i;
							physicalTable[cnt++].count = procTable[i].ntraces;
						}
						else {
							test = physicalTable[0].count; oldid = physicalTable[0].id; p = physicalTable[0].pid; k = 0;
							for (j = 1; j < nf; j++) {
								if (test > physicalTable[j].count) {
									test = physicalTable[j].count;
									oldid = physicalTable[j].id;
									k = j;
									p = physicalTable[j].pid;
								}
								else if (test == physicalTable[j].count) {
									if (p > physicalTable[j].pid) {
										test = physicalTable[j].count;
										oldid = physicalTable[j].id;
										k = j;
										p = physicalTable[j].pid;
									}
								}
							}
							(pageTable[i][fnum].secondLevel)[snum].valid = true;
							tf = oldid >> (20 - first);
							ts = oldid - (tf << (20 - first));
							(pageTable[p][tf].secondLevel)[ts].valid = false;
							physicalTable[k].count = procTable[i].ntraces;
							physicalTable[k].id = num;
							physicalTable[k].pid = i;
						}

					}
				}
				else {
					procTable[i].numPageFault++;
					procTable[i].num2ndLevelPageTable++;
					newsecond(pageTable, first, i, fnum);
					if (cnt < nf) {
						pageTable[i][fnum].valid = true;
						(pageTable[i][fnum].secondLevel)[snum].valid = true;
						physicalTable[cnt].id = num;
						physicalTable[cnt].pid = i;
						physicalTable[cnt++].count = procTable[i].ntraces;
					}
					else {
						test = physicalTable[0].count; oldid = physicalTable[0].id; p = physicalTable[0].pid; k = 0;
						for (j = 1; j < nf; j++) {
							if (test > physicalTable[j].count) {
								test = physicalTable[j].count;
								oldid = physicalTable[j].id;
								k = j;
								p = physicalTable[j].pid;
							}
							else if (test == physicalTable[j].count) {
								if (p > physicalTable[j].pid) {
									test = physicalTable[j].count;
									oldid = physicalTable[j].id;
									k = j;
									p = physicalTable[j].pid;
								}
							}
						}
						pageTable[i][fnum].valid = true;
						(pageTable[i][fnum].secondLevel)[snum].valid = true;
						tf = oldid >> (20 - first);
						ts = oldid - (tf << (20 - first));
						(pageTable[p][tf].secondLevel)[ts].valid = false;
						physicalTable[k].count = procTable[i].ntraces;
						physicalTable[k].id = num;
						physicalTable[k].pid = i;
					}
				}
				procTable[i].ntraces++;
			}
			else {
				fin = 1;
				break;
			}
		}
		if (fin == 1) break;
	}
		for (i = 0; i < numProcess; i++) {
			printf("**** %s *****\n", procTable[i].traceName);
			printf("Proc %d Num of traces %d\n", i, procTable[i].ntraces);
			printf("Proc %d Num of second level page tables allocated %d\n", i, procTable[i].num2ndLevelPageTable);
			printf("Proc %d Num of Page Faults %d\n", i, procTable[i].numPageFault);
			printf("Proc %d Num of Page Hit %d\n", i, procTable[i].numPageHit);
			assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		}
}


void invertedPageVMSim(int nf, int first) {

	int i, j, index, num, test, oldid, k, pd, oldindex;
	int fin = 0; int cnt = 0; int hit = 0; int all = 1; int isn = 0; 
	unsigned addr;
	char rw;

	struct pageTableEntry* hashTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry) * nf);
	physicalTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry) * nf);
	struct pageTableEntry* p;

	while (1) {
		for (i = 0; i < numProcess; i++) {
			if (fscanf(procTable[i].tracefp, "%x %c", &addr, &rw) != EOF) {
				num = addr >> 12;
				index = (num + i) % nf;
				p = hashTable[index].next;
				while (p != NULL) {
					procTable[i].numIHTConflictAccess++;
					if (p->id == num && p->pid == i) {
						procTable[i].numPageHit++;
						procTable[i].numIHTNonNULLAcess++;
						hit = 1;
						for (j = 0; j < cnt; j++) {
							if (physicalTable[j].id == num && physicalTable[j].pid == i) {
								physicalTable[j].count = procTable[i].ntraces;
								break;
							}
						}
						break;
					}
					p = p->next;
				}

				if (hit == 0) {
					procTable[i].numPageFault++;
					if (cnt < nf) {
						struct pageTableEntry* newNode = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry));
						newNode->id = num;
						newNode->pid = i;
						struct pageTableEntry* node = &hashTable[index];
						if (node->next == NULL) {
							node->next = newNode;
							newNode->prev = node;
							procTable[i].numIHTNULLAccess++;
						}
						else {
							newNode->next = node->next;
							node->next->prev = newNode;
							node->next = newNode;
							newNode->prev = node;
							procTable[i].numIHTNonNULLAcess++;
						}
						physicalTable[cnt].id = num;
						physicalTable[cnt].pid = i;
						physicalTable[cnt++].count = procTable[i].ntraces;
					}

					else {
						test = physicalTable[0].count; oldid = physicalTable[0].id; k = 0; pd = physicalTable[0].pid;
						for (j = 1; j < nf; j++) {
							if (test > physicalTable[j].count) {
								test = physicalTable[j].count;
								oldid = physicalTable[j].id;
								k = j;
								pd = physicalTable[j].pid;
							}
							else if (test == physicalTable[j].count) {
								if (pd > physicalTable[j].pid) {
									test = physicalTable[j].count;
									oldid = physicalTable[j].id;
									k = j;
									pd = physicalTable[j].pid;
								}
							}
						}
						struct pageTableEntry* newNode = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry));
						newNode->id = num;
						newNode->pid = i;
						struct pageTableEntry* node = &hashTable[index];
						if (node->next == NULL) {
							node->next = newNode;
							newNode->prev = node;
							procTable[i].numIHTNULLAccess++;

						}
						else {
							newNode->next = node->next;
							node->next->prev = newNode;
							node->next = newNode;
							newNode->prev = node;
							procTable[i].numIHTNonNULLAcess++;
						}

						oldindex = (oldid + pd) % nf;
						struct pageTableEntry* a = hashTable[oldindex].next;

						while (!(a->id == oldid && a->pid == pd)) {
							a = a->next;
						}
							if (a->next == NULL) {
								a->prev->next = NULL;
								a->next = NULL;
								a->prev = NULL;
								free(a);
							}
							else {
								struct pageTableEntry* tmp = a->next;
								tmp->prev = a->prev;
								a->prev->next = tmp;
								a->next = NULL;
								a->prev = NULL;
								free(a);
							}
			
						physicalTable[k].count = procTable[i].ntraces;
						physicalTable[k].id = num;
						physicalTable[k].pid = i;
					}
				}

				hit = 0;
				procTable[i].ntraces++;
			//	printf("%d %d %d ", procTable[i].ntraces, procTable[i].numPageHit, procTable[i].numPageFault);
			/*  for (j = 0; j < cnt; j++) printf("%x ", physicalTable[j].id);
				printf("\n");*/

			/*	for (j = 0; j < nf; j++) {
					printf("%d: (", j);
					struct pageTableEntry* p = &hashTable[j];
					while (p->next != NULL) {
						p = p->next;
						printf("%x ", p->id);
					}
					printf(")");
				}
				printf("\n");*/
			}
			else {
				fin = 1;
				break;
			}
		}
		if (fin == 1) break;
	}

	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n",i,procTable[i].numIHTConflictAccess);
		printf("Proc %d Num of Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNULLAccess);
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNonNULLAcess);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAcess == procTable[i].ntraces);
	}
}

int main(int argc, char* argv[]) {
	int i, c, simType;
	int firstLevel, phyMemSizeBits, nFrame;
	simType = atoi(argv[1]);
	firstLevel = atoi(argv[2]);
	phyMemSizeBits = atoi(argv[3]);
	i = 4;	numProcess = 0;	nFrame = 1;
	while (argv[i] != NULL) {
		numProcess++; i++;
	}
	procTable = (struct procEntry*)malloc(sizeof(struct procEntry) * numProcess);

	// initialize procTable for Memory Simulations
	for (i = 0; i < numProcess; i++) {

		// opening a tracefile for the process
		printf("process %d opening %s\n", i, argv[i + 4]);
		procTable[i].traceName = argv[i + 4];
		procTable[i].pid = i;
		procTable[i].ntraces = 0;
		procTable[i].num2ndLevelPageTable = 0;
		procTable[i].numIHTConflictAccess = 0;
		procTable[i].numIHTNonNULLAcess = 0;
		procTable[i].numIHTNULLAccess = 0;
		procTable[i].numPageFault = 0;
		procTable[i].numPageHit = 0;
		procTable[i].tracefp = fopen(argv[i + 4], "r");
		if (procTable[i].tracefp == NULL) {
			printf("ERROR: can't open %s file; exiting...", argv[i + 4]);
			exit(1);
		}
	}
	for (i = 0; i < phyMemSizeBits - 12; i++) {
		nFrame *= 2;
	}
	printf("Num of Frames %d Physical Memory Size %ld bytes\n", nFrame, (1L << phyMemSizeBits));

	if (simType == 0) {
		printf("=============================================================\n");
		printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		oneLevelVMSim(nFrame, simType);
	}

	if (simType == 1) {
		printf("=============================================================\n");
		printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		oneLevelVMSim(nFrame, simType);
	}
	
	if (simType == 2) {
		printf("=============================================================\n");
		printf("The Two-Level Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		twoLevelVMSim(nFrame, firstLevel);
	}
	
	if (simType == 3) {
		printf("=============================================================\n");
		printf("The Inverted Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		invertedPageVMSim(nFrame, firstLevel);
	}
	
	return(0);
}