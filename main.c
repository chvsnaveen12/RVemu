/*
Didnt implement fence and zifenci
*/
#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>

#define NUM_CSRS 4096

const uint64_t DRAM_SIZE = 1024*1024*128;
const uint64_t DRAM_BASE = 0x80000000;
const uint64_t DRAM_END = DRAM_BASE+DRAM_SIZE-1;
uint8_t *DRAM;
uint8_t user = 0b00;
uint8_t supervisor = 0b01;
uint8_t machine = 0b11;
char temp;

struct CPU {
	uint64_t regs[32];
	uint64_t pc;
	uint64_t CSR[NUM_CSRS];
	uint8_t privilege;
} CPU;

// MISC & DECLARATIONS==============================================================================

void error(char* errormsg){
	printf("%s\n", errormsg);
	free(DRAM);
	exit(0);
}

uint64_t bus_load(uint64_t address, uint8_t size);
void bus_store(uint64_t address, uint8_t size, uint64_t value);
void dram_init(char* file_name);
uint64_t dram_load(uint64_t address, uint8_t size);
void dram_store(uint64_t address, uint8_t size, uint64_t value);
void cpu_init();
uint32_t cpu_fetch();
void cpu_execute(uint32_t instruction);
void cpu_reg_dump(uint32_t instruction);
uint64_t cpu_load_csr(uint16_t address);
void cpu_store_csr(uint16_t address, uint64_t value);
void cpu_csr_dump();
// =================================================================================================

// CSRs=============================================================================================

const uint64_t mhartid = 0xf14;
const uint64_t mstatus = 0x300;
const uint64_t medeleg = 0x302;
const uint64_t mideleg = 0x303;
const uint64_t mie = 0x304;
const uint64_t mtvec = 0x305;
const uint64_t mcounteren = 0x306;
const uint64_t mscratch = 0x340;
const uint64_t mepc = 0x341;
const uint64_t mcause = 0x342;
const uint64_t mtval = 0x343;
const uint64_t mip = 0x344;

const uint64_t sstatus = 0x100;
const uint64_t sie = 0x104;
const uint64_t stvec = 0x105;
const uint64_t sscratch = 0x140;
const uint64_t sepc = 0x141;
const uint64_t scause = 0x142;
const uint64_t stval = 0x143;
const uint64_t sip = 0x144;
const uint64_t satp = 0x180;

const uint64_t 	mask_sie = 1UL << 1;
const uint64_t 	mask_mie = 1UL << 3;
const uint64_t 	mask_spie = 1UL << 5;
const uint64_t 	mask_ube = 1UL << 6;
const uint64_t 	mask_mpie = 1UL << 7;
const uint64_t 	mask_spp = 1UL << 8;
const uint64_t 	mask_vs = 0b11UL << 9;
const uint64_t 	mask_mpp = 0b11UL << 11;
const uint64_t 	mask_fs = 0b11UL << 13;
const uint64_t 	mask_xs = 0b11UL << 15;
const uint64_t 	mask_mprv = 1UL << 17;
const uint64_t 	mask_sum = 1UL << 18;
const uint64_t 	mask_mxr = 1UL << 19;
const uint64_t 	mask_tvm = 1UL << 20;
const uint64_t 	mask_tw = 1UL << 21;
const uint64_t 	mask_tsr = 1UL << 22;
const uint64_t 	mask_uxl = 0b11UL << 32;
const uint64_t 	mask_sxl = 0b11UL << 34;
const uint64_t 	mask_sbe = 1UL << 36;
const uint64_t 	mask_mbe = 1UL << 37;
const uint64_t 	mask_sd = 1UL << 63;

const uint64_t mask_sstatus = mask_sie | mask_spie | mask_ube | mask_spp | mask_fs
							| mask_xs | mask_sum | mask_mxr | mask_uxl | mask_sd;

const uint64_t 	mask_ssip = 1UL << 1;
const uint64_t 	mask_msip = 1UL << 3;
const uint64_t 	mask_stip = 1UL << 5;
const uint64_t 	mask_mtip = 1UL << 7;
const uint64_t 	mask_seip = 1UL << 9;
const uint64_t 	mask_meip = 1UL << 11;


