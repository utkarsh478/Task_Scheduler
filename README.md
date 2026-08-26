# Preemptive Round-Robin Task Scheduler — STM32F446RE (Cortex-M4)

A hand-written preemptive scheduler for the ARM Cortex-M4, built from scratch at the register level on a NUCLEO-F446RE. No RTOS, no HAL, no CMSIS driver layer — only raw memory-mapped register access and inline assembly for the context switch.

The point of the project is the switch itself: how the Cortex-M exception model, the dual-stack-pointer architecture, and `EXC_RETURN` combine to let one core run five independent tasks.

> **Status:** Learning project. Runs four blinking tasks plus an idle task. Several known defects are documented in [Known Limitations](#known-limitations) — they are listed deliberately rather than hidden.

---

## What it does

Five tasks share one Cortex-M4 core:

| Task | LED | Pin | Blink rate |
|---|---|---|---|
| `task1_handler` | Green (LD2, on-board) | PA5 | 0.5 Hz |
| `task2_handler` | Orange (external) | PA6 | 1 Hz |
| `task3_handler` | Red (external) | PA7 | 2 Hz |
| `task4_handler` | Blue (external) | PA8 | 4 Hz |
| `idle_task` | — | — | runs when all tasks are blocked |

Each task calls `task_delay()` and yields the core. The scheduler switches to the next ready task. When every user task is blocked, the idle task runs until a tick wakes one of them.

Only the green LED is on the board. PA6/PA7/PA8 need external LEDs with series resistors.

---

## Hardware and toolchain

- **Board:** NUCLEO-F446RE (STM32F446RET6, Cortex-M4F, 128 KB SRAM)
- **Clock:** 16 MHz HSI, no PLL — `SystemInit()` is left untouched
- **IDE:** STM32CubeIDE (arm-none-eabi-gcc)
- **Debug/flash:** on-board ST-LINK V2-1

---

## Architecture

### Privilege and stack pointers

The Cortex-M4 has two stack pointers. The scheduler uses both:

- **MSP** — used by `main()` during startup and by every exception handler
- **PSP** — used by task code in thread mode

`init_scheduler_stack()` relocates MSP to a dedicated scheduler region. `switch_sp_to_psp()` then writes `0x02` to the `CONTROL` register, which switches thread mode over to PSP. From that point, task code and handler code live on separate stacks — a task stack overflow cannot corrupt the exception path's own stack.

### The task stack frame

Every task's stack is pre-loaded in `init_tasks_stack()` with a fake exception frame — 16 words that look exactly like what the hardware would have pushed if the task had been interrupted:

```
   higher address
   ┌──────────────────┐
   │  xPSR  0x01000000│  <- T-bit set. Cortex-M is Thumb-only;
   │  PC    task_addr │     a clear T-bit gives UsageFault (INVSTATE)
   │  LR    0xFFFFFFFD│
   │  R12   0          │  hardware-stacked frame (8 words)
   │  R3    0          │
   │  R2    0          │
   │  R1    0          │
   │  R0    0          │
   ├──────────────────┤
   │  R11   0          │
   │  R10   0          │
   │  R9    0          │  manually-stacked frame (8 words)
   │  ...              │
   │  R4    0          │  <- psp_value points here
   └──────────────────┘
   lower address
```

The saved PSP points at the R4 slot, because `LDMIA R0!, {R4-R11}` in the context switch starts there. The hardware unstacks the upper 8 words on exception return.

`0xFFFFFFFD` is the `EXC_RETURN` value meaning *return to thread mode, use PSP, no floating-point context*.

### SysTick and PendSV

The two exceptions have separate jobs:

- **`SysTick_Handler`** (1 kHz) — increments the tick counter, scans for tasks whose wake-up tick has arrived, then sets the PENDSVSET bit in ICSR
- **`PendSV_Handler`** — does the actual context switch

The split matters. PendSV is the architecturally intended place for a context switch because it can be made the lowest-priority exception, so it tail-chains after every other pending ISR has finished. You never switch stacks halfway through servicing a peripheral.

### The context switch

```
MRS   R0, PSP            ; current task's stack pointer
STMDB R0!, {R4-R11}      ; save callee-saved registers
PUSH  {LR}               ; preserve EXC_RETURN across the BLs
BL    save_psp_value     ; store PSP in the outgoing TCB
BL    update_next_task   ; round-robin scan for a READY task
BL    get_psp_value      ; fetch incoming task's PSP
LDMIA R0!, {R4-R11}      ; restore callee-saved registers
MSR   PSP, R0            ; point PSP at the new task's frame
POP   {LR}
BX    LR                 ; EXC_RETURN — hardware unstacks the rest
```

R0–R3, R12, LR, PC and xPSR are saved and restored by hardware. Only R4–R11 need manual handling — they are callee-saved under AAPCS, so the compiler assumes they survive a function call.

### Blocking and waking

`task_delay(ticks)` records `global_tick_count + ticks` in the task's TCB, marks it `TASK_BLOCKED_STATE`, and pends PendSV to yield immediately. `unblock_tasks()` runs on every SysTick and returns a task to `TASK_READY_STATE` once the tick count reaches its wake-up value.

`update_next_task()` skips task 0 (idle) while scanning. If no user task is ready, it falls back to idle.

---

## Memory map

128 KB SRAM at `0x20000000`, carved into 1 KB regions from the top down:

| Region | Address |
|---|---|
| SRAM end | `0x20020000` |
| Task 1 stack | `0x20020000` |
| Task 2 stack | `0x2001FC00` |
| Task 3 stack | `0x2001F800` |
| Task 4 stack | `0x2001F400` |
| Idle stack | `0x2001F000` |
| Scheduler stack (MSP) | `0x2001EC00` |

Full-descending stacks. Regions are adjacent with no guard bands — see limitations.

---

## SysTick configuration

```
reload = (SYSTICK_TIM_CLK / TICK_HZ) - 1
       = (16 000 000 / 1000) - 1
       = 15 999
```

`SYST_CSR` bits set: CLKSOURCE (processor clock, not HCLK/8), TICKINT (enable exception), ENABLE.

---

## Files

```
main.c    scheduler core, task stacks, SysTick/PendSV, task handlers
main.h    stack layout, tick rate, task states, DUMMY_XPSR
led.c     GPIOA register-level driver (BSRR-based, atomic)
led.h     pin mapping and delay constants
```

`led.c` uses BSRR rather than a read-modify-write on ODR. In a preemptive system, `ODR |= bit` is three instructions with a preemption window in the middle — two tasks toggling different pins on the same port can lose a write.

---

## Build and flash

1. Import into STM32CubeIDE as an existing project, or create an empty STM32F446RETx project and drop these files into `Core/Src` and `Core/Inc`
2. Build
3. Flash over ST-LINK
4. Green LD2 should blink at 0.5 Hz

To watch it work: break in `PendSV_Handler` and inspect `current_task`, or watch `global_tick_count` in a live expression.

---

## Known limitations

These are real defects, left in and documented rather than papered over.

**Architectural**

- **Round-robin only.** No task priorities, no preemption by importance. Every ready task gets one tick.
- **No mutexes, semaphores, or queues.** Tasks cannot communicate or share resources safely.
- **No stack overflow detection.** Stack regions are adjacent with no guard bands or canaries. An overflowing task silently corrupts its neighbour.
- **Static task set.** Five tasks fixed at compile time; no create/delete at runtime.

**Correctness**

- **Missing ISB after `MSR CONTROL, R0`.** The architecture requires an instruction synchronisation barrier before the new stack pointer selection is guaranteed visible to subsequent instructions.
- **`PUSH {LR}` breaks 8-byte SP alignment.** A single-register push leaves the stack 4-byte aligned; AAPCS requires 8-byte alignment at every call boundary. Should be `PUSH {R0, LR}`.
- **FPU context is not saved.** The project builds hard-float. S16–S31 are callee-saved and are not handled in the switch, so any task using floating point would be corrupted on preemption.
- **`task_delay()` is not atomic.** SysTick can preempt between the `block_count` write and the `current_state` write.
- **SysTick is enabled before PSP is set** in `main()`, and `SYST_CVR` is never cleared. CVR is UNKNOWN at reset, so a tick can in principle fire before the stack pointer is valid.
- **PendSV priority is not configured.** Both SysTick and PendSV sit at priority 0. It works here by tail-chaining, but PendSV should be set to the lowest priority in SHPR3 before any peripheral interrupt is added.
- **UsageFault is enabled in SHCSR with no handler defined**, so it falls through to the weak `Default_Handler` and the diagnostic is lost.
- **`printf()` in fault handlers.** Calls into newlib and `_sbrk` from a fault context, which can turn a debuggable fault into a lockup.

---


Primary documents:

- ARM Cortex-M4 Devices Generic User Guide (ARM DUI 0553)
- STM32F446xx Reference Manual (RM0390)
- NUCLEO-64 User Manual (UM1724)
