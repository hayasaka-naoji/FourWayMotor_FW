# Command-line build for the FourWay motor firmware.
#
# STM32CubeIDE itself uses its own managed build (see .cproject); this
# Makefile is provided so the same sources can be built/flashed from a shell,
# and so the project can be verified without opening the IDE.
#
#   make PREFIX=/c/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/\
#        com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin/arm-none-eabi-
#
# If arm-none-eabi-gcc is already on PATH, plain "make" is enough.

PREFIX  ?= arm-none-eabi-
CC      := $(PREFIX)gcc
AS      := $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size

TARGET  := FourWayMotor_FW
BUILD   := build

MCU     := -mcpu=cortex-m3 -mthumb
DEFS    := -DUSE_HAL_DRIVER -DSTM32F103xE
OPT     := -O2 -g3
CFLAGS  := $(MCU) $(DEFS) $(OPT) -std=gnu11 -Wall \
           -ffunction-sections -fdata-sections -MMD -MP

# Vendor device header (stm32f103xe.h) is taken from the local Cube repository.
CUBE_F1 ?= C:/Users/watson/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7
INCLUDES := -ICore/Inc \
            -IDrivers/STM32F1xx_HAL_Driver/Inc \
            -IDrivers/STM32F1xx_HAL_Driver/Inc/Legacy \
            -IDrivers/CMSIS/Device/ST/STM32F1xx/Include \
            -IDrivers/CMSIS/Include \
            -I$(CUBE_F1)/Drivers/CMSIS/Device/ST/STM32F1xx/Include

LDSCRIPT := STM32F103RCTX_FLASH.ld
LDFLAGS  := $(MCU) -T$(LDSCRIPT) -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map \
            --specs=nano.specs --specs=nosys.specs
LIBS     := -lc -lm

C_SOURCES := $(wildcard Core/Src/*.c) \
             $(wildcard Drivers/STM32F1xx_HAL_Driver/Src/*.c)
ASM_SOURCES := Core/Startup/startup_stm32f103xe.s

OBJECTS := $(addprefix $(BUILD)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

.PHONY: all clean

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin

$(BUILD)/%.o: %.c Makefile | $(BUILD)
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

$(BUILD)/%.o: %.s Makefile | $(BUILD)
	$(AS) -c $(CFLAGS) $(INCLUDES) $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJECTS) $(LDSCRIPT) Makefile | $(BUILD)
	$(CC) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $@
	$(SIZE) $@

$(BUILD)/%.hex: $(BUILD)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	$(OBJCOPY) -O binary -S $< $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
