#include "MIPS_Processor.hpp"

struct SevenNineStageWithBypass : SevenNineStage {
    SevenNineStageWithBypass(std::ifstream &file) : SevenNineStage(file) {}

    void executeIF1(PipelineRegisters::IF_ID & IF_IF) {
        if (PC >= commands.size()) {
            IF_IF.cmmnd.name = "nop";
            return;
        }
        IF_IF.cmmnd = commands[PC];
    }

    void executeIF2(PipelineRegisters::IF_ID & IF_IF, PipelineRegisters::IF_ID & IF_ID) {
        IF_ID = IF_IF;
    }

    void executeID1(PipelineRegisters::IF_ID & IF_ID, PipelineRegisters::ID_RR & ID_ID ) {
        if (IF_ID.cmmnd.name == "nop") {
            ID_ID.nop = true;
            return;
        }
        ID_ID.nop = false;
        ID_ID.r1 = IF_ID.cmmnd.r1;
        ID_ID.r2 = IF_ID.cmmnd.r2;
        ID_ID.imm = IF_ID.cmmnd.imm;
        ID_ID.reg_dst = (two_reg_type.find(IF_ID.cmmnd.name) != two_reg_type.end() ||
            offset_type.find(IF_ID.cmmnd.name) != offset_type.end());
        ID_ID.reg_to_write = IF_ID.cmmnd.r3;
        ID_ID.mem_write = IF_ID.cmmnd.name == "sw";
        ID_ID.branch = IF_ID.cmmnd.name == "beq" ? 1 : IF_ID.cmmnd.name == "bne" ? 2 : 0;
        ID_ID.jump = IF_ID.cmmnd.name == "j";
        ID_ID.mem_to_reg = IF_ID.cmmnd.name == "lw";
        ID_ID.branch_address = IF_ID.cmmnd.imm;
        if (IF_ID.cmmnd.name == "add" || IF_ID.cmmnd.name == "addi" ||
            IF_ID.cmmnd.name == "lw" || IF_ID.cmmnd.name == "sw")
            ID_ID.opc = ADD;
        else if (IF_ID.cmmnd.name == "sub" || IF_ID.cmmnd.name == "beq" ||
            IF_ID.cmmnd.name == "bne")
            ID_ID.opc = SUB;
        else if (IF_ID.cmmnd.name == "slt")
            ID_ID.opc = SLT;
        else if (IF_ID.cmmnd.name == "and")
            ID_ID.opc = AND;
        else if (IF_ID.cmmnd.name == "or")
            ID_ID.opc = OR;
        else if (IF_ID.cmmnd.name == "sll")
            ID_ID.opc = SLL;
        else if (IF_ID.cmmnd.name == "srl")
            ID_ID.opc = SRL;
        else if (IF_ID.cmmnd.name == "mul")
            ID_ID.opc = MUL;
    }

    void executeID2(PipelineRegisters::ID_RR & ID_ID, PipelineRegisters::ID_RR & ID_RR) {
        ID_RR = ID_ID;
    }

    void executeRR(PipelineRegisters::ID_RR & ID_RR, PipelineRegisters::RR_EX & RR_EX, PipelineRegisters::EX_MEM & EX_MEM, PipelineRegisters::MEM_WB & MEM_WB) {
        if (ID_RR.nop) {
            RR_EX.nop = true;
            return;
        }
        RR_EX.r2 = ID_RR.r2;
        RR_EX.nop = false;
        RR_EX.reg_to_write = ID_RR.reg_to_write;
        RR_EX.mem_write = ID_RR.mem_write;
        RR_EX.branch = ID_RR.branch;
        RR_EX.jump = ID_RR.jump;
        RR_EX.mem_to_reg = ID_RR.mem_to_reg;
        RR_EX.branch_address = ID_RR.branch_address;
        RR_EX.opc = ID_RR.opc;
        int rval1 = registers[ID_RR.r1];
        int rval2 = registers[ID_RR.r2];
        if (!EX_MEM.nop && ID_RR.r1 == EX_MEM.reg_to_write && ID_RR.r1 && !EX_MEM.mem_to_reg)
            rval1 = EX_MEM.output;
        else if (!MEM_WB.nop && ID_RR.r1 == MEM_WB.reg_to_write && ID_RR.r1)
            rval1 = MEM_WB.output;
        if (!EX_MEM.nop && ID_RR.r2 == EX_MEM.reg_to_write && ID_RR.r2 && !EX_MEM.mem_to_reg)
            rval2 = EX_MEM.output;
        else if (!MEM_WB.nop && ID_RR.r2 == MEM_WB.reg_to_write && ID_RR.r2)
            rval2 = MEM_WB.output;
        RR_EX.alu_input1 = rval1;
        RR_EX.alu_input2 = ID_RR.reg_dst ? ID_RR.imm : rval2;
        RR_EX.write_data = rval2;
    }

