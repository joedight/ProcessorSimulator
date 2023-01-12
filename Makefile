demos_asm := $(wildcard kernel/*.s)
demos_c := $(wildcard kernel/*.c)

.PHONY: all clean

all: sim
kernel: $(demos_asm:.s=_asm.bin) $(demos_c:.c=_c.bin)

opt_flags = -O0

sim: src/simulator.c src/pipeline.c src/stats.c src/util.c src/config.c src/debugger.c src/alu.c  src/bht.c  src/bru.c  src/btac.c  src/cdb.c  src/lsu.c  src/ras.c  src/rob.c  src/rs.c
	cc -Wall -Wextra -Werror -O2 -Wno-missing-braces -Wno-missing-field-initializers -Wno-unused-parameter -Wno-pointer-arith -std=gnu11 -g $^ -o sim

%_c.bin %_c.enp: %.c kernel/include/isa.h
	riscv32-unknown-elf-gcc -specs=nosys.specs -static -ffreestanding -I ${PWD}/kernel/include/ $(opt_flags) -Ttext 0x1000 -g -march=rv32i -o "$*_c.o" $^
	readelf -l "$*_c.o" | grep -P -o "(?<=[Ee]ntry point 0x1)[0-9a-f]{3}" > "$*_c.enp"
	riscv32-unknown-elf-objcopy -O binary "$*_c.o" $@

%_asm.bin %_asm.enp: %.s
	riscv32-unknown-elf-gcc -nostdlib -static -ffreestanding -O1 -g -Ttext 0x1000 -march=rv32i -o "$*_asm.o" $^
	readelf -l "$*_asm.o" | grep -P -o "(?<=[Ee]ntry point 0x1)[0-9a-f]{3}" > "$*_asm.enp"
	riscv32-unknown-elf-objcopy -O binary "$*_asm.o" $@

clean:
	rm -f sim kernel/*.bin kernel/*.o kernel/*.enp
