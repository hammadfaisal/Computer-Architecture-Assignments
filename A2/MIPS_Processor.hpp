#ifndef __MIPS_PIPELINED_HPP__
#define __MIPS_PIPELINED_HPP__
#include <boost/tokenizer.hpp>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct MIPS_Processor {
    struct command {
        std::string name;
        int r1, r2, r3, imm;
        // r1-> read register 1, r2-> read register 2, r3-> write register
        // for subtraction r1-r2, r3 is the destination
        // address for jumping or branching is stored in imm
        command(std::string name, int r1, int r2, int r3, int imm)
            : name(name), r1(r1), r2(r2), r3(r3), imm(imm) {}

        command() {
            r1 = r2 = r3 = imm = 0;
            name = "nop";
        }
    };
    enum alu_op_code { ADD = 0, SUB, SLT, AND, OR, SLL, SRL, MUL };
    std::unordered_map<std::string, int> registerMap,
        address; // Map register names to register numbers
    std::unordered_map<int, int> memoryDelta; // stores the change in memory
    std::unordered_set<std::string> three_reg_type = {"add", "sub", "slt",
                                                      "and", "or",  "mul"};
    std::unordered_set<std::string> two_reg_type = {"sll", "srl", "addi"};
    std::unordered_set<std::string> offset_type = {"lw", "sw"};
    std::unordered_set<std::string> branch_type = {"beq", "bne"};
    std::unordered_set<std::string> jump_type = {"j"};
    int registers[32] = {0};            // 32 registers
    static const int MAX_MEM = 1 << 20; // 1 MB memory
    int memory[MAX_MEM / 4] = {0};      // 4 bytes per word
    int PC = 0;                         // Program counter
    int error_PC = 0;                   // PC at which error occurred
    int PC_next = 0;                    // Next PC
    // enum stages {IF=0, ID, EX, MEM, WB}; // Pipeline stages
    int cycle = 0;                 // Current cycle
    std::vector<command> commands; // List of commands
    std::vector<std::vector<std::string>> parsed;
    enum exit_code {
        SUCCESS = 0,
        INVALID_REGISTER,
        INVALID_LABEL,
        INVALID_ADDRESS,
        SYNTAX_ERROR,
        MEMORY_ERROR
    };

    MIPS_Processor(std::ifstream &file) {
        for (int i = 0; i < 32; ++i)
            registerMap["$" + std::to_string(i)] = i;
        registerMap["$zero"] = 0;
        registerMap["$at"] = 1;
        registerMap["$v0"] = 2;
        registerMap["$v1"] = 3;
        for (int i = 0; i < 4; ++i)
            registerMap["$a" + std::to_string(i)] = i + 4;
        for (int i = 0; i < 8; ++i)
            registerMap["$t" + std::to_string(i)] = i + 8,
                               registerMap["$s" + std::to_string(i)] = i + 16;
        registerMap["$t8"] = 24;
        registerMap["$t9"] = 25;
        registerMap["$k0"] = 26;
        registerMap["$k1"] = 27;
        registerMap["$gp"] = 28;
        registerMap["$sp"] = 29;
        registerMap["$s8"] = 30;
        registerMap["$ra"] = 31;

        if (!parseInput(file))
            handleExit(SYNTAX_ERROR, 0);
        exit_code e = genCommands();
        if (e != SUCCESS)
            handleExit(e, 0);
    }

    // checks if the register is a valid one
    inline bool checkRegister(std::string r) {
        return registerMap.find(r) != registerMap.end();
    }

    // checks if all of the registers are valid or not
    bool checkRegisters(std::vector<std::string> regs) {
        return std::all_of(regs.begin(), regs.end(),
                           [&](std::string r) { return checkRegister(r); });
    }

    // Similar to an assembler, generates the commands
    exit_code genCommands() {
        for (auto &line : parsed) {
            if (line.size() == 0)
                continue;
            if (three_reg_type.find(line[0]) != three_reg_type.end()) {
                if (line.size() != 4)
                    return SYNTAX_ERROR;
                if (!checkRegisters({line[1], line[2], line[3]}) ||
                    registerMap[line[1]] == 0)
                    return INVALID_REGISTER;
                commands.push_back(command(line[0], registerMap[line[2]],
                                           registerMap[line[3]],
                                           registerMap[line[1]], 0));
            } else if (two_reg_type.find(line[0]) != two_reg_type.end()) {
                if (line.size() != 4)
                    return SYNTAX_ERROR;
                if (!checkRegisters({line[1], line[2]}) ||
                    registerMap[line[1]] == 0)
                    return INVALID_REGISTER;
                int imm = stoi(line[3]);
                commands.push_back(command(line[0], registerMap[line[2]], 0,
                                           registerMap[line[1]], imm));
            } else if (offset_type.find(line[0]) != offset_type.end()) {
                if (line.size() != 3)
                    return SYNTAX_ERROR;
                int offset = stoi(line[2].substr(0, line[2].find('(')));
                std::string reg =
                    line[2].substr(line[2].find('(') + 1,
                                   line[2].find(')') - line[2].find('(') - 1);
                if (!checkRegisters({line[1], reg}))
                    return INVALID_REGISTER;
                if (line[0] == "sw") {
                    if (registerMap[line[1]] == 0)
                        return INVALID_REGISTER;
                    commands.push_back(command(line[0], registerMap[reg],
                                               registerMap[line[1]], 0,
                                               offset));
                } else {
                    commands.push_back(command(line[0], registerMap[reg], 0,
                                               registerMap[line[1]], offset));
                }
            } else if (branch_type.find(line[0]) != branch_type.end()) {
                if (line.size() != 4)
                    return SYNTAX_ERROR;
                if (!checkRegisters({line[1], line[2]}))
                    return INVALID_REGISTER;
                if (address.find(line[3]) == address.end() ||
                    address[line[3]] == -1)
                    return INVALID_LABEL;
                commands.push_back(command(line[0], registerMap[line[1]],
                                           registerMap[line[2]], 0,
                                           address[line[3]]));
            } else if (jump_type.find(line[0]) != jump_type.end()) {
                if (line.size() != 2)
                    return SYNTAX_ERROR;
                if (address.find(line[1]) == address.end() ||
                    address[line[1]] == -1)
                    return INVALID_LABEL;
                commands.push_back(command(line[0], 0, 0, 0, address[line[1]]));
            } else {
                return SYNTAX_ERROR;
            }
            error_PC++;
        }
        return SUCCESS;
    }

    // parse the command assuming correctly formatted MIPS labels
    bool parseCommand(std::string line) {
        // strip until before the comment begins
        line = line.substr(0, line.find('#'));
        std::vector<std::string> toks;
        boost::tokenizer<boost::char_separator<char>> tokens(
            line, boost::char_separator<char>(", \t"));
        for (auto &s : tokens)
            toks.push_back(s);
        // empty line or a comment only line
        if (toks.empty())
            return true;
        else if (toks.size() == 1) {
            std::string label = toks[0].back() == ':'
                                    ? toks[0].substr(0, toks[0].size() - 1)
                                    : "?";
            if (address.find(label) == address.end())
                address[label] = parsed.size();
            else
                address[label] = -1;
            return true;
        } else if (toks[0].back() == ':') {
            std::string label = toks[0].substr(0, toks[0].size() - 1);
            if (address.find(label) == address.end())
                address[label] = parsed.size();
            else
                address[label] = -1;
            toks = std::vector<std::string>(toks.begin() + 1, toks.end());
        } else if (toks[0].find(':') != std::string::npos) {
            int idx = toks[0].find(':');
            std::string label = toks[0].substr(0, idx);
            if (address.find(label) == address.end())
                address[label] = parsed.size();
            else
                address[label] = -1;
            toks[0] = toks[0].substr(idx + 1);
        } else if (toks[1][0] == ':') {
            if (address.find(toks[0]) == address.end())
                address[toks[0]] = parsed.size();
            else
                address[toks[0]] = -1;
            toks[1] = toks[1].substr(1);
            if (toks[1] == "")
                toks.erase(toks.begin(), toks.begin() + 2);
            else
                toks.erase(toks.begin(), toks.begin() + 1);
        }
        if (toks.empty())
            return true;
        if (toks.size() > 4) {
            parsed.push_back(toks);
            error_PC = parsed.size() - 1;
            return false;
        }
        parsed.push_back(toks);
        return true;
    }

    bool parseInput(std::ifstream &file) {
        std::string line;
        while (getline(file, line)) {
            if (!parseCommand(line)) {
                file.close();
                return false;
            }
        }
        file.close();
        return true;
    }

    void handleExit(exit_code code, int cycleCount) {
        std::cout << '\n';
        switch (code) {
        case 1:
            std::cerr
                << "Invalid register provided or syntax error in providing "
                   "register\n";
            break;
        case 2:
            std::cerr << "Label used not defined or defined too many times\n";
            break;
        case 3:
            std::cerr << "Unaligned or invalid memory address specified\n";
            break;
        case 4:
            std::cerr << "Syntax error encountered\n";
            break;
        case 5:
            std::cerr << "\n";
            break;
        default:
            break;
        }
        if (code != 0) {
            std::cerr << "Error encountered at:\n";
            for (auto &s : parsed[error_PC])
                std::cerr << s << ' ';
            std::cerr << '\n';
        }
        exit(code);
    }
    void printRegistersAndMemoryDelta(int clockCycle) {
        for (int i = 0; i < 32; ++i)
            std::cout << registers[i] << ' ';
        std::cout << '\n';
        if (memoryDelta.empty())
            std::cout << "0\n";
        else {
            std::cout << memoryDelta.size() << ' ';
            for (auto &p : memoryDelta)
                std::cout << p.first << ' ' << p.second << '\n';
            memoryDelta.clear();
        }
    }
};