    void executeEXtoMEM(PipelineRegisters::RR_EX & RR_EX, PipelineRegisters::EX_MEM & EX_MEM, PipelineRegisters::MEM_WB & MEM_WB) {
        if (RR_EX.nop) {
            EX_MEM.nop = true;
            return;
        }
        EX_MEM.nop = false;
        EX_MEM.reg_to_write = RR_EX.reg_to_write;
        EX_MEM.mem_write = RR_EX.mem_write;
        EX_MEM.branch = RR_EX.branch;
        EX_MEM.mem_to_reg = RR_EX.mem_to_reg;
        EX_MEM.write_data = RR_EX.write_data;
        if (RR_EX.opc == ADD)
            EX_MEM.output = RR_EX.alu_input1 + RR_EX.alu_input2;
        else if (RR_EX.opc == SUB)
            EX_MEM.output = RR_EX.alu_input1 - RR_EX.alu_input2;
        else if (RR_EX.opc == SLT)
            EX_MEM.output = RR_EX.alu_input1 < RR_EX.alu_input2;
        else if (RR_EX.opc == AND)
            EX_MEM.output = RR_EX.alu_input1 & RR_EX.alu_input2;
        else if (RR_EX.opc == OR)
            EX_MEM.output = RR_EX.alu_input1 | RR_EX.alu_input2;
        else if (RR_EX.opc == SLL)
            EX_MEM.output = RR_EX.alu_input1 << RR_EX.alu_input2;
        else if (RR_EX.opc == SRL)
            EX_MEM.output = RR_EX.alu_input1 >> RR_EX.alu_input2;
        else if (RR_EX.opc == MUL)
            EX_MEM.output = RR_EX.alu_input1 * RR_EX.alu_input2;
        if (RR_EX.branch == 1 && EX_MEM.output == 0)
            EX_MEM.branch_address = RR_EX.branch_address;
        else if (RR_EX.branch == 2 && EX_MEM.output != 0)
            EX_MEM.branch_address = RR_EX.branch_address;
        else
            EX_MEM.branch_address = PC;
        if (!MEM_WB.nop && MEM_WB.reg_to_write == RR_EX.r2 && RR_EX.r2)
            EX_MEM.write_data = MEM_WB.output;
    }

    void executeMEMtoWB(PipelineRegisters::EX_MEM & MEM_MEM, PipelineRegisters::MEM_WB & MEM_WB) {
        if (MEM_MEM.nop) {
            MEM_WB.nop = true;
            return;
        }
        MEM_WB.nop = false;
        MEM_WB.reg_to_write = MEM_MEM.reg_to_write;
        if (MEM_MEM.mem_write) {
            if (memory[MEM_MEM.output/4] != MEM_MEM.write_data)
                memoryDelta[MEM_MEM.output/4] = MEM_MEM.write_data;
            memory[MEM_MEM.output/4] = MEM_MEM.write_data;
        }
        else
            MEM_WB.output = memory[MEM_MEM.output/4];
    }

    void executeMEMtoMEM(PipelineRegisters::EX_MEM & EX_MEM, PipelineRegisters::EX_MEM & MEM_MEM) {
        MEM_MEM = EX_MEM;
    }

    void executeWB(PipelineRegisters::MEM_WB & MEM_WB) {
        if (MEM_WB.nop)
            return;
        if (MEM_WB.reg_to_write) {
            // std::cout << "reg " << MEM_WB.reg_to_write << " = " << MEM_WB.output << std::endl;
            registers[MEM_WB.reg_to_write] = MEM_WB.output;
        }
    }

    void executeWB2(PipelineRegisters::EX_MEM & MEM_WB, PipelineRegisters::EX_MEM & MEM_MEM) {
        MEM_MEM.nop = true;
        if (MEM_WB.nop)
            return;
        if (MEM_WB.reg_to_write) {
            // std::cout << "reg " << MEM_WB.reg_to_write << " = " << MEM_WB.output << std::endl;
            registers[MEM_WB.reg_to_write] = MEM_WB.output;
        }
    }

    bool jumpStall(PipelineRegisters::IF_ID & IF_ID, PipelineRegisters::ID_RR & ID_ID, PipelineRegisters::ID_RR & ID_RR) {
        if (!IF_ID.is_nop() && IF_ID.cmmnd.name == "j") {
            return true;
        }
        if (!ID_ID.nop && ID_ID.jump) {
            return true;
        }
        if (!ID_RR.nop && ID_RR.jump) {
            PC = ID_RR.branch_address;
            return true;
        }
        return false;
    }

