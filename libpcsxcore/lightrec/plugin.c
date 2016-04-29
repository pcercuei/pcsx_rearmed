#include <disassembler.h>
#include <lightrec.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "../cdrom.h"
#include "../gte.h"
#include "../mdec.h"
#include "../psxdma.h"
#include "../psxhw.h"
#include "../psxmem.h"
#include "../r3000a.h"

static struct lightrec_state *lightrec_state;

static char cache_control[512];

static char *name = "pcsx";

/* Unused for now */
u32 event_cycles[PSXINT_COUNT];
u32 next_interupt;

enum my_cp2_opcodes {
	OP_CP2_RTPS		= 0x01,
	OP_CP2_NCLIP		= 0x06,
	OP_CP2_OP		= 0x0c,
	OP_CP2_DPCS		= 0x10,
	OP_CP2_INTPL		= 0x11,
	OP_CP2_MVMVA		= 0x12,
	OP_CP2_NCDS		= 0x13,
	OP_CP2_CDP		= 0x14,
	OP_CP2_NCDT		= 0x16,
	OP_CP2_NCCS		= 0x1b,
	OP_CP2_CC		= 0x1c,
	OP_CP2_NCS		= 0x1e,
	OP_CP2_NCT		= 0x20,
	OP_CP2_SQR		= 0x28,
	OP_CP2_DCPL		= 0x29,
	OP_CP2_DPCT		= 0x2a,
	OP_CP2_AVSZ3		= 0x2d,
	OP_CP2_AVSZ4		= 0x2e,
	OP_CP2_RTPT		= 0x30,
	OP_CP2_GPF		= 0x3d,
	OP_CP2_GPL		= 0x3e,
	OP_CP2_NCCT		= 0x3f,
};

static void (*cp2_ops[])(struct psxCP2Regs *) = {
	[OP_CP2_RTPS] = gteRTPS,
	[OP_CP2_RTPS] = gteRTPS,
	[OP_CP2_NCLIP] = gteNCLIP,
	[OP_CP2_OP] = gteOP,
	[OP_CP2_DPCS] = gteDPCS,
	[OP_CP2_INTPL] = gteINTPL,
	[OP_CP2_MVMVA] = gteMVMVA,
	[OP_CP2_NCDS] = gteNCDS,
	[OP_CP2_CDP] = gteCDP,
	[OP_CP2_NCDT] = gteNCDT,
	[OP_CP2_NCCS] = gteNCCS,
	[OP_CP2_CC] = gteCC,
	[OP_CP2_NCS] = gteNCS,
	[OP_CP2_NCT] = gteNCT,
	[OP_CP2_SQR] = gteSQR,
	[OP_CP2_DCPL] = gteDCPL,
	[OP_CP2_DPCT] = gteDPCT,
	[OP_CP2_AVSZ3] = gteAVSZ3,
	[OP_CP2_AVSZ4] = gteAVSZ4,
	[OP_CP2_RTPT] = gteRTPT,
	[OP_CP2_GPF] = gteGPF,
	[OP_CP2_GPL] = gteGPL,
	[OP_CP2_NCCT] = gteNCCT,
};

static u32 cop_mfc_cfc(struct lightrec_state *state, int cp, u8 reg, bool cfc)
{
	switch (cp) {
	case 0:
		return psxRegs.CP0.r[reg];
	case 2:
		if (cfc)
			return psxRegs.CP2C.r[reg];
		else
			return MFC2(reg);
	default:
		fprintf(stderr, "Access to non-existing CP %i\n", cp);
		return 0;
	}
}

static u32 cop_mfc(struct lightrec_state *state, int cp, u8 reg)
{
	return cop_mfc_cfc(state, cp, reg, false);
}

static u32 cop_cfc(struct lightrec_state *state, int cp, u8 reg)
{
	return cop_mfc_cfc(state, cp, reg, true);
}

static void cop_mtc_ctc(struct lightrec_state *state,
		int cp, u8 reg, u32 value, bool ctc)
{
	switch (cp) {
	case 0:
		switch (reg) {
		case 1:
		case 4:
		case 8:
		case 14:
		case 15:
			/* Those registers are read-only */
			break;
		case 12: /* Status */
			psxRegs.CP0.n.Status = value;
			state->block_exit_flags = LIGHTREC_EXIT_CHECK_INTERRUPT;
			break;
		case 13: /* Cause */
			psxRegs.CP0.n.Cause &= ~0x0300;
			psxRegs.CP0.n.Cause |= value & 0x0300;
			state->block_exit_flags = LIGHTREC_EXIT_CHECK_INTERRUPT;
			break;
		default:
			psxRegs.CP0.r[reg] = value;
			break;
		}
		break;
	case 2:
		if (ctc)
			CTC2(value, reg);
		else
			MTC2(value, reg);
		break;
	default:
		break;
	}
}

