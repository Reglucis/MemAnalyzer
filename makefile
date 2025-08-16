Debug_Flag = 0
CleanUp_Flag = 0

TargetName = main

shell = pwsh.exe

user_source_dir = \
. \
./Src \
./Src/LinkList \

C_INCLUDES =

CC_OPT = \
-fdiagnostics-color=always \
-fexec-charset=UTF-8 \
-finput-charset=UTF-8 \

ifeq ($(Debug_Flag), 0)
CC_OPT += \
-O3 \

else
CC_OPT += \
-o0 \
-g \

endif


C_DEFS =

_C_INCLUDES = $(addprefix -I , $(C_INCLUDES)) 
_C_DEFS = $(addprefix -D , $(C_DEFS))                 

CCFLAGS = $(_C_DEFS) $(_C_INCLUDES) $(CC_OPT)

BuildRootDir = .Output
_ObjectDir := $(BuildRootDir)\obj

_user_srcs := $(sort $(foreach i, $(user_source_dir), $(wildcard $(i)/*.c)))
vpath %.c $(dir $(_user_srcs))
vpath %.h $(dir $(_user_srcs))
_user_obj := $(addprefix $(_ObjectDir)/, $(notdir $(_user_srcs:.c=.o)))
all_obj = $(_user_obj)


COMPILER	= gcc.exe
LINKER		= gcc.exe

.PHONY: build
build: $(BuildRootDir)/$(TargetName).exe
$(BuildRootDir)/$(TargetName).exe: $(all_obj)
	@ echo Linking $@
	@ $(LINKER) $(all_obj) -g -o $@
	@ echo exe file has been created
	@ echo running exe file...
	@ echo -------------------------------   
# 	@ $(BuildRootDir)/$(TargetName).exe -p D:\MyProject_Local\MemAnalyzer\.Output\test.map

$(_ObjectDir)/%.o: %.c makefile | $(_ObjectDir)
	@ echo Building $@
	@ $(COMPILER) -c $(CCFLAGS) $< -o $@

$(_ObjectDir): | $(BuildRootDir)
	@ mkdir $(_ObjectDir)

$(BuildRootDir):
	@ mkdir $(BuildRootDir)

#######################################
# clean
#######################################
.PHONY: cleanfiles
cleanfiles:
	@echo off
	@echo [INFO] Clean compiled files...
	@DEL /S /Q *.bak 1>nul 2>nul
	@DEL /S /Q *.ddk 1>nul 2>nul
	@DEL /S /Q *.edk 1>nul 2>nul
	@DEL /S /Q *.lst 1>nul 2>nul
	@DEL /S /Q *.lnp 1>nul 2>nul
	@DEL /S /Q *.mpf 1>nul 2>nul
	@DEL /S /Q *.mpj 1>nul 2>nul
	@DEL /S /Q *.obj 1>nul 2>nul
	@DEL /S /Q *.omf 1>nul 2>nul
	@DEL /S /Q *.plg 1>nul 2>nul
	@DEL /S /Q *.rpt 1>nul 2>nul
	@DEL /S /Q *.tmp 1>nul 2>nul
	@DEL /S /Q *.__i 1>nul 2>nul
	@DEL /S /Q *.crf 1>nul 2>nul
	@DEL /S /Q *.o   1>nul 2>nul
	@DEL /S /Q *.d   1>nul 2>nul
	@DEL /S /Q *.axf 1>nul 2>nul
	@DEL /S /Q *.tra 1>nul 2>nul
	@DEL /S /Q *.dep 1>nul 2>nul
	@DEL /S /Q JLinkLog.txt 1>nul 2>nul
	@DEL /S /Q *.iex 1>nul 2>nul
	@DEL /S /Q *.htm 1>nul 2>nul
	@DEL /S /Q *.dbgconf 1>nul 2>nul
	@DEL /S /Q *.uvguix.* 1>nul 2>nul
	@DEL /S /Q *.scvd 1>nul 2>nul
	@echo [INFO] Cleanup completed!

.PHONY: clean
clean:
	@echo off
	@rd /s /q $(BUILD_ROOT_DIR)
