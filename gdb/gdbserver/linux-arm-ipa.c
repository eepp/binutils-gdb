/* GNU/Linux/arm specific low level interface, for the in-process
   agent library for GDB.

   Copyright (C) 2015 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "server.h"
#include <stdint.h>
#include <sys/mman.h>
#include "tracepoint.h"
#include <sys/auxv.h>

/* ARM GNU/Linux HWCAP values.  These are in defined in
   <asm/elf.h> in current kernels.  */
#define HWCAP_VFP       64
#define HWCAP_IWMMXT    512
#define HWCAP_NEON      4096
#define HWCAP_VFPv3     8192
#define HWCAP_VFPv3D16  16384

/* Defined in auto-generated file regs-arm.c.  */
void init_registers_arm (void);
extern const struct target_desc *tdesc_arm;

void init_registers_arm_with_vfpv2 (void);
extern const struct target_desc *tdesc_arm_with_vfpv2;

void init_registers_arm_with_vfpv3 (void);
extern const struct target_desc *tdesc_arm_with_vfpv3;

void init_registers_arm_with_neon (void);
extern const struct target_desc *tdesc_arm_with_neon;

/* 32 bits GPR registers.  */
#define GPR_SIZE 4
/* 64 bits FPR registers.  */
#define FPR_SIZE 8

/* Special registers mappings.  */
#define FT_CR_PC	0
#define FT_CR_CPSR	1 * GPR_SIZE
#define FT_CR_LR	15 * GPR_SIZE
#define FT_CR_GPR_0	2 * GPR_SIZE
#define FT_CR_FPR_0	FT_CR_LR + GPR_SIZE
#define FT_CR_GPR(n)	(FT_CR_GPR_0 + (n * GPR_SIZE))
#define FT_CR_FPR(n)	(FT_CR_FPR_0 + (n * FPR_SIZE))
#define FT_CR_UNAVAIL	-1

/* Mapping between registers collected by the jump pad and GDB's register
   array layout used by regcache for arm core registers.

   See linux-arm-low.c (arm_install_fast_tracepoint_jump_pad) for
   more details.  */

static const int arm_core_ft_collect_regmap[] = {
  FT_CR_GPR (0),
  FT_CR_GPR (1),
  FT_CR_GPR (2),
  FT_CR_GPR (3),
  FT_CR_GPR (4),
  FT_CR_GPR (5),
  FT_CR_GPR (6),
  FT_CR_GPR (7),
  FT_CR_GPR (8),
  FT_CR_GPR (9),
  FT_CR_GPR (10),
  FT_CR_GPR (11),
  FT_CR_GPR (12),
  /* SP is calculated rather than collected.  */
  FT_CR_UNAVAIL,
  FT_CR_LR,
  FT_CR_PC,
  /* Legacy FPA Registers. 16 to 24.  */
  FT_CR_UNAVAIL,
  FT_CR_UNAVAIL,
  FT_CR_UNAVAIL,
  FT_CR_UNAVAIL,
  FT_CR_UNAVAIL,
  FT_CR_UNAVAIL,
  FT_CR_UNAVAIL,
  FT_CR_UNAVAIL,
  FT_CR_UNAVAIL,
  FT_CR_CPSR,
};

/* Mapping for VFPv2 registers.  */
static const int arm_vfpv2_ft_collect_regmap[] = {
  FT_CR_FPR (0),
  FT_CR_FPR (1),
  FT_CR_FPR (2),
  FT_CR_FPR (3),
  FT_CR_FPR (4),
  FT_CR_FPR (5),
  FT_CR_FPR (6),
  FT_CR_FPR (7),
  FT_CR_FPR (8),
  FT_CR_FPR (9),
  FT_CR_FPR (10),
  FT_CR_FPR (11),
  FT_CR_FPR (12),
  FT_CR_FPR (13),
  FT_CR_FPR (14),
  FT_CR_FPR (15),
};

/* Mapping for VFPv3 registers.  */
static const int arm_vfpv3_ft_collect_regmap[] = {
  FT_CR_FPR (0),
  FT_CR_FPR (1),
  FT_CR_FPR (2),
  FT_CR_FPR (3),
  FT_CR_FPR (4),
  FT_CR_FPR (5),
  FT_CR_FPR (6),
  FT_CR_FPR (7),
  FT_CR_FPR (8),
  FT_CR_FPR (9),
  FT_CR_FPR (10),
  FT_CR_FPR (11),
  FT_CR_FPR (12),
  FT_CR_FPR (13),
  FT_CR_FPR (14),
  FT_CR_FPR (15),
  FT_CR_FPR (16),
  FT_CR_FPR (17),
  FT_CR_FPR (18),
  FT_CR_FPR (19),
  FT_CR_FPR (20),
  FT_CR_FPR (21),
  FT_CR_FPR (22),
  FT_CR_FPR (23),
  FT_CR_FPR (24),
  FT_CR_FPR (25),
  FT_CR_FPR (26),
  FT_CR_FPR (27),
  FT_CR_FPR (28),
  FT_CR_FPR (29),
  FT_CR_FPR (30),
  FT_CR_FPR (31),
};