// =================================================================================================

// BUS==============================================================================================
uint64_t bus_load(uint64_t address, uint8_t size){
	if(address >= DRAM_BASE)
		return dram_load(address-DRAM_BASE, size);
	else{
		error("Bus load address error");
		return 0;
	}
}

void bus_store(uint64_t address, uint8_t size, uint64_t value){
	if(address >= DRAM_BASE)
		return dram_store(address-DRAM_BASE, size, value);
	else
		error("Bus store address error");
}
// =================================================================================================


// DRAM=============================================================================================

void dram_init(char* file_name){
	FILE *fptr;
		fptr = fopen(file_name, "rb");
		if(fptr == NULL)
				error("File doesn't exist");
		
		fseek(fptr, 0, SEEK_END);
		uint64_t file_length = ftell(fptr);
		fseek(fptr, 0, SEEK_SET);
		
	DRAM = (uint8_t*)malloc(DRAM_SIZE);
		fread(DRAM, 1, file_length, fptr);
		fclose(fptr);
}

uint64_t dram_load(uint64_t address, uint8_t size){
	uint64_t val;
	switch(size){
		case 8:
			val = ((uint64_t)DRAM[address+0] << 0);
			break;
		case 16:
			val = ((uint64_t)DRAM[address+0] << 0) 
				| ((uint64_t)DRAM[address+1] << 8);
			break;
		case 32:
			val = ((uint64_t)DRAM[address+0] << 0)
				| ((uint64_t)DRAM[address+1] << 8)
				| ((uint64_t)DRAM[address+2] << 16)
				| ((uint64_t)DRAM[address+3] << 24);
			break;
		case 64:
			val = ((uint64_t)DRAM[address+0] << 0)
				| ((uint64_t)DRAM[address+1] << 8)
				| ((uint64_t)DRAM[address+2] << 16)
				| ((uint64_t)DRAM[address+3] << 24)
				| ((uint64_t)DRAM[address+4] << 32)
				| ((uint64_t)DRAM[address+5] << 40)
				| ((uint64_t)DRAM[address+6] << 48)
				| ((uint64_t)DRAM[address+7] << 56);
			break;
		default:
			error("Invalid dram load length");
			break;
	}
	return val;
}

void dram_store(uint64_t address, uint8_t size, uint64_t value){
	switch (size){
	case 8:
		DRAM[address+0] = (uint8_t)(value >> 0);
		break;
	
	case 16:
		DRAM[address+0] = (uint8_t)(value >> 0);
		DRAM[address+1] = (uint8_t)(value >> 8);
		break;

	case 32:
		DRAM[address+0] = (uint8_t)(value >> 0);
		DRAM[address+1] = (uint8_t)(value >> 8);
		DRAM[address+2] = (uint8_t)(value >> 16);
		DRAM[address+3] = (uint8_t)(value >> 24);
		break;

	case 64:
		DRAM[address+0] = (uint8_t)(value >> 0);
		DRAM[address+1] = (uint8_t)(value >> 8);
		DRAM[address+2] = (uint8_t)(value >> 16);
		DRAM[address+3] = (uint8_t)(value >> 24);
		DRAM[address+4] = (uint8_t)(value >> 32);
		DRAM[address+5] = (uint8_t)(value >> 40);
		DRAM[address+6] = (uint8_t)(value >> 48);
		DRAM[address+7] = (uint8_t)(value >> 56);
		break;

	default:
		error("Invalid dram store length");
		break;
	}
}

// =================================================================================================

// CPU==============================================================================================

void cpu_init(){
	for(int i = 0; i<32; i++)
		CPU.regs[i] = 0;
	CPU.regs[2] = DRAM_END;
	CPU.pc = DRAM_BASE;
	CPU.privilege = machine;
}

uint32_t cpu_fetch(){
	return bus_load(CPU.pc, 32);
}

