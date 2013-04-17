default: check

ifndef LUA_DIR
LUA_DIR=../src
endif

LUA=$(LUA_DIR)/lua
LUAC=$(LUA_DIR)/luac

std_progs = \
	bisect.lua \
	factorial.lua \
	fib.lua \
	hello.lua \
	sort.lua \
	trace-calls.lua \
	env.lua \
	fibfor.lua \
	life.lua \
	printf.lua \
	sieve.lua \
	cf.lua \
	trace-globals.lua

special_progs = \
	table.lua \
	globals.lua \
	luac.lua \
	xd.lua \
	readonly.lua \
	echo.lua

all_progs = $(std_progs) $(special_progs)

default: check

.PHONY: run_std
run_std: 
	@for f in $(std_progs); do \
		echo running $$f ...; \
		$(LUA) $$f > `basename $$f .lua`.out 2>&1; \
		#$(LUA) $$f; \
    	done

.PHONY: run_special
run_special:
	@echo running globals.lua ...
	@$(LUAC) -p -l life.lua | $(LUA) globals.lua | sort | $(LUA) table.lua > globals.out
	@echo running xd.lua ...
	@$(LUA) xd.lua < life.lua > xd.out
	@echo running echo.lua ...
	@$(LUA) echo.lua AAA BBBB CCCCC DDDDDD > echo.out
	@echo running luac.lua ...
	@$(LUA) luac.lua luac.lua
	@echo running table.lua ...
	@$(LUA) table.lua < data.txt > table.out
	@echo running readonly.lua ...
	@$(LUA) readonly.lua 2>&1 | sed 's/^.*\/lua:/lua:/' > readonly.out

.PHONY: run_all
run_all: run_std run_special

.PHONY: check
check: run_all
	@for f in `ls *.lua`; do \
		base=`basename $$f .lua`; \
		if (diff -u $$base.exp $$base.out > $$base.res); then \
		    echo $$base passed; \
		else \
		    echo "***" $$base FAILED!; \
		fi; \
	done

.PHONY: clean
clean:
	rm -f *.out
	rm -f *.res