#define ARM_CORE_NUM_FT_COLLECT_REGS \
  (sizeof(arm_core_ft_collect_regmap) / sizeof(arm_core_ft_collect_regmap[0]))

#define ARM_VFPV2_NUM_FT_COLLECT_REGS \
  (sizeof(arm_vfpv2_ft_collect_regmap) / sizeof(arm_vfpv2_ft_collect_regmap[0]))

#define ARM_VFPV3_NUM_FT_COLLECT_REGS \
  (sizeof(arm_vfpv3_ft_collect_regmap) / sizeof(arm_vfpv3_ft_collect_regmap[0]))

void
supply_fast_tracepoint_registers (struct regcache *regcache,
				  const unsigned char *buf)
{
  int i;
  uint32_t val = 0;
  /* Number of extention registers collected.  */
  int num_ext_regs = 0;

  for (i = 0; i < ARM_CORE_NUM_FT_COLLECT_REGS; i++)
    {
      int index = arm_core_ft_collect_regmap[i];
      if (index != FT_CR_UNAVAIL)
	supply_register (regcache, i,
			 (char *) buf + arm_core_ft_collect_regmap[i]);
    }

  if (ipa_tdesc == tdesc_arm_with_neon || ipa_tdesc == tdesc_arm_with_vfpv3)
    {
      num_ext_regs = ARM_VFPV3_NUM_FT_COLLECT_REGS;

      for (i = 0; i < ARM_VFPV3_NUM_FT_COLLECT_REGS; i++)
	supply_register (regcache, i + ARM_CORE_NUM_FT_COLLECT_REGS,
			 (char *) buf + arm_vfpv3_ft_collect_regmap[i]);
    }
  else if (ipa_tdesc == tdesc_arm_with_vfpv2)
    {
      num_ext_regs = ARM_VFPV2_NUM_FT_COLLECT_REGS;

      for (i = 0; i < ARM_VFPV2_NUM_FT_COLLECT_REGS; i++)
	supply_register (regcache, i + ARM_CORE_NUM_FT_COLLECT_REGS,
			 (char *) buf + arm_vfpv2_ft_collect_regmap[i]);
    }

  /* SP calculation from stack layout.  */
  val = (uint32_t) buf + 16 * 4 + num_ext_regs * 8;
  supply_register (regcache, 13, &val);
}

IP_AGENT_EXPORT_FUNC ULONGEST
gdb_agent_get_raw_reg (const unsigned char *raw_regs, int regnum)
{
  /* only for jit  */
  return 0;
}

const char *gdbserver_xmltarget;

static void
arm_ipa_get_hwcap (unsigned long *valp)
{
#ifdef HAVE_GETAUXVAL
  *valp = getauxval (AT_HWCAP);
#else
  unsigned long data[2];
  FILE *f = fopen ("/proc/self/auxv", "r");

  if (f == NULL)
    return;

  while (fread (data, sizeof (data), 1, f) > 0)
    {
      if (data[0] == AT_HWCAP)
	{
	  *valp = data[1];
	  break;
	}
    }

  fclose (f);
#endif /* HAVE_GETAUXVAL */
}
const struct target_desc *
arm_ipa_read_hwcap (void)
{
  unsigned long arm_hwcap = 0;

  arm_ipa_get_hwcap (&arm_hwcap);

  if (arm_hwcap == 0)
    return tdesc_arm;

  /* iwmmxt registers collection is not supported.  */
  if (arm_hwcap & HWCAP_IWMMXT)
    return tdesc_arm;

  if (arm_hwcap & HWCAP_VFP)
    {
      const struct target_desc *result;

      /* NEON implies either no VFP, or VFPv3-D32.  We only support
	 it with VFP.  */
      if (arm_hwcap & HWCAP_NEON)
	result = tdesc_arm_with_neon;
      else if ((arm_hwcap & (HWCAP_VFPv3 | HWCAP_VFPv3D16)) == HWCAP_VFPv3)
	result = tdesc_arm_with_vfpv3;
      else
	result = tdesc_arm_with_vfpv2;

      return result;
    }

  /* The default configuration uses legacy FPA registers, probably
     simulated.  */
  return tdesc_arm;
}

void
initialize_low_tracepoint (void)
{
  /* Initialize the Linux target descriptions.  */
  init_registers_arm ();
  init_registers_arm_with_vfpv2 ();
  init_registers_arm_with_vfpv3 ();
  init_registers_arm_with_neon ();

  ipa_tdesc = arm_ipa_read_hwcap ();
}
