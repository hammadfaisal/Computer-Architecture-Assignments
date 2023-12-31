compile: 5stage 5stage_bypass 79stage 79stage_bypass

.PHONY:compile

5stage: 5stage.cpp MIPS_Processor.hpp
	g++ -o 5stage 5stage.cpp

5stage_bypass: 5stage_bypass.cpp MIPS_Processor.hpp
	g++ -o 5stage_bypass 5stage_bypass.cpp
	
79stage: 79stage.cpp MIPS_Processor.hpp
	g++ -o 79stage 79stage.cpp
	
79stage_bypass: 79stage_bypass.cpp MIPS_Processor.hpp
	g++ -o 79stage_bypass 79stage_bypass.cpp
	
run_5stage: 5stage input.asm
	./5stage input.asm

run_5stage_bypass: 5stage_bypass input.asm
	./5stage_bypass input.asm

run_79stage: 79stage input.asm
	./79stage input.asm

run_79stage_bypass: 79stage_bypass input.asm
	./79stage_bypass input.asm
	
clean:
	rm -f 5stage 5stage_bypass 79stage 79stage_bypass

	
