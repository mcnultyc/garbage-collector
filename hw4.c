#include "memlib.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mm.h"

struct memory_region{
  size_t * start;
  size_t * end;
};

struct memory_region global_mem;
struct memory_region stack_mem;

void walk_region_and_mark(void* start, void* end);


//grabbing the address and size of the global memory region from proc 
void init_global_range(){
  int next = 0;
  char file[100];
  char * line=NULL;
  size_t n=0;
  size_t read_bytes=0;
  size_t start, end;


  sprintf(file, "/proc/%d/maps", getpid());
  FILE * mapfile  = fopen(file, "r");
  if (mapfile==NULL){
    perror("opening maps file failed\n");
    exit(-1);
  }

  int counter=0;
  while ((read_bytes = getline(&line, &n, mapfile)) != -1) {
    // .data of hw4 executable
    if (strstr(line, "hw4") != NULL && strstr(line,"rw-p")){
      sscanf(line, "%lx-%lx", &start, &end);
      global_mem.start = (void*) start;
      global_mem.end = (void*) end;
      break;
    }

  }
  fclose(mapfile);
}


//marking related operations

int is_marked(unsigned int * chunk) {
  return ((*chunk) & 0x2) > 0;
}

void mark(unsigned int * chunk) {
  (*chunk)|=0x2;
}

void clear_mark(unsigned int * chunk) {
  (*chunk)&=(~0x2);
}

// chunk related operations

#define chunk_size(c)  ((*((unsigned int *)c))& ~(unsigned int)7 ) 
void* next_chunk(void* c) { 
  if(chunk_size(c) == 0) {
    fprintf(stderr,"Panic, chunk is of zero size.\n");
  }
  if((c+chunk_size(c)) < mem_heap_hi())
    return ((void*)c+chunk_size(c));
  else 
    return 0;
}

int in_use(void *c) { 
  return *(unsigned int *)(c) & 0x1;
}

// the actual collection code
void sweep() {
  void *current = mem_heap_lo(); 
  while(chunk_size(current)){
    if(is_marked(current)){
      clear_mark(current);
    }
    else if(in_use(current)){
      mm_free((unsigned int*)current + 1);
    }
    current += chunk_size(current);
  }
}

int in_block(void *ptr, void *c){
  return ptr >= (void*)((unsigned int*)c+1) && ptr < c + chunk_size(c);
}

//determine if what "looks" like a pointer actually points to a block in the heap
void * is_pointer(void *ptr) {
  void *current = mem_heap_lo();  
  while(chunk_size(current)){
    if(in_use(current) && in_block(ptr, current)){
      return current;
    }
    current += chunk_size(current);
  }
  return 0;
}

void walk_region_and_mark(void* start, void* end) {
  void *chunk;
  while(start < end){
    chunk = is_pointer((void*)*((size_t*)start));
    if(chunk){
      if(!is_marked(chunk)){
        mark(chunk);
        walk_region_and_mark((unsigned int*)chunk + 1, 
                chunk+chunk_size(chunk));
      }
    }
    start = (size_t*)start + 1;
  }
}

void print_heap(){
  fprintf(stdout, "HEAP:\n");
  void *current = mem_heap_lo(); 
  fprintf(stdout, "chunk header at: %p, size: %lu\n", current, chunk_size(current));
  fprintf(stdout, "chunk malloc'd at: %p\n", (unsigned int*)current + 1);
  while(chunk_size(current)){
    fprintf(stdout, "chunk header at: %p, size: %lu\n", current, chunk_size(current));
    fprintf(stdout, "chunk malloc'd at: %p\n", (unsigned int*)current + 1); 
    current += chunk_size(current);
  }
  fprintf(stdout, "END HEAP\n");
}

// standard initialization 

void init_gc() {
  size_t stack_var;
  init_global_range();
  //since the heap grows down, the end is found first
  stack_mem.end=(size_t *)&stack_var;
}

void gc() {
  size_t stack_var;
  //grows down, so start is a lower address
  stack_mem.start=(size_t *)&stack_var;

  //print_heap();
  //walk memory regions
  walk_region_and_mark(global_mem.start,global_mem.end);
  walk_region_and_mark(stack_mem.start,stack_mem.end);
  sweep();
}
