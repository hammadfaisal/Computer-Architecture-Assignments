#include "MIPS_Processor.hpp"

struct FiveStageWithoutBypass : FiveStage {
    FiveStageWithoutBypass(std::ifstream &file) : FiveStage(file) {}
    void executeIF(PipelineRegisters::IF_ID & IF_ID) {
        if (PC >= commands.size()) {
            IF_ID.cmmnd.name = "nop";
            return;
        }
        IF_ID.cmmnd = commands[PC];
    }

    void executeID(PipelineRegisters::IF_ID & IF_ID,
        PipelineRegisters::ID_EX & ID_EX) {
        if (IF_ID.cmmnd.name == "nop") {
            ID_EX.nop = true;
            return;
        }
        ID_EX.nop = false;
        ID_EX.alu_input1 = registers[IF_ID.cmmnd.r1];
        if (two_reg_type.find(IF_ID.cmmnd.name) != two_reg_type.end() ||
            offset_type.find(IF_ID.cmmnd.name) != offset_type.end())
            ID_EX.alu_input2 = IF_ID.cmmnd.imm;
        else
            ID_EX.alu_input2 = registers[IF_ID.cmmnd.r2];
        ID_EX.write_data = registers[IF_ID.cmmnd.r2];
        ID_EX.reg_to_write = IF_ID.cmmnd.r3;
        ID_EX.mem_write = IF_ID.cmmnd.name == "sw";
        ID_EX.branch = IF_ID.cmmnd.name == "beq" ? 1 : IF_ID.cmmnd.name == "bne" ? 2 : 0;
        ID_EX.jump = IF_ID.cmmnd.name == "j";
        ID_EX.mem_to_reg = IF_ID.cmmnd.name == "lw";
        ID_EX.branch_address = IF_ID.cmmnd.imm;
        if (IF_ID.cmmnd.name == "add" || IF_ID.cmmnd.name == "addi" ||
            IF_ID.cmmnd.name == "lw" || IF_ID.cmmnd.name == "sw")
            ID_EX.opc = ADD;
        else if (IF_ID.cmmnd.name == "sub" || IF_ID.cmmnd.name == "beq" ||
            IF_ID.cmmnd.name == "bne")
            ID_EX.opc = SUB;
        else if (IF_ID.cmmnd.name == "slt")
            ID_EX.opc = SLT;
        else if (IF_ID.cmmnd.name == "and")
            ID_EX.opc = AND;
        else if (IF_ID.cmmnd.name == "or")
            ID_EX.opc = OR;
        else if (IF_ID.cmmnd.name == "sll")
            ID_EX.opc = SLL;
        else if (IF_ID.cmmnd.name == "srl")
            ID_EX.opc = SRL;
        else if (IF_ID.cmmnd.name == "mul")
            ID_EX.opc = MUL;
    }

    void executeEX(PipelineRegisters::ID_EX & ID_EX,
        PipelineRegisters::EX_MEM & EX_MEM) {
        if (ID_EX.nop) {
            EX_MEM.nop = true;
            return;
        }
        EX_MEM.nop = false;
        EX_MEM.reg_to_write = ID_EX.reg_to_write;
        EX_MEM.mem_write = ID_EX.mem_write;
        EX_MEM.branch = ID_EX.branch;
        EX_MEM.mem_to_reg = ID_EX.mem_to_reg;
        if (ID_EX.opc == ADD)
            EX_MEM.alu_output = ID_EX.alu_input1 + ID_EX.alu_input2;
        else if (ID_EX.opc == SUB)
            EX_MEM.alu_output = ID_EX.alu_input1 - ID_EX.alu_input2;
        else if (ID_EX.opc == MUL)
            EX_MEM.alu_output = ID_EX.alu_input1 * ID_EX.alu_input2;
        else if (ID_EX.opc == SLT)
            EX_MEM.alu_output = ID_EX.alu_input1 < ID_EX.alu_input2;
        else if (ID_EX.opc == AND)
            EX_MEM.alu_output = ID_EX.alu_input1 & ID_EX.alu_input2;
        else if (ID_EX.opc == OR)
            EX_MEM.alu_output = ID_EX.alu_input1 | ID_EX.alu_input2;
        else if (ID_EX.opc == SLL)
            EX_MEM.alu_output = ID_EX.alu_input1 << ID_EX.alu_input2;
        else if (ID_EX.opc == SRL)
            EX_MEM.alu_output = ID_EX.alu_input1 >> ID_EX.alu_input2;
        if (ID_EX.branch == 1 && EX_MEM.alu_output == 0)
            EX_MEM.branch_address = ID_EX.branch_address;
        else if (ID_EX.branch == 2 && EX_MEM.alu_output != 0)
            EX_MEM.branch_address = ID_EX.branch_address;
        else
            EX_MEM.branch_address = PC;
        EX_MEM.write_data = ID_EX.write_data;
    }

