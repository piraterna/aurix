/*********************************************************************************/
/* Module Name:  sched.c */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/*********************************************************************************/

#include <sys/sched.h>
#include <mm/heap.h>
#include <debug/log.h>
#include <string.h>

#define SCHED_DEFAULT_SLICE 10

static tcb *run_queue = NULL;
static tcb *current_thread = NULL;

void sched_init()
{
	run_queue = NULL;
	current_thread = NULL;
}