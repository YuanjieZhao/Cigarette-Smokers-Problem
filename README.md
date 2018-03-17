# Cigarette-Smokers-Problem
A solution to Cigarette Smokers Problem with clear explanation

Cigarette smokers problem – classic concurrency problem

**Motivation of this post**

The cigarette smokers problem was originally proposed by Suhas Patil, who claimed that one cannot solve it with semaphores. It resembles a producer-consumer problem but has extra constraints that make this problem very interesting. There are many ways to tackle this problem, but very few posts online clearly explain how they came up with their solutions. Here I present a natural strategy I have come up with and clear explanations of my thought process, in the hope that other readers would find it useful.

The detailed implementation of solution is in smoke.c file.

**Problem description** (from Wikipedia)

Assume a cigarette requires three ingredients to make and smoke: tobacco, paper, and matches. There are three smokers around a table, each of whom has an infinite supply of _one_ of the three ingredients — one smoker has an infinite supply of tobacco, another has paper, and the third has matches.

There is also a non-smoking agent who enables the smokers to make their cigarettes by arbitrarily (non-deterministically) selecting two of the supplies to place on the table. The smoker who has the third supply should remove the two items from the table, using them (along with their own supply) to make a cigarette, which they smoke for a while. Once the smoker has finished his cigarette, the agent places two new random items on the table. This process continues forever.

**Constraints**

To model a resource-management problem of operating systems in real situations, the following constraints are applied to the agent (the agent represents an operating system):

1. The agent is only allowed to communicate by signaling the availability of a resource using a condition variable.
2. The agent is not permitted to disclose resource availability in any other way; i.e., smokers cannot ask the agent what is available.
3. The agent is not permitted to know anything about the resource needs of smokers; i.e., the agent cannot wakeup a smoker directly.
4. Each time the agent makes two resources available, it must wait on a condition variable for a smoker to smoke before it can make any additional resources available.

**Problem analysis**

**Trick of the problem**

When the agent makes two items available, every smoker thread can use one of them, but only one can use both. For example, if the agent makes paper and matches available, both the paper and the matches smokers want one of these, but neither will be able to smoke because neither has tobacco. But, if either of them does wake up and consume a resource, that will prevent the tobacco thread from begin able to smoke and thus also prevent the agent from waking up to deliver additional resources. If this happens, the system is deadlocked; no thread will be able to make further progress.

**My strategy**

In each iteration of the agent, the agent signals two events and the smokers must wait for both events to decide how to behave accordingly

However, the condition\_wait operation of a monitor can only be used for a single event. Stacking two condition\_wait operations in each smoker function doesn&#39;t help, because this method requires us to know the order of the two events beforehand (i.e. which event take places first) and therefore is impossible. Thus, we need an intermediate mechanism that can intercept the two events signaled from the agent, combine them into a single event, and then signal the smokers (i.e. wake up the correct smoker).

**Implementation outline**

There are different approaches to implement the strategy mentioned above. My solution implements the intermediate mechanism as three listeners sharing a global variable called sum. Each listener is only responsible for a certain type of event (paper/match/tobacco). I use the sum to record the events that listeners have received so far. Each combination of two events adds up to a unique value. Once the sum reaches one of the characteristic values, listeners will trigger a new event and signal the smokers, which will wake up the correct smoker.

**Additional notes of C implementation files**

1. To test the output, I record the number of times that the agent signals resources and the number of times that a smoker smokes.
2. Instead of using printf for debugging, I defined a macro called VERBOSE\_PRINT, so I can turn diagnostic printf&#39;s on or off when I compile the program.

To turn them on:

gcc -D VERBOSE -std=gnu11 -o smoke smoke.c uthread.c uthread\_mutex\_cond.c -pthread

To turn them off:

gcc -std=gnu11 -o smoke smoke.c uthread.c uthread\_mutex\_cond.c –pthread

1. Uthread library is a wrapper of pthread library in standard C but has simpler interface. Uthread library is written by Professor Mike Feeley.