uint64_t cpu_load_csr(uint16_t address){
	if (address == sie)
		return CPU.CSR[mie] & CPU.CSR[mideleg];
	
	else if (address == sip)
		return CPU.CSR[mip] & CPU.CSR[mideleg];
	
	else if (address == sstatus)
		return CPU.CSR[mstatus] & mask_sstatus;

	else
		return CPU.CSR[address];
}

void cpu_store_csr(uint16_t address, uint64_t value){

	if (address == sie)
		CPU.CSR[mie] = (CPU.CSR[mie] & ~CPU.CSR[mideleg]) | (value & CPU.CSR[mideleg]);
	
	else if (address == sip)
		CPU.CSR[mip] = (CPU.CSR[mip] & ~CPU.CSR[mideleg]) | (value & CPU.CSR[mideleg]);
	
	else if (address == sstatus)
		CPU.CSR[mstatus] = (CPU.CSR[mstatus] & ~mask_sstatus) | (value & mask_sstatus);

	else
		CPU.CSR[address] = value;
}

void cpu_execute(uint32_t instruction){
	uint8_t funct3 = (uint8_t)(instruction >> 12) & 0x07;
	uint8_t opcode = (uint8_t)instruction & 0b01111111;
	uint8_t rd = (uint8_t)(instruction >> 7) & 0b00011111;
	uint8_t rs1 = (uint8_t)(instruction >> 15) & 0b00011111;
	uint8_t rs2 = (uint8_t)(instruction >> 20) & 0b00011111;

	// ALL ARE FOR RV32I UNLESS MENTIONED
	if(opcode == 0x37){
		// LUI
		uint64_t imm = ((uint64_t)instruction) & 0x00000000fffff000;
		imm = (imm & 0x0000000080000000) >> 31 ? (imm | 0xffffffff00000000) : imm;
		CPU.regs[rd] = imm;
	}
	
	else if(opcode == 0x17){
		// AUIPC
		uint64_t imm = ((uint64_t)instruction) & 0x00000000fffff000;
		imm = (imm & 0x0000000080000000) >> 31 ? (imm | 0xffffffff00000000) : imm;
		CPU.regs[rd] = CPU.pc + imm;
	}

	else if(opcode == 0x6f){
		// JAL
		uint64_t imm =    (((uint64_t)instruction >> 20) & 0x00000000000007fe)
						| (((uint64_t)instruction >>  9) & 0x0000000000000800)
						| (((uint64_t)instruction)       & 0x00000000000ff000)
						| (((uint64_t)instruction >> 11) & 0x0000000000100000);
		imm = (imm & 0x0000000000100000) >> 20 ? (imm | 0xffffffffffe00000) : imm;
		CPU.regs[rd] = CPU.pc + 4;
		CPU.pc = CPU.pc + imm - 4;
	}

	else if(opcode == 0x67){
		// JALR
		uint64_t imm = ((uint64_t)instruction >> 20) & 0x0000000000000fff;
		imm = (imm & 0x0000000000000800) >> 11 ? (imm | 0xfffffffffffff000) : imm;

		uint64_t temp = CPU.pc+4;
		CPU.pc = ((CPU.regs[rs1] + imm) & 0xfffffffffffffffe);
		CPU.regs[rd] = temp;
	}

	else if(opcode == 0x63){
		// BEQ BNE BLT BGE BLTU BGEU
		uint64_t imm = (((uint64_t)instruction >>  7) & 0x000000000000001e)
						| (((uint64_t)instruction >> 10) & 0x00000000000007e0)
						| (((uint64_t)instruction <<  4) & 0x0000000000000800)
						| (((uint64_t)instruction >> 19) & 0x0000000000001000);
		imm = (imm & 0x0000000000001000) >> 12 ? (imm | 0xffffffffffffe000) : imm;

		if(funct3 == 0){
			// BEQ
			if(CPU.regs[rs1] == CPU.regs[rs2])
				CPU.pc = CPU.pc + imm - 4;
		}
		
		else if(funct3 == 1){
			// BNE
			if(CPU.regs[rs1] != CPU.regs[rs2])
				CPU.pc = CPU.pc + imm - 4;
		}

		else if(funct3 == 4){
			// BLT
			if((int64_t)CPU.regs[rs1] < (int64_t)CPU.regs[rs2])
				CPU.pc = CPU.pc + imm - 4;
		}

		else if(funct3 == 5){
			// BGE
			if((int64_t)CPU.regs[rs1] > (int64_t)CPU.regs[rs2])
				CPU.pc = CPU.pc + imm - 4;
		}

		else if(funct3 == 6)
			// BLTU
			if((uint64_t)CPU.regs[rs1] < (uint64_t)CPU.regs[rs2])
				CPU.pc = CPU.pc + imm - 4;

		else if(funct3 == 7)
			// BGEU
			if((uint64_t)CPU.regs[rs1] > (uint64_t)CPU.regs[rs2])
				CPU.pc = CPU.pc + imm - 4;
	}

	else if(opcode == 0x03){
		// LB LH LW LBU LHU (RV64I => LWU LD)
		uint64_t imm = (instruction >> 20) & 0x0000000000000fff;
		imm = (imm & 0x0000000000001000) >> 11 ? (imm | 0xfffffffffffff000) : imm;

		if(funct3 == 0){
			// LB
			uint64_t val = bus_load(CPU.regs[rs1] + imm, 8);
			CPU.regs[rd] = (val & 0x0000000000000080) >> 7 ? (val | 0xffffffffffffff00) : val;     
		}

		else if(funct3 == 1){
			// LH
			uint64_t val = bus_load(CPU.regs[rs1] + imm, 16);
			CPU.regs[rd] = (val & 0x0000000000008000) >> 15 ? (val | 0xffffffffffff0000) : val;     
		}

		else if(funct3 == 2){
			// LW
			uint64_t val = bus_load(CPU.regs[rs1] + imm, 32);
			CPU.regs[rd] = (val & 0x0000000080000000) >> 31 ? (val | 0xffffffff00000000) : val;     
		}

		else if(funct3 == 4)
			// LBU
			CPU.regs[rd] = bus_load(CPU.regs[rs1] + imm, 8);

		else if(funct3 == 5)
			// LHU
			CPU.regs[rd] = bus_load(CPU.regs[rs1] + imm, 16);
		
		else if(funct3 == 6)
			// LWU
			CPU.regs[rd] = bus_load(CPU.regs[rs1] + imm, 32);

		else if(funct3 == 3)
			// LD
			CPU.regs[rd] = bus_load(CPU.regs[rs1] + imm, 64);

	}

	else if(opcode == 0x23){
		// SB SH SW (RV64I => SD)
		uint64_t imm = ((instruction >> 7) & 0x000000000000001f)
						| ((instruction >> 20) & 0x0000000000000fe0);
		imm = (imm & 0x0000000000000800) >> 11 ? (imm | 0xfffffffffffff000) : imm;

		if(funct3 == 0)
			// SB
			bus_store(CPU.regs[rs1] + imm, 8, rs2);
		
		if(funct3 == 1)
			// SH
			bus_store(CPU.regs[rs1] + imm, 16, rs2);
		
		if(funct3 == 2)
			// SW
			bus_store(CPU.regs[rs1] + imm, 32, rs2);
		
		if(funct3 == 3)
			// SD
			bus_store(CPU.regs[rs1] + imm, 64, rs2);

	}

	else if(opcode == 0x13){
		// ADDI SLTI SLTIU XORI ORI ANDI
		// (RV64I => SLLI SRLI SRAI)
		uint64_t imm = (instruction >> 20) & 0x0000000000000fff;
		imm = (imm & 0x0000000000000800) >> 11 ? (imm | 0xfffffffffffff000) : imm;

		if(funct3 == 0)
			// ADDI
			CPU.regs[rd] = CPU.regs[rs1] + imm;

		else if(funct3 == 2)
			// SLTI
			CPU.regs[rd] = (int64_t)CPU.regs[rs1] < (int64_t)imm ? 1 : 0;

		else if(funct3 == 3)
			// SLTIU
			CPU.regs[rd] = (uint64_t)CPU.regs[rs1] < (uint64_t)imm ? 1 : 0;

		else if(funct3 == 4)
			// XORI
			CPU.regs[rd] = CPU.regs[rs1] ^ imm;

		else if(funct3 == 6)
			// ORI
			CPU.regs[rd] = CPU.regs[rs1] | imm;
		
		else if(funct3 == 7)
			// ANDI
			CPU.regs[rd] = CPU.regs[rs1] & imm;
		
		else if(funct3 == 1)
			//  SLLI
			CPU.regs[rd] = CPU.regs[rs1] << ((uint8_t)imm & 0x2f);

		else if(funct3 == 5){
			//  SRLI SRAI
			if((instruction >> 30) & 1)
				// SRAI
				CPU.regs[rd] = (CPU.regs[rs1] >> ((uint8_t)imm & 0x3f)) | (CPU.regs[rs1] & 0x8000000000000000);
			else
				// SRLI
				CPU.regs[rd] = (CPU.regs[rs1] >> ((uint8_t)imm & 0x3f));
		}
	}
	
	else if(opcode == 0x33){
		// ADD SUB SLL SLT SLTU XOR SRL SRA OR AND
		if(funct3 == 0)
			// ADD SUB
			CPU.regs[rd] = (instruction >> 30) & 1 ? CPU.regs[rs1] - CPU.regs[rs2] : CPU.regs[rs1] + CPU.regs[rs2];
		
		else if(funct3 == 1)
			// SLL
			CPU.regs[rd] = CPU.regs[rs1] << (CPU.regs[rs2] & 0x1f);
		
		else if(funct3 == 2)
			// SLT
			CPU.regs[rd] = (int64_t)CPU.regs[rs1] < (int64_t)CPU.regs[rs2] ? 1 : 0;
		
		else if(funct3 == 3)
			// SLTU
			CPU.regs[rd] = (uint64_t)CPU.regs[rs1] < (uint64_t)CPU.regs[rs2] ? 1 : 0;
		
		else if(funct3 == 4)
			// XOR
			CPU.regs[rd] = CPU.regs[rs1] ^ CPU.regs[rs2];
		
		else if(funct3 == 5){
			// SRA SRL
			uint64_t result = CPU.regs[rs1] >> (CPU.regs[rs2] & 0x1f);
			result |= (instruction >> 30) & 1 ? (CPU.regs[rs1] & 0x8000000000000000) : 0;
			CPU.regs[rd] = result;
		}

		else if(funct3 == 6)
			// AND
			CPU.regs[rd] = CPU.regs[rs1] & CPU.regs[rs2];

		else if(funct3 == 7)
			// OR
			CPU.regs[rd] = CPU.regs[rs1] | CPU.regs[rs2];
	}

	else if(opcode == 0x0f){
	    // FENCE
	}

	else if(opcode == 0x73){
		// ECALL EBREAK SRET MRET SFENCE.VMA CSRRW CSRRS  CSRRC CSRRWI CSRRSI CSRRCI
		uint16_t csr = (instruction >> 20) & 0x0000000000000fff;

		if (funct3 == 0){
			// ECALL EBREAK SRET MRET SFENCE.VMA
			uint8_t funct7 = (instruction >> 25) & 0x1f;
			if(rs2 == 0x2 && funct7 == 0x08){
				// SRET
				uint64_t sstatus_val = cpu_load_csr(sstatus);
				CPU.privilege = (uint8_t)((sstatus_val & mask_spp) >> 8);
				uint64_t spie_val = (sstatus_val & mask_spie) >> 5;
				sstatus_val = (sstatus_val & ~mask_sie) | (spie_val << 1);
				sstatus_val |= mask_spie;
				sstatus_val &= ~mask_spp;
				cpu_store_csr(sstatus, sstatus_val);
				CPU.pc = (cpu_load_csr(sepc) & ~0b11) - 4;
			}

			else if(rs2 == 0x2 && funct7 == 0x18){
				// MRET
				uint64_t mstatus_val = cpu_load_csr(mstatus);
				CPU.privilege = (uint8_t)((mstatus_val & mask_mpp) >> 11);
				uint64_t mpie_val = (mstatus_val & mask_mpie) >> 7;
				mstatus_val = (mstatus_val & ~mask_mpie) | (mpie_val << 3);
				mstatus_val = mask_mpie;
				mstatus_val &= ~mask_mpp;
				mstatus_val = ~mask_mprv;
				cpu_store_csr(mstatus, mstatus_val);
				CPU.pc = (cpu_load_csr(mepc) & ~0b11) - 4;
			}

		}

		else if(funct3 == 1){
			// CSRRW
			uint64_t temp = cpu_load_csr(csr);
			cpu_store_csr(csr, CPU.regs[rs1]);
			CPU.regs[rd] = temp;
		}

		else if(funct3 == 2){
			// CSRRS
			uint64_t temp = cpu_load_csr(csr);
			cpu_store_csr(csr, temp | CPU.regs[rs1]);
			CPU.regs[rd] = temp;
		}
		
		else if(funct3 == 3){
			// CSRRC
			uint64_t temp = cpu_load_csr(csr);
			cpu_store_csr(csr, temp &  ~CPU.regs[rs1]);
			CPU.regs[rd] = temp;
		}
		
		else if(funct3 == 5){
			// CSRRWI
			CPU.regs[rd] = cpu_load_csr(csr);
			cpu_store_csr(csr, (uint64_t)rs1);
		}

		else if(funct3 == 6){
			// CSRRSI
			uint64_t temp = cpu_load_csr(csr);
			cpu_store_csr(csr, temp | (uint64_t)rs1);
			CPU.regs[rd] = temp;
		}

		else if(funct3 == 7){
			// CSRRCI
			uint64_t temp = cpu_load_csr(csr);
			cpu_store_csr(csr, temp & ~(uint64_t)rs1);
			CPU.regs[rd] = temp;
		}
	}

	else if(opcode == 0x1b){
		// (RV64I => ADDIW SLLIW SRLIW SRAIW)
		uint64_t result;
		if(funct3 == 0){
			// ADDIW
			uint64_t imm = (instruction >> 20) & 0x0000000000000fff;
			imm |= (imm >> 11) & 1 ? 0xfffffffffffff000 : 0;
			result = (uint32_t)(CPU.regs[rs1] + imm);
			CPU.regs[rd] = (result >> 31) & 1 ? (result | 0xffffffff00000000) : result;
		}

		else if(funct3 == 1){
			// SLLIW
			result = (uint32_t)(CPU.regs[rs1] << rs2);
			CPU.regs[rd] = (result >> 31) & 1 ? (result | 0xffffffff00000000) : result;
		}

		else if(funct3 == 5){
			// SRLIW SRAIW
			result = (uint32_t)CPU.regs[rs1] >> rs2;
			result |= (instruction >> 30) & 1 ? (CPU.regs[rs1] & 0x0000000080000000) : 0;
			CPU.regs[rd] = (result >> 31) & 1 ? (result | 0xffffffff00000000) : result;
		}
	}

	else if(opcode == 0x3b){
		// (RV64I => ADDW SUBW SLLW SRLW SRAW)
		uint64_t result;
		if(funct3 == 0){
			// ADDW SUBW
			result = (uint32_t)((instruction >> 30) & 1 ? CPU.regs[rs1] - CPU.regs[rs2] : CPU.regs[rs1] + CPU.regs[rs2]);
			CPU.regs[rd] = (result >> 31) & 1 ? (result | 0xffffffff00000000) : result;
		}

		else if(funct3 == 1){
			// SLLW
			result = (uint32_t)(CPU.regs[rs1] << (CPU.regs[rs2] & 0x1f));
			CPU.regs[rd] = (result >> 31) & 1 ? (result | 0xffffffff00000000) : result;
		}

		else if(funct3 == 5){
			// SRLW SRAW
			result = (uint32_t)CPU.regs[rs1] >> (CPU.regs[rs2] & 0x1f);
			result |= (instruction >> 30) & 1 ? CPU.regs[rs1] & 0x0000000080000000 : 0;
			CPU.regs[rd] = (result >> 31) & 1 ? (result | 0xffffffff00000000) : result;
		}

	}

	// M extension instructions
	else if(opcode == 0x33){
		// MUL MULH MULHSU MULHU DIV DIVU REM REMU
		if(funct3 == 0)
			CPU.regs[rd] = (uint64_t)(CPU.regs[rs1]*CPU.regs[rs2]);

		else if(opcode == 5)
			CPU.regs[rd] = (uint64_t)(CPU.regs[rs1]/CPU.regs[rs2]);

		else if(opcode == 5)
			CPU.regs[rd] = (uint64_t)(CPU.regs[rs1]/CPU.regs[rs2]);
		
		else
			error("Illegal Instruction");
	}

	else if(opcode == 0x3b){
		// (RV64M => MULW DIVW DIVUW REMW REMUW)
		if(funct3 == 7)
			CPU.regs[rd] = (uint64_t)((uint32_t)CPU.regs[rs1]%(uint32_t)CPU.regs[rs2]);
		
		else
			error("Illegal Instruction");

	}
	
	// A extension
	else if(opcode == 0x2f){
		// LR.W SC.W AMOSWAP.W AMOADD.W AMOXOR.W AMOAND.W 
		// AMOOR.W AMOMIN.W AMOMAX.W AMOMINU.W AMOMAXU.W
		// (RV64A => LR.D SC.D AMOSWAP.D AMOADD.D AMOXOR.D AMOAND.D) 
		// (RV64A => AMOOR.D AMOMIN.D AMOMAX.D AMOMINU.D AMOMAXU.D)
	}

	else
		error("INVALID INSTRUCTION");

	CPU.regs[0] = 0;
}