struct FiveStage : MIPS_Processor {
    struct PipelineRegisters {
        struct IF_ID {
            command cmmnd;
            IF_ID() { cmmnd = command(); }
            void flush() { cmmnd.name = "nop"; }
            bool is_nop() { return cmmnd.name == "nop"; }
        };
        struct ID_EX {
            bool mem_write, jump, mem_to_reg, nop;
            int reg_to_write, alu_input1, alu_input2, branch_address, branch,
                r2,
                write_data; // branch-> 0 for no branch, 1 for branch on equal,
                            // 2 for
            // branch on not equal
            alu_op_code opc;
            ID_EX() { nop = true; }
        };
        struct EX_MEM {
            bool mem_write, branch, mem_to_reg, nop;
            int reg_to_write, alu_output, branch_address, write_data;
            EX_MEM() { nop = true; }
        };
        struct MEM_WB {
            bool nop;
            int reg_to_write, mem_output;
            MEM_WB() { nop = true; }
        };
    };
    FiveStage(std::ifstream & file) : MIPS_Processor(file) {}
};

struct SevenNineStage : MIPS_Processor {
    struct PipelineRegisters {
        struct IF_ID {
            command cmmnd;
            IF_ID() { cmmnd = command(); }
            void flush() { cmmnd.name = "nop"; }
            bool is_nop() { return cmmnd.name == "nop"; }
        };
        struct ID_RR {
            bool mem_write, jump, mem_to_reg, nop, reg_dst;
            int reg_to_write, r1, r2, imm, branch_address, branch,
                write_data; // branch-> 0 for no branch, 1 for branch on equal,
                            // 2 for
            // branch on not equal
            alu_op_code opc;
            ID_RR() { nop = true; }
        };
        struct RR_EX {
            bool mem_write, jump, mem_to_reg, nop;
            int reg_to_write, alu_input1, alu_input2, branch_address, branch,
                r2,
                write_data; // branch-> 0 for no branch, 1 for branch on equal,
                            // 2 for
            // branch on not equal
            alu_op_code opc;
            RR_EX() { nop = true; }
        };
        struct EX_MEM {
            bool mem_write, branch, mem_to_reg, nop;
            int reg_to_write, output, branch_address, write_data;
            EX_MEM() { nop = true; }
        };
        struct MEM_WB {
            bool nop;
            int reg_to_write, output;
            MEM_WB() { nop = true; }
        };
    };
    SevenNineStage(std::ifstream & file) : MIPS_Processor(file) {}
};

#endif