#include <unistd.h>
#include <stdio.h>
#include "smalloc.h" 

sm_container_ptr sm_first = 0x0 ;
sm_container_ptr sm_last = 0x0 ;
sm_container_ptr sm_unused_containers = 0x0 ;

void sm_container_split(sm_container_ptr hole, size_t size)
{
	sm_container_ptr remainder = hole->data + size ;

	remainder->data = ((void *)remainder) + sizeof(sm_container_t) ;
	remainder->dsize = hole->dsize - size - sizeof(sm_container_t) ;
	remainder->status = Unused ;
	remainder->next = hole->next ;
	hole->next = remainder ;

	if (hole == sm_last)
		sm_last = remainder ;
}

void * sm_retain_more_memory(int size)
{
	sm_container_ptr hole ;
	int pagesize = getpagesize() ;
	int n_pages = 0 ;

	n_pages = (sizeof(sm_container_t) + size + sizeof(sm_container_t)) / pagesize  + 1 ;
	hole = (sm_container_ptr) sbrk(n_pages * pagesize) ;
	if (hole == 0x0)
		return 0x0 ;

	hole->data = ((void *) hole) + sizeof(sm_container_t) ;
	hole->dsize = n_pages * getpagesize() - sizeof(sm_container_t) ;
	hole->status = Unused ;

	return hole ;
}

void * smalloc(size_t size) 
{
	sm_container_ptr hole = 0x0 ;
	sm_container_ptr itr = 0x0 ;
	sm_container_ptr next = 0x0 ;

	int smallest = 10000 ;

	for (itr = sm_first ; itr != 0x0 ; itr = itr->next) {
		if (itr->status == Busy)
			continue ;

		if (size == itr->dsize) {
			// a hole of the exact size
			itr->status = Busy ;
			return itr->data ;
		}
		else if (size + sizeof(sm_container_t) < itr->dsize) {
			// a hole large enought to split 
			if (itr->dsize < smallest) {
				smallest = itr->dsize ;
				hole = itr ;
			}
		}

		//linked list of unused container
		if (next != 0x0) {
			next->next_unused = itr ;	
			next = next->next_unused ;	
		}
		else {
			sm_unused_containers = itr ;
			next = sm_unused_containers ;
		}
	}
	
	if (hole == 0x0) {
		hole = sm_retain_more_memory(size) ;

		if (hole == 0x0)
			return 0x0 ;

		if (sm_first == 0x0) {
			sm_first = hole ;
			sm_last = hole ;
			hole->next = 0x0 ;
		}
		else {
			sm_last->next = hole ;
			sm_last = hole ;
			hole->next = 0x0 ;
		}
	}
	sm_container_split(hole, size) ;
	hole->dsize = size ;
	hole->status = Busy ;
	return hole->data ;
}



void sfree(void * p)
{
	sm_container_ptr itr ;
	sm_container_ptr cont ;

	for (itr = sm_first ; itr->next != 0x0 ; itr = itr->next) {

		if (itr->data == p) {
			itr->status = Unused ;
		}

		//merge
		if ((itr->status == Unused) && (itr->next->status == Unused)) {
			itr->dsize +=  itr->next->dsize ;

			if (itr->next->next != 0x0) {
				cont->next = itr->next ;
				itr->next = itr->next->next ;
				cont = itr->next ;
			}
			else {
				itr->next = 0x0 ;
			}
		}
	}
}

void print_sm_containers()
{
	sm_container_ptr itr ;
	int i = 0 ;

	printf("==================== sm_containers ====================\n") ;
	for (itr = sm_first ; itr != 0x0 ; itr = itr->next, i++) {
		char * s ;
		printf("%3d:%p:%s:", i, itr->data, itr->status == Unused ? "Unused" : "  Busy") ;
		printf("%8d:", (int) itr->dsize) ;

		for (s = (char *) itr->data ;
			 s < (char *) itr->data + (itr->dsize > 8 ? 8 : itr->dsize) ;
			 s++) 
			printf("%02x ", *s) ;
		printf("\n") ;
	}
	printf("=======================================================\n") ;

}

void print_sm_uses() {
	sm_container_ptr itr = 0x0 ;
	size_t retained = 0 ;
	size_t allocated = 0 ;
	size_t uncalloc = 0 ;
	int i = 0 ;
	char buf[100] ;

	printf("\n================= sm_uses =================\n") ;

	for (itr = sm_first; itr != 0x0; itr = itr->next, i++) {
		retained += ( itr->dsize + sizeof(sm_container_t) ) ;
		if (itr->status == Busy)
			allocated += itr->dsize ;
		else
			uncalloc += itr->dsize ;
	}

	sprintf(buf, "Retained : %d\n", (int)retained) ;
	fputs(buf, stderr) ;
	printf("Allocated : %d\n", (int)allocated) ;
	fputs(buf, stderr) ;
	printf("Retained but unallocated : %d\n", (int)uncalloc) ;
	fputs(buf, stderr) ;
	printf("=======================================================\n") ;
}
