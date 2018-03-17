#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

// #include <time.h>

/* Strategy to solve Cigarette Smokers Problem
 * In each iteration, the agent signals two events, and the smokers must wait for both events to decide how to behave accordingly
 * However, uthread_cond_wait(...) can only be used for a single event. Stacking two uthread_cond_wait(...) in the smoker function
 * doesn't help because doing so requires us to know the order of the two events beforehand (i.e. which event take places first), which
 * is impossible. Thus, we need an intermediate mechanism that can intercept the two events signaled from the agent, combine them 
 * into a single event, and then signal the smokers (i.e. wake up the correct smoker).
 * 
 * My solution implements this intermediate mechanism as three listeners sharing a global variable sum. Each listener is responsible
 * for a certain type of event (paper/match/tobacco). I use the sum to record the events that listeners have received so far. Each combination
 * of two events add up to a unique value. Once the sum reaches one of the characteristic values, listeners will trigger a new event 
 * and signal the smokers, which will wake up the correct smoker.
 * 
 */

#define NUM_ITERATIONS 1000

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

int sum = 0;
uthread_cond_t match_and_paper;
uthread_cond_t paper_and_tobacco;
uthread_cond_t match_and_tobacco;

struct Agent {
  uthread_mutex_t mutex;
  uthread_cond_t  match;
  uthread_cond_t  paper;
  uthread_cond_t  tobacco;
  uthread_cond_t  smoke;
};

struct Agent* createAgent() {
  struct Agent* agent = malloc (sizeof (struct Agent));
  agent->mutex   = uthread_mutex_create();
  agent->paper   = uthread_cond_create (agent->mutex);
  agent->match   = uthread_cond_create (agent->mutex);
  agent->tobacco = uthread_cond_create (agent->mutex);
  agent->smoke   = uthread_cond_create (agent->mutex);
  return agent;
}

/**
 *   Note that Resource enum had values 1, 2 and 4 so you can combine resources;
 *   e.g., having a MATCH and PAPER is the value MATCH | PAPER == 1 | 2 == 3
 */
enum Resource            {    MATCH = 1, PAPER = 2,   TOBACCO = 4}; // why TOBACCO = 4 not 3 ???
char* resource_name [] = {"", "match",   "paper", "", "tobacco"};

int signal_count [5];  // # of times resource signalled
int smoke_count  [5];  // # of times smoker with resource smoked

/**
 * This is the agent procedure.  It is complete and you shouldn't change it in
 * any material way.  You can re-write it if you like, but be sure that all it does
 * is choose 2 random reasources, signal their condition variables, and then wait
 * wait for a smoker to smoke.
 */
void* agent (void* av) {
  struct Agent* a = av;
  static const int choices[]         = {MATCH|PAPER, MATCH|TOBACCO, PAPER|TOBACCO};
  static const int matching_smoker[] = {TOBACCO,     PAPER,         MATCH};
  
  uthread_mutex_lock (a->mutex);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
      int r = random() % 3;
      signal_count [matching_smoker [r]] ++;
      int c = choices [r];
      if (c & MATCH) { // how does this implementation enable agent to choose 2 random resource???
        VERBOSE_PRINT ("match available\n");
        uthread_cond_signal (a->match);
      }
      if (c & PAPER) {
        VERBOSE_PRINT ("paper available\n");
        uthread_cond_signal (a->paper);
      }
      if (c & TOBACCO) {
        VERBOSE_PRINT ("tobacco available\n");
        uthread_cond_signal (a->tobacco);
      }
      VERBOSE_PRINT ("agent is waiting for smoker to smoke\n");
      uthread_cond_wait (a->smoke);
    }
  uthread_mutex_unlock (a->mutex);
  return NULL;
}

// =========================================
void try_wake_up_smoker(int s){
  switch(s){
    case MATCH + PAPER :
      // wake up tabacco smoker
      VERBOSE_PRINT ("Wake up Tobacco smoker.\n");
      uthread_cond_signal(match_and_paper);
      sum = 0;
      break;
    case PAPER + TOBACCO:
      // wake up match smoker
      VERBOSE_PRINT ("Wake up Match smoker.\n");
      uthread_cond_signal(paper_and_tobacco);
      sum = 0;
      break;
    case MATCH + TOBACCO:
      // wake up paper smoker
      VERBOSE_PRINT ("Wake up Paper smoker.\n");
      uthread_cond_signal(match_and_tobacco);
      sum = 0;
      break;
    default:
      // do nothing
      break;
  }
}

// =================  listener fn =====================

void* tobacco_listener (void* av){
  struct Agent* a = av;
  uthread_mutex_lock(a->mutex); // must request acquiring mutex outside while loop
  while(1){
    uthread_cond_wait(a->tobacco);
    sum = sum + TOBACCO;
    try_wake_up_smoker(sum);
  }
  uthread_mutex_unlock(a->mutex);
}

void* paper_listener (void* av){
  struct Agent* a = av;
  uthread_mutex_lock(a->mutex);
  while(1){
    uthread_cond_wait(a->paper);
    sum = sum + PAPER;
    try_wake_up_smoker(sum);
  }
  uthread_mutex_unlock(a->mutex);
}

void* match_listener (void* av){
  struct Agent* a = av;
  uthread_mutex_lock(a->mutex);
  while(1){
    uthread_cond_wait(a->match);
    sum = sum + MATCH;
    try_wake_up_smoker(sum);
  }
  uthread_mutex_unlock(a->mutex);
}

// =================  smoker fn =====================

void* tabacco_smoker (void* av){
  struct Agent* a = av;
  uthread_mutex_lock(a->mutex);
  while(1){
    uthread_cond_wait(match_and_paper);
    VERBOSE_PRINT ("Tobacco smoker is smoking.\n");
    uthread_cond_signal(a->smoke);
    smoke_count [TOBACCO]++;
  }
  uthread_mutex_unlock(a->mutex);
}

void* match_smoker (void* av){
  struct Agent* a = av;
  uthread_mutex_lock(a->mutex);
  while(1){
    uthread_cond_wait(paper_and_tobacco);
    VERBOSE_PRINT ("Match smoker is smoking.\n");
    uthread_cond_signal(a->smoke);
    smoke_count [MATCH]++;
  }
  uthread_mutex_unlock(a->mutex);
}

void* paper_smoker (void* av){
  struct Agent* a = av;
  uthread_mutex_lock(a->mutex);
  while(1){
    uthread_cond_wait(match_and_tobacco);
    VERBOSE_PRINT ("Paper smoker is smoking.\n");
    uthread_cond_signal(a->smoke);
    smoke_count [PAPER]++;
  }
  uthread_mutex_unlock(a->mutex);
}

int main (int argc, char** argv) {
  uthread_init (7);
  // Initialize objects
  struct Agent*  a = createAgent();
  match_and_paper = uthread_cond_create(a->mutex);
  paper_and_tobacco = uthread_cond_create(a->mutex);
  match_and_tobacco = uthread_cond_create(a->mutex);

  // Initialize threads
  uthread_create (tobacco_listener, a);
  uthread_create (paper_listener, a);
  uthread_create (match_listener, a);
  uthread_create (tabacco_smoker, a);
  uthread_create (match_smoker, a);
  uthread_create (paper_smoker, a);

  uthread_join (uthread_create (agent, a), 0);

  assert (signal_count [MATCH]   == smoke_count [MATCH]);
  assert (signal_count [PAPER]   == smoke_count [PAPER]);
  assert (signal_count [TOBACCO] == smoke_count [TOBACCO]);
  assert (smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS);
  printf ("Smoke counts: %d matches, %d paper, %d tobacco\n",
          smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);
}