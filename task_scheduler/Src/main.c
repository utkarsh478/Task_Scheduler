#include <stdint.h>
#include <stdio.h>
#include "main.h"
#include "led.h"

#if !defined(__SOFT_FP__) && defined(__ARM_FP)
  #warning "FPU is not initialized, but the project is compiling for an FPU. Please initialize the FPU before use."
#endif


void task1_handler(void);
void task2_handler(void);
void task3_handler(void);
void task4_handler(void);

void init_systick_timer(uint32_t tick_hz);

void init_scheduler_stack(uint32_t sched_top_of_stack);

void init_tasks_stack();

void enable_processor_faults(void);

uint32_t get_psp_value();

__attribute__((naked)) void switch_sp_to_psp();


uint32_t psp_of_tasks[MAX_TASKS]={T1_STACK_START,T2_STACK_START,T3_STACK_START,T4_STACK_START};
uint32_t task_handlers[MAX_TASKS];

uint32_t current_task = 0;

int main(void)
{
	enable_processor_faults();

	init_scheduler_stack(SCHED_STACK_START);

	//msp =sp

	task_handlers[0]=(uint32_t)task1_handler;
	task_handlers[1]=(uint32_t)task2_handler;
	task_handlers[2]=(uint32_t)task3_handler;
	task_handlers[3]=(uint32_t)task4_handler;

	init_tasks_stack();

	led_init_all();

	init_systick_timer(TICK_HZ);

	//psp=sp

	switch_sp_to_psp();

	task1_handler();


    /* Loop forever */
	for(;;);
}

void enable_processor_faults(void){
	uint32_t *pSHCSR = (uint32_t*)0xE000ED24;
	*pSHCSR |= (1 << 16); //mem manage
	*pSHCSR |= (1 << 17); //bus fault
	*pSHCSR	|= (1 << 18); //usage fault

}



void init_systick_timer(uint32_t tick_hz){

	uint32_t *pSRVR = (uint32_t*)0xE000E014;
	uint32_t *pSCSR = (uint32_t*)0xE000E010;

	uint32_t count_value = (SYSTICK_TIM_CLK/tick_hz) - 1;

	//clear the value
	*pSRVR &= ~(0x00FFFFFF);

	//load the value

	*pSRVR |= count_value;

	//do some setting


	*pSCSR = (1<<1);
	*pSCSR = (1<<2);

	//enable systic

	*pSCSR = (1<<0);

}

uint32_t save_psp_value(uint32_t current_psp_value){

	return psp_of_tasks[current_task] = current_psp_value;
}

void update_next_task(void){
	current_task++;
	current_task %= MAX_TASKS;
}


__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack){

	__asm volatile("MSR MSP,%0": : "r" (sched_top_of_stack): ); //variable to arm
	__asm volatile("BX LR"); //return from fun call

}

void init_tasks_stack(){

	uint32_t *pPSP;

	for(int i=0;i<MAX_TASKS;i++){
		pPSP = (uint32_t*)psp_of_tasks[i]; // if i =0 it will give the value of T1_STACK_START

		pPSP--; //xpsp
		pPSP = (uint32_t*)DUMMY_XPSR; //0x00100000

		pPSP--; // pc
		pPSP = (uint32_t*)task_handlers[i];

		pPSP--; // LR
		pPSP = (uint32_t*)0xFFFFFFFD;

		for(int j=0;j<13;j++){
			pPSP--;
			*pPSP=0;
		}

		psp_of_tasks[i]=(uint32_t)pPSP;



	}

}


__attribute__((naked)) void SysTick_Handler(void){
	//saving current contex
	__asm volatile ("MRS R0,PSP"); // CURRENT PSP ->  R0

	__asm volatile ("STMDB R0!,{R4-R11}"); //STORE THE VALUE R4-R11 to R0 AND RETUTRE THE LAST ADDRESS TO R0

	__asm volatile ("PUSH {LR}");

	__asm volatile ("BL save_psp_value"); //store current psp



	//retriving the nxt contex

	__asm volatile("BL update_next_task"); // decide next task to run

	__asm volatile("BL get_psp_value"); 	// get its last psp value

	__asm volatile("LDMIA R0!,{R4-R11}");	// retrive value in r4-r11

	__asm volatile("MSR PSP,R0"); // UPDATE PSP AND EXIT

	__asm volatile ("POP {LR}");

	__asm volatile ("BX LR");

}

void HardFault_Handler(void){
	printf("Exception: Hardfault\n");
	while(1);
}


void MemManage_Handler(void){
	printf("Exception: MemManage\n");
	while(1);
}
void BusFault_Handler(void){
	printf("Exception: BusFault\n");
	while(1);
}


uint32_t get_psp_value(){
	return psp_of_tasks[current_task];
}

__attribute__((naked)) void switch_sp_to_psp(){
	//init psp with task1 stack start



	//get the value of psp of current

	__asm volatile ("PUSH {LR}");//preserve lr because it is going back to main
	__asm volatile ("BL get_psp_value");
	__asm volatile ("MSR PSP,R0");
	__asm volatile ("POP {LR}");

	//change sp to psp
	__asm volatile ("MOV R0,0x02");
	__asm volatile ("MSR CONTROL,R0");
	__asm volatile ("BX LR");


}



void task1_handler(void){
	while(1){
		printf("this is task 1\n");

	}
}

void task2_handler(void){
	while(1){
		printf("this is task 2\n");
	}
}

void task3_handler(void){
	while(1){
		printf("this is task 3\n");
	}
}


void task4_handler(void){
	while(1){
		printf("this is task 4\n");
	}
}