    void executeMEM(PipelineRegisters::EX_MEM & EX_MEM,
        PipelineRegisters::MEM_WB & MEM_WB) {
        if (EX_MEM.nop) {
            MEM_WB.nop = true;
            return;
        }
        MEM_WB.nop = false;
        MEM_WB.reg_to_write = EX_MEM.reg_to_write;
        if (EX_MEM.mem_write) {
            if (memory[EX_MEM.alu_output/4] != EX_MEM.write_data)
                memoryDelta[EX_MEM.alu_output/4] = EX_MEM.write_data;
            memory[EX_MEM.alu_output/4] = EX_MEM.write_data;
        }
        if (EX_MEM.mem_to_reg)
            MEM_WB.mem_output = memory[EX_MEM.alu_output/4];
        else
            MEM_WB.mem_output = EX_MEM.alu_output;
    }

    void executeWB(PipelineRegisters::MEM_WB & MEM_WB) {
        if (MEM_WB.nop)
            return;
        if (MEM_WB.reg_to_write != 0)
            registers[MEM_WB.reg_to_write] = MEM_WB.mem_output;
    }

    void executeCommandsPipelined() {
        PipelineRegisters::MEM_WB MEM_WB;
        PipelineRegisters::EX_MEM EX_MEM;
        PipelineRegisters::ID_EX ID_EX;
        PipelineRegisters::IF_ID IF_ID;

        int cycles = 0;
        printRegistersAndMemoryDelta(0);
        while (PC < commands.size() || !IF_ID.is_nop() || !ID_EX.nop ||
            !EX_MEM.nop || !MEM_WB.nop) {
            executeWB(MEM_WB);
            executeMEM(EX_MEM, MEM_WB);
            executeEX(ID_EX, EX_MEM);
            executeID(IF_ID, ID_EX);
            executeIF(IF_ID);
            bool stall = false;

            if (!ID_EX.nop && ID_EX.jump) {
                PC = ID_EX.branch_address;
                IF_ID.flush();
                stall = true;
            }
            if (!ID_EX.nop && ID_EX.branch) {
                IF_ID.flush();
                stall = true;
            }
            if (!EX_MEM.nop && EX_MEM.branch) {
                PC = EX_MEM.branch_address;
                IF_ID.flush();
                stall = true;
            }
            if (((ID_EX.reg_to_write != 0 && !ID_EX.nop && !IF_ID.is_nop()) &&
                    (ID_EX.reg_to_write == IF_ID.cmmnd.r1 ||
                        ID_EX.reg_to_write == IF_ID.cmmnd.r2)) ||
                ((EX_MEM.reg_to_write != 0 && !EX_MEM.nop && !IF_ID.is_nop()) &&
                    (EX_MEM.reg_to_write == IF_ID.cmmnd.r1 ||
                        EX_MEM.reg_to_write == IF_ID.cmmnd.r2))) {
                IF_ID.flush();
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
    FiveStageWithoutBypass mips(file);
    mips.executeCommandsPipelined();
    return 0;
}