static void cop_mtc(struct lightrec_state *state, int cp, u8 reg, u32 value)
{
	cop_mtc_ctc(state, cp, reg, value, false);
}

static void cop_ctc(struct lightrec_state *state, int cp, u8 reg, u32 value)
{
	cop_mtc_ctc(state, cp, reg, value, true);
}

static void cop_op(struct lightrec_state *state, int cp, u32 func)
{
	psxRegs.code = func;

	if (unlikely(cp != 2))
		fprintf(stderr, "Access to invalid co-processor %i\n", cp);
	else if (unlikely(!cp2_ops[func & 0x3f]))
		fprintf(stderr, "Invalid CP2 function %u\n", func);
	else
		cp2_ops[func & 0x3f](&psxRegs.CP2);
}

static const struct lightrec_cop_ops cop_ops = {
	.mfc = cop_mfc,
	.cfc = cop_cfc,
	.mtc = cop_mtc,
	.ctc = cop_ctc,
	.op = cop_op,
};

static void hw_write_byte(struct lightrec_state *state,
		const struct opcode *op, u32 mem, u8 val)
{
	psxRegs.cycle = state->current_cycle +
		lightrec_cycles_of_block(state->current, op);

	psxHwWrite8(mem, val);
	state->block_exit_flags = LIGHTREC_EXIT_CHECK_INTERRUPT;
}

static void hw_write_half(struct lightrec_state *state,
		const struct opcode *op, u32 mem, u16 val)
{
	psxRegs.cycle = state->current_cycle +
		lightrec_cycles_of_block(state->current, op);

	psxHwWrite16(mem, val);
	state->block_exit_flags = LIGHTREC_EXIT_CHECK_INTERRUPT;
}

static void hw_write_word(struct lightrec_state *state,
		const struct opcode *op, u32 mem, u32 val)
{
	psxRegs.cycle = state->current_cycle +
		lightrec_cycles_of_block(state->current, op);

	psxHwWrite32(mem, val);
	state->block_exit_flags = LIGHTREC_EXIT_CHECK_INTERRUPT;
}

static u8 hw_read_byte(struct lightrec_state *state,
		const struct opcode *op, u32 mem)
{
	psxRegs.cycle = state->current_cycle +
		lightrec_cycles_of_block(state->current, op);

	state->block_exit_flags = LIGHTREC_EXIT_CHECK_INTERRUPT;
	return psxHwRead8(mem);
}

static u16 hw_read_half(struct lightrec_state *state,
		const struct opcode *op, u32 mem)
{
	psxRegs.cycle = state->current_cycle +
		lightrec_cycles_of_block(state->current, op);

	state->block_exit_flags = LIGHTREC_EXIT_CHECK_INTERRUPT;
	return psxHwRead16(mem);
}

static u32 hw_read_word(struct lightrec_state *state,
		const struct opcode *op, u32 mem)
{
	psxRegs.cycle = state->current_cycle +
		lightrec_cycles_of_block(state->current, op);

	state->block_exit_flags = LIGHTREC_EXIT_CHECK_INTERRUPT;
	return psxHwRead32(mem);
}

static struct lightrec_mem_map_ops hw_regs_ops = {
	.sb = hw_write_byte,
	.sh = hw_write_half,
	.sw = hw_write_word,
	.lb = hw_read_byte,
	.lh = hw_read_half,
	.lw = hw_read_word,
};

enum psx_map {
	PSX_MAP_KERNEL_USER_RAM,
	PSX_MAP_BIOS,
	PSX_MAP_SCRATCH_PAD,
	PSX_MAP_PARALLEL_PORT,
	PSX_MAP_HW_REGISTERS,
	PSX_MAP_CACHE_CONTROL,
};

