/* Instruction decoding. */
#pragma once

static inline const char *reg_name(uint32_t reg)
{
	switch (reg) {
	case 0: return "zero";
	case 1: return "ra";
	case 2: return "sp";
	case 3: return "gp";
	case 4: return "tp";
	case 5: return "t0";
	case 6: return "t1";
	case 7: return "t2";
	case 8: return "s0/fp";
	case 9: return "s1";
	case 10: return "a0";
	case 11: return "a1";
	case 12: return "a2";
	case 13: return "a3";
	case 14: return "a4";
	case 15: return "a5";
	case 16: return "a6";
	case 17: return "a7";
	case 18: return "s2";
	case 19: return "s3";
	case 20: return "s4";
	case 21: return "s5";
	case 22: return "s6";
	case 23: return "s7";
	case 24: return "s8";
	case 25: return "s9";
	case 26: return "s10";
	case 27: return "s11";
	case 28: return "t3";
	case 29: return "t4";
	case 30: return "t5";
	case 31: return "t6";
	default: return "invalid";
	}
}

enum {
	OPC_LUI		= 0x37,
	OPC_AUIPC	= 0x17,
	OPC_JAL		= 0x6F,
	OPC_JALR	= 0x67,
	OPC_BRANCH	= 0x63,
	OPC_LOAD	= 0x03,
	OPC_STORE	= 0x23,
	OPC_REG_IMM	= 0x13,
	OPC_REG_REG	= 0x33,
	OPC_FENCE	= 0x0F,
	OPC_ENV		= 0x73,
};

static inline word_u instr_opcode(word_u i)
{
	i.u = i.u & 0x7F;
	return i;
}

static inline word_u instr_funct3(word_u i)
{
	i.u = (i.u >> 12) & 0x7;
	return i;
}

static inline word_u instr_funct7(word_u i)
{
	i.u = (i.u >> 25) & 0x7F;
	return i;
}

static inline uint8_t instr_rs1(word_u i)
{
	return (i.u >> 15) & 0x1F;
}

static inline uint8_t instr_rs2(word_u i)
{
	return (i.u >> 20) & 0x1F;
}

static inline uint8_t instr_rd(word_u i)
{
	return (i.u >> 7) & 0x1F;
}

static inline word_u instr_imm_itype(word_u i)
{
	i.s >>= 7 + 5 + 3 + 5;
	return i;
}

static inline word_u instr_imm_stype(word_u i)
{
	word_u out;
	out.u = (i.u >> 7) & 0x1F;

	i.s >>= 7 + 3 + 5 + 5;
	out.u |= i.u & ~0x1F;

	return out;
}

static inline word_u instr_imm_btype(word_u i)
{
	word_u out;

	i.u >>= 7;

	out.u = (i.u & 1) << 11;
	out.u |= i.u & 0x1E;
	
	i.u >>= 5 + 3 + 5;

	out.u |= i.u & 0x7E0;

	if (i.u & 0b100000000000)
		out.u |= ~ 0b11111111111;

	return out;
}

static inline word_u instr_imm_utype(word_u i)
{
	i.u = i.u & ~0xFFF;
	return i;
}

static inline word_u instr_imm_jtype(word_u i)
{
	word_u out;
	//            109876543210
	out.u = i.u &  0b10000000000000000000000000000000;
	//             00000000000100000000000000000000
	out.s >>= 11;
	out.u |= i.u & 0b00000000000011111111000000000000;
	//             00000000000111111111000000000000
	
	out.u |=(i.u & 0b00000000000100000000000000000000) >> 9;

	out.u |=(i.u & 0b01111111111000000000000000000000) >> 20;

	return out;
}