    bool branchStall(PipelineRegisters::IF_ID & IF_ID, PipelineRegisters::ID_RR & ID_ID, PipelineRegisters::ID_RR & ID_RR, PipelineRegisters::RR_EX & RR_EX, PipelineRegisters::EX_MEM & EX_MEM) {
        if (!IF_ID.is_nop() && (IF_ID.cmmnd.name == "beq" || IF_ID.cmmnd.name == "bne"))
            return true;
        if (!ID_ID.nop && ID_ID.branch) 
            return true;
    
        if (!ID_RR.nop && ID_RR.branch)
            return true;
        
        if (!RR_EX.nop && RR_EX.branch)
            return true;
        
        if (!EX_MEM.nop && EX_MEM.branch) {
            PC = EX_MEM.branch_address;
            return true;
        }
        return false;
    }


    bool secStall(PipelineRegisters::IF_ID &IF_IF, PipelineRegisters::ID_RR &ID_ID) {
        if (IF_IF.is_nop() || ID_ID.nop || !ID_ID.mem_to_reg)
            return false;
        if (two_reg_type.find(IF_IF.cmmnd.name) != two_reg_type.end() || three_reg_type.find(IF_IF.cmmnd.name) != three_reg_type.end())
            return  true;
        if (IF_IF.cmmnd.r1 == ID_ID.reg_to_write || (IF_IF.cmmnd.r2 == ID_ID.reg_to_write && IF_IF.cmmnd.name != "sw"))
            return true;
        return false;
    }

    bool firstStall(PipelineRegisters::IF_ID &IF_IF, PipelineRegisters::IF_ID &IF_ID) {
        if (IF_IF.is_nop() || IF_ID.is_nop())
            return false;
        if (offset_type.find(IF_IF.cmmnd.name) == offset_type.end() && offset_type.find(IF_ID.cmmnd.name) != offset_type.end())
            return true;
        if (IF_ID.cmmnd.name == "lw" && (IF_IF.cmmnd.r1 == IF_ID.cmmnd.r3 || IF_IF.cmmnd.r2 == IF_ID.cmmnd.r3))
            return true;
        return false;
    }

    void executeCommandsPipelined() {
        PipelineRegisters::MEM_WB MEM_WB;
        PipelineRegisters::EX_MEM EX_MEM, MEM_MEM;
        PipelineRegisters::RR_EX RR_EX;
        PipelineRegisters::ID_RR ID_ID, ID_RR;
        PipelineRegisters::IF_ID IF_IF, IF_ID;

        int cycles = 0;
        printRegistersAndMemoryDelta(0);
        while (PC < commands.size() || !IF_IF.is_nop() || !IF_ID.is_nop() || !ID_ID.nop || !ID_RR.nop || !RR_EX.nop || !EX_MEM.nop || !MEM_MEM.nop || !MEM_WB.nop) {
            // forwarding from EX_MEM when writing back or MEM_WB to RR_EX handled since writeback happens before reading in sequence of execution in simulator
            executeWB(MEM_WB);
            executeMEMtoWB(MEM_MEM, MEM_WB);
            if (!EX_MEM.nop && !EX_MEM.mem_write && !EX_MEM.mem_to_reg)
                executeWB2(EX_MEM, MEM_MEM);
            else
                executeMEMtoMEM(EX_MEM, MEM_MEM);
            executeEXtoMEM(RR_EX, EX_MEM, MEM_WB);
            executeRR(ID_RR, RR_EX, EX_MEM, MEM_WB);
            executeID2(ID_ID, ID_RR);
            executeID1(IF_ID, ID_ID);
            executeIF2(IF_IF, IF_ID);
            executeIF1(IF_IF);
            bool stall = false;
            if (firstStall(IF_IF, IF_ID)) {
                IF_IF.flush();
                stall = true;
            }
            if (secStall(IF_IF, ID_ID)) {
                IF_IF.flush();
                stall = true;
            }
            // jump stalls
            if (jumpStall(IF_ID, ID_ID, ID_RR)) {
                IF_IF.flush();
                stall = true;
            }
            // branch stalls
            if (branchStall(IF_ID, ID_ID, ID_RR, RR_EX, EX_MEM)) {
                IF_IF.flush();
                stall = true;
            }

            if (!stall)
                PC++;
            printRegistersAndMemoryDelta(++cycles);
        }
    }
};

int main(int argc, char ** argv) {
    if (argc != 2)
	{
		std::cerr << "Required argument: file_name\n./MIPS_interpreter <file name>\n";
		return 1;
	}
    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "File not found\n";
        return 1;
    }
    SevenNineStageWithBypass mips(file);
    mips.executeCommandsPipelined();
    return 0;
}