static struct lightrec_mem_map lightrec_map[] = {
	[PSX_MAP_KERNEL_USER_RAM] = {
		/* Kernel and user memory */
		.pc = 0x00000000,
		.length = 0x200000,
		.flags = MAP_IS_RWX,
	},
	[PSX_MAP_BIOS] = {
		/* BIOS */
		.pc = 0x1fc00000,
		.length = 0x80000,
	},
	[PSX_MAP_SCRATCH_PAD] = {
		/* Scratch pad */
		.pc = 0x1f800000,
		.length = 0x400,
	},
	[PSX_MAP_PARALLEL_PORT] = {
		/* Parallel port */
		.pc = 0x1f000000,
		.length = 0x10000,
	},
	[PSX_MAP_HW_REGISTERS] = {
		/* Hardware registers */
		.pc = 0x1f801000,
		.length = 0x2000,
		.ops = &hw_regs_ops,
	},
	[PSX_MAP_CACHE_CONTROL] = {
		/* Cache control */
		.pc = 0x5ffe0000,
		.length = sizeof(cache_control),
		.address = &cache_control,
	},
};

static int lightrec_plugin_init(void)
{
	lightrec_map[PSX_MAP_KERNEL_USER_RAM].address = psxM;
	lightrec_map[PSX_MAP_BIOS].address = psxR;
	lightrec_map[PSX_MAP_SCRATCH_PAD].address = psxH;
	lightrec_map[PSX_MAP_PARALLEL_PORT].address = psxM + 0x200000;

	lightrec_state = lightrec_init(name,
			lightrec_map, ARRAY_SIZE(lightrec_map),
			&cop_ops);

	/* At some point, the BIOS disables the writes to the RAM - every
	 * SB/SH/SW/etc pointing to the RAM won't have any effect.
	 * Since Lightrec does not emulate that, we just hack the BIOS here to
	 * jump above that code. */
	memset((void *) 0x1fc00250, 0, 0x28);
	memset((void *) 0x1fc002a0, 0, 0x88);
	*(u32 *) 0x1fc00320 = 0x240a1000;
	*(u32 *) 0x1fc00324 = 0x240b0f80;

	memset((void *) 0x1fc01960, 0, 0x28);
	memset((void *) 0x1fc019b0, 0, 0x88);
	*(u32 *) 0x1fc01a30 = 0x240a1000;
	*(u32 *) 0x1fc01a34 = 0x240b0f80;

	fprintf(stderr, "M=0x%lx, P=0x%lx, R=0x%lx, H=0x%lx\n",
			(uintptr_t) psxM,
			(uintptr_t) psxP,
			(uintptr_t) psxR,
			(uintptr_t) psxH);

	signal(SIGPIPE, exit);

	/* For now, exit Lightrec after each trace. */
	lightrec_state->stop = 1;

	return 0;
}

static void lightrec_plugin_execute_block(void)
{
	memcpy(lightrec_state->native_reg_cache, psxRegs.GPR.r,
			sizeof(psxRegs.GPR.r));

	psxRegs.pc = lightrec_execute(lightrec_state, psxRegs.pc);
	psxRegs.cycle = lightrec_state->current_cycle;

	memcpy(psxRegs.GPR.r, lightrec_state->native_reg_cache,
			sizeof(psxRegs.GPR.r));

	if (lightrec_state->block_exit_flags == LIGHTREC_EXIT_SEGFAULT) {
		fprintf(stderr, "Exiting at cycle 0x%08x\n", psxRegs.cycle);
		exit(1);
	}

	if (lightrec_state->block_exit_flags == LIGHTREC_EXIT_SYSCALL)
		psxException(0x20, 0);

	psxBranchTest();

	if ((psxRegs.CP0.n.Cause & psxRegs.CP0.n.Status & 0x300) &&
			(psxRegs.CP0.n.Status & 0x1)) {
		/* Handle software interrupts */
		psxRegs.CP0.n.Cause &= ~0x7c;
		psxException(psxRegs.CP0.n.Cause, 0);
	}
}

static void lightrec_plugin_execute(void)
{
	extern int stop;

	while (!stop)
		lightrec_plugin_execute_block();
}

static void lightrec_plugin_clear(u32 addr, u32 size)
{
	/* size * 4: PCSX uses DMA units */
	lightrec_invalidate(lightrec_state, addr, size * 4);
}

static void lightrec_plugin_shutdown(void)
{
	lightrec_destroy(lightrec_state);
}

static void lightrec_plugin_reset(void)
{
	lightrec_plugin_shutdown();
	lightrec_plugin_init();
}

R3000Acpu psxRec =
{
	lightrec_plugin_init,
	lightrec_plugin_reset,
	lightrec_plugin_execute,
	lightrec_plugin_execute_block,
	lightrec_plugin_clear,
	lightrec_plugin_shutdown,
};