void cpu_reg_dump(uint32_t instruction){
    printf("\e[1;1H\e[2J");
	printf("inst: %#010x\n", instruction);
    printf("PC: 0x%016lx\n\n", CPU.pc);

    for (int i = 0; i < 16; i++){
        printf("x%02d: 0x%016lx                x%02d: 0x%016lx\n", i, CPU.regs[i], i+16, CPU.regs[i+16]);
    }
	printf("\n");
}

void cpu_csr_dump(){
	printf("mhartid:    0x%016lx                mstatus:  0x%016lx\n", CPU.CSR[mhartid], CPU.CSR[mstatus]);
	printf("medeleg:    0x%016lx                mideleg:  0x%016lx\n", CPU.CSR[medeleg], CPU.CSR[mideleg]);
	printf("mie:        0x%016lx                mtvec:    0x%016lx\n", CPU.CSR[mie], CPU.CSR[mtvec]);
	printf("mcounteren: 0x%016lx                mscratch: 0x%016lx\n", CPU.CSR[mcounteren], CPU.CSR[mscratch]);
	printf("mepc:       0x%016lx                mcause:   0x%016lx\n", CPU.CSR[mepc], CPU.CSR[mcause]);
	printf("mtval:      0x%016lx                mip:      0x%016lx\n\n", CPU.CSR[mtval], CPU.CSR[mip]);

	printf("sstatus:    0x%016lx                sie:      0x%016lx\n", CPU.CSR[sstatus], CPU.CSR[sie]);
	printf("stvec:      0x%016lx                sscratch: 0x%016lx\n", CPU.CSR[stvec], CPU.CSR[sscratch]);
	printf("sepc:       0x%016lx                scause:   0x%016lx\n", CPU.CSR[sepc], CPU.CSR[scause]);
	printf("stval:      0x%016lx                sip:      0x%016lx\n", CPU.CSR[stval], CPU.CSR[sip]);
	printf("satp:       0x%016lx\n", CPU.CSR[satp]);
}
// =================================================================================================


int main(int argc, char *argv[]){
	if(argc != 2)
		error("File not given");
	dram_init(argv[1]);
	cpu_init();

	while(1){
		uint32_t instruction = cpu_fetch();
		cpu_execute(instruction);
		cpu_reg_dump(instruction);
		cpu_csr_dump();
		CPU.pc +=4;
		scanf("%c", &temp);
	}


	free(DRAM);
	return 0;
}
