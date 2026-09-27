# Makefile - strict POSIX build recipe for the pith binary and runtime.
#
# The toolchain links against the vendored tcc tree (vendor/tcc) for
# libtcc (the in-memory execution bridge and built-in ELF linker) and
# builds runtime/libruntime.a for `pith build` AOT artifacts.
#
# No GNU make extensions are used: no $(shell), no conditionals, no
# pattern rules, no .DEFAULT_GOAL, no target-specific variables. The
# default goal is the first target (all). Override CC/CFLAGS/LDFLAGS on
# the command line if needed.

# ------------------------------------------------------------------
# Tool configuration
# ------------------------------------------------------------------

CC = cc
AR = ar
CFLAGS = -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Iinclude
LDFLAGS =
POSIXDEF = -D_POSIX_C_SOURCE=200809L

VENDOR_TCC = vendor/tcc
LIBTCC = $(VENDOR_TCC)/libtcc.a

VENDOR_QBE = vendor/qbe
LIBQBE = $(VENDOR_QBE)/libqbe.a

# libtcc and libqbe are built from vendored trees
TCC_CFLAGS = -DPITH_HAVE_LIBTCC=1 -I$(VENDOR_TCC)
TCC_LINK = $(LIBTCC) -ldl

QBE_CFLAGS = -DPITH_HAVE_LIBQBE=1 -I$(VENDOR_QBE)
QBE_LINK = $(LIBQBE)

DEFINES = $(POSIXDEF)

PITH_OBJS = src/main.o src/lexer.o src/parser.o src/gen_qbe.o \
	src/engine_proxy.o src/pkg_manager.o src/config.o src/tar.o \
	src/pith_embed.o src/cffi.o
RUNTIME_OBJS = runtime/memory.o runtime/os_fs.o runtime/network.o
RUNTIME_LIB = runtime/libruntime.a
PITH = pith

# ------------------------------------------------------------------
# all is the first target: the POSIX default goal
# ------------------------------------------------------------------

all: $(PITH) $(RUNTIME_LIB)

$(PITH): $(PITH_OBJS) $(RUNTIME_OBJS) $(LIBTCC) $(LIBQBE)
	$(CC) $(LDFLAGS) -o $@ $(PITH_OBJS) $(RUNTIME_OBJS) $(QBE_LINK) $(TCC_LINK)

# Find an `ar` that can CREATE archives (BusyBox ar can only
# extract/list archives, which breaks static library builds).
# Detection runs inside the recipe shell; no $(shell) is used.
$(RUNTIME_LIB): $(RUNTIME_OBJS)
	AR_BIN="$(AR)"; \
	if [ "$$AR_BIN" = "ar" ] || [ -z "$$AR_BIN" ]; then \
		for a in /usr/bin/ar `command -v x86_64-linux-gnu-ar` `command -v llvm-ar`; do \
			if [ -n "$$a" ] && $$a --version 2>/dev/null | head -n 1 | grep -qE 'GNU ar|LLVM'; then \
				AR_BIN=$$a; \
				break; \
			fi; \
		done; \
	fi; \
	if [ -z "$$AR_BIN" ]; then \
		AR_BIN=ar; \
	fi; \
	$$AR_BIN rcs $@ $(RUNTIME_OBJS)

# ------------------------------------------------------------------
# Suffix rules (POSIX inference rules; no %.o pattern rules)
# ------------------------------------------------------------------

.SUFFIXES:
.SUFFIXES: .c .o

.c.o:
	$(CC) $(DEFINES) $(CFLAGS) $(TCC_CFLAGS) $(QBE_CFLAGS) -c -o $@ $<

$(PITH_OBJS): vendor/qbe/.pith-patched vendor/tcc/.pith-patched
$(RUNTIME_OBJS): vendor/tcc/.pith-patched

# ------------------------------------------------------------------
# Vendored QBE (libqbe)
# ------------------------------------------------------------------

vendor/qbe/.pith-patched: patches/qbe-embed.patch vendor/qbe/parse.c
	if grep -q "qbe_err_jmp" vendor/qbe/parse.c; then \
		touch vendor/qbe/.pith-patched; \
	else \
		cd vendor/qbe && patch -p1 -N < ../../patches/qbe-embed.patch && touch .pith-patched; \
	fi

$(LIBQBE): vendor/qbe/.pith-patched
	AR_BIN="$(AR)"; \
	if [ "$$AR_BIN" = "ar" ] || [ -z "$$AR_BIN" ]; then \
		for a in /usr/bin/ar `command -v x86_64-linux-gnu-ar` `command -v llvm-ar`; do \
			if [ -n "$$a" ] && $$a --version 2>/dev/null | head -n 1 | grep -qE 'GNU ar|LLVM'; then \
				AR_BIN=$$a; \
				break; \
			fi; \
		done; \
	fi; \
	if [ -z "$$AR_BIN" ]; then \
		AR_BIN=ar; \
	fi; \
	AR_DIR=`dirname "$$AR_BIN"`; \
	cd $(VENDOR_QBE) && PATH="$$AR_DIR:$$PATH" $(MAKE) CC="$(CC)" AR="$$AR_BIN" libqbe.a

$(VENDOR_QBE)/qbe: vendor/qbe/.pith-patched
	cd $(VENDOR_QBE) && $(MAKE) qbe

# ------------------------------------------------------------------
# Vendored tcc (libtcc)
# ------------------------------------------------------------------

# Pith carries a small patch for the vendored tcc (missing scalar SSE
# opcodes: movsd/movss/addsd/... that QBE's float codegen emits). The
# patch is applied automatically after a fresh submodule checkout; it
# is also tracked in patches/ for upstreaming (see docs/vendor-tcc.md).
vendor/tcc/.pith-patched: patches/tcc-scalar-sse.patch vendor/tcc/x86_64-asm.h
	if grep -q "movsd" vendor/tcc/x86_64-asm.h; then \
		touch vendor/tcc/.pith-patched; \
	else \
		cd vendor/tcc && patch -p1 -N < ../../patches/tcc-scalar-sse.patch && touch .pith-patched; \
	fi

vendor/tcc/config.mak: vendor/tcc/configure
	cd $(VENDOR_TCC) && ./configure

# The vendored tcc Makefile invokes `ar` from PATH; prefix PATH with
# the directory of the archiver detected above (BusyBox ar cannot
# create archives).
$(LIBTCC): $(VENDOR_TCC)/config.mak $(VENDOR_TCC)/.pith-patched
	AR_BIN=""; \
	for a in /usr/bin/ar `command -v x86_64-linux-gnu-ar` `command -v llvm-ar`; do \
		if [ -n "$$a" ] && $$a --version 2>/dev/null | head -n 1 | grep -qE 'GNU ar|LLVM'; then \
			AR_BIN=$$a; \
			break; \
		fi; \
	done; \
	if [ -z "$$AR_BIN" ]; then \
		AR_BIN=ar; \
	fi; \
	AR_DIR=`dirname "$$AR_BIN"`; \
	cd $(VENDOR_TCC) && PATH="$$AR_DIR:$$PATH" $(MAKE)

# ------------------------------------------------------------------
# Verification & maintenance
# ------------------------------------------------------------------

.PHONY: all check test clean distclean help

check test: all
	./pith run tests/test_audit.pi
	./pith build tests/test_audit.pi
	./test_audit
	rm -f test_audit
	./pith run tests/test_sweep.pi 2>/dev/null | grep -q "sweep: done"
	./pith run tests/test_arc.pi 2>/dev/null | grep -q "arc: done"
	./pith run tests/test_fn.pi > /dev/null
	@! ./pith run tests/test_err1.pi > /dev/null 2>&1
	@! ./pith run tests/test_err2.pi > /dev/null 2>&1
	@! ./pith decompile tests/test_audit.pi tests/test_multi_extra.pi 2>/dev/null | grep -q 'fn_'
	./pith build --embed-source tests/test_audit.pi
	@tail -c 16 test_audit | grep -q PITHDEBG
	./pith decompile ./test_audit
	@test -f restored_workspace/tests/test_audit.pi
	@rm -rf restored_workspace test_audit
	# [ARC] scope exits, self-assignment hazards, string semantics
	./pith run tests/arc_deep.pi 2>/dev/null | grep -q "outer binding alive"
	@! ./pith run tests/arc_selfassign.pi 2>/dev/null | grep -q BROKEN
	./pith run tests/lexer_nul.pi 2>/dev/null | grep -q "nul lossless ok"
	./pith run tests/lexer_strings.pi 2>/dev/null | grep -q "utf8 payload ok"
	# [CODEGEN] INT64 boundaries, empty branches, deterministic SIGFPE
	@! ./pith run tests/codegen_int64.pi 2>/dev/null | grep -q BROKEN
	./pith run tests/codegen_branches.pi 2>/dev/null | grep -q "empty: done"
	# [LOOPS] while loops and break statements (JIT and AOT)
	@! ./pith run tests/test_while.pi 2>/dev/null | grep -q BROKEN
	./pith run tests/test_while.pi 2>/dev/null | grep -q "while: done"
	./pith build tests/test_while.pi
	@! ./test_while | grep -q BROKEN
	./test_while | grep -q "while: done"
	@rm -f test_while
	@! ./pith run tests/while_err_break.pi > /dev/null 2>&1
	# [LOGICAL] and, or, not, and continue statements (JIT and AOT)
	@! ./pith run tests/test_logical.pi 2>/dev/null | grep -q BROKEN
	./pith run tests/test_logical.pi 2>/dev/null | grep -q "logical: done"
	./pith build tests/test_logical.pi
	@! ./test_logical | grep -q BROKEN
	./test_logical | grep -q "logical: done"
	@rm -f test_logical
	@! ./pith run tests/while_err_continue.pi > /dev/null 2>&1
	# [FUNCTIONS] user functions, recursion, direct calls (JIT and AOT)
	@! ./pith run tests/test_fn_call.pi 2>/dev/null | grep -q BROKEN
	./pith run tests/test_fn_call.pi 2>/dev/null | grep -q "functions: done"
	./pith build tests/test_fn_call.pi
	@! ./test_fn_call | grep -q BROKEN
	./test_fn_call | grep -q "functions: done"
	@rm -f test_fn_call
	# [BUILTINS] os.* and net.* namespaces (JIT and AOT)
	@! ./pith run tests/test_os_net.pi 2>/dev/null | grep -q BROKEN
	./pith run tests/test_os_net.pi 2>/dev/null | grep -q "os_net: done"
	./pith build tests/test_os_net.pi
	@! ./test_os_net | grep -q BROKEN
	./test_os_net | grep -q "os_net: done"
	@rm -f test_os_net tests/test_scratch.txt
	# [BUILTINS] proc.* namespace (JIT, AOT, and exit code)
	@! ./pith run tests/test_proc.pi 2>/dev/null | grep -q BROKEN
	./pith run tests/test_proc.pi 2>/dev/null | grep -q "proc: done"
	./pith build tests/test_proc.pi
	@! ./test_proc | grep -q BROKEN
	./test_proc | grep -q "proc: done"
	@rm -f test_proc
	@./pith run tests/test_proc_exit.pi >/dev/null 2>&1; test $$? -eq 42
	./pith build tests/test_proc_exit.pi
	@./test_proc_exit >/dev/null 2>&1; test $$? -eq 42
	@rm -f test_proc_exit
	# [REPL] interactive execution and session persistence
	@printf 'x = 10\nx + 5\nexit\n' | ./pith repl 2>/dev/null | grep -q "15"
	# [FFI] native C import pipeline: JIT and AOT paths
	@! ./pith run tests/test_ffi.pi 2>/dev/null | grep -q BROKEN
	./pith run tests/test_ffi.pi 2>/dev/null | grep -q "ffi: done"
	./pith build tests/test_ffi.pi
	@! ./test_ffi | grep -q BROKEN
	./test_ffi | grep -q "ffi: done"
	@rm -f test_ffi
	# [TYPES] sized types, mutability, wrapping, bounds checking
	@! ./pith run tests/test_types.pi 2>/dev/null | grep -q BROKEN
	./pith run tests/test_types.pi 2>/dev/null | grep -q "types: done"
	@! ./pith run tests/types_err_immutable.pi > /dev/null 2>&1
	@! ./pith run tests/types_err_typed_immutable.pi > /dev/null 2>&1
	@! ./pith run tests/types_err_u8_overflow.pi > /dev/null 2>&1
	@! ./pith run tests/types_err_u8_negative.pi > /dev/null 2>&1
	@! ./pith run tests/types_err_i8_overflow.pi > /dev/null 2>&1
	# [PARSER] error-expectation tests: diagnostics, not segfaults
	@! ./pith run tests/lexer_unterminated.pi > /dev/null 2>&1
	@! ./pith run tests/lexer_ident_nonascii.pi > /dev/null 2>&1
	@! ./pith run tests/lexer_int_overflow.pi > /dev/null 2>&1
	# [PARSER] generated deep-nesting torture (within the C-stack guards)
	@awk 'BEGIN { for (i = 0; i < 200; i++) print "if 1"; print "print \"deep blocks ok\""; for (i = 0; i < 200; i++) print "end" }' > tests/gen_deep_blocks.pi
	./pith run tests/gen_deep_blocks.pi 2>/dev/null | grep -q "deep blocks ok"
	@awk 'BEGIN { for (i = 0; i < 400; i++) print "if 1"; print "print \"unreachable\""; for (i = 0; i < 400; i++) print "end" }' > tests/gen_deep_over.pi
	@! ./pith run tests/gen_deep_over.pi > /dev/null 2>&1
	@awk 'BEGIN { printf "x = "; for (i = 0; i < 200; i++) printf "("; printf "1"; for (i = 0; i < 200; i++) printf ")"; print "" }' > tests/gen_deep_parens.pi
	@! ./pith run tests/gen_deep_parens.pi > /dev/null 2>&1
	@rm -f tests/gen_deep_blocks.pi tests/gen_deep_over.pi tests/gen_deep_parens.pi
	# [ARC]/[PACKAGE]/[CODEGEN] C harnesses
	$(CC) $(POSIXDEF) -std=c99 -O2 tests/harness/arc_stress.c runtime/memory.o -o tests/harness/arc_stress
	./tests/harness/arc_stress
	$(CC) $(POSIXDEF) -std=c99 -O2 tests/harness/arc_cycle.c runtime/memory.o -o tests/harness/arc_cycle
	./tests/harness/arc_cycle
	$(CC) $(POSIXDEF) -std=c99 -O2 tests/harness/tar_security.c src/tar.o -o tests/harness/tar_security
	./tests/harness/tar_security
	$(CC) $(POSIXDEF) -std=c99 -O2 tests/harness/div0.c -o tests/harness/div0
	./tests/harness/div0 ./pith tests/codegen_div0.pi
	@rm -f tests/harness/arc_stress tests/harness/arc_cycle tests/harness/tar_security tests/harness/div0
	# [ENGINE]/[PACKAGE] sandboxed in a throwaway HOME: nothing may
	# leak into the real user home or the project tree
	@rm -rf /tmp/opencode/pith_check
	@mkdir -p /tmp/opencode/pith_check/pkgsample
	@printf 'payload\n' > /tmp/opencode/pith_check/pkgsample/readme.txt
	@printf '[dependencies]\ndemo = "./pkgsample"\n' > /tmp/opencode/pith_check/pith.toml
	HOME=/tmp/opencode/pith_check "$$PWD/pith" engine install 0.1.0 > /dev/null
	HOME=/tmp/opencode/pith_check "$$PWD/pith" engine list | grep -q "0.1.0"
	HOME=/tmp/opencode/pith_check "$$PWD/pith" engine use 0.1.0 > /dev/null
	HOME=/tmp/opencode/pith_check "$$PWD/pith" engine list | grep -q "active default"
	cd /tmp/opencode/pith_check && HOME=/tmp/opencode/pith_check "$$OLDPWD/pith" pkg install > /dev/null
	@test -f /tmp/opencode/pith_check/pith.lock
	@test -d /tmp/opencode/pith_check/.pith/pkgs
	cd /tmp/opencode/pith_check && HOME=/tmp/opencode/pith_check "$$OLDPWD/pith" pkg sync | grep -q "verified"
	# cleanup invariants: nothing the suite created may survive it
	@rm -rf /tmp/opencode/pith_check
	@for f in test_audit test_ffi test_while test_logical test_fn_call test_os_net test_proc test_proc_exit tests/test_scratch.txt pith.lock tests/gen_deep_blocks.pi tests/gen_deep_over.pi tests/gen_deep_parens.pi tests/harness/arc_stress tests/harness/arc_cycle tests/harness/tar_security tests/harness/div0; do \
		if [ -e "$$f" ]; then \
			echo "check: residue left behind: $$f" >&2; \
			exit 1; \
		fi; \
	done
	@if [ -d restored_workspace ]; then \
		echo "check: residue left behind: restored_workspace" >&2; \
		exit 1; \
	fi
	@if [ -e "$$HOME/.pith/toolchains" ]; then \
		echo "check: residue left behind: ~/.pith/toolchains" >&2; \
		exit 1; \
	fi
	@echo "all checks passed (workspace clean)"

test-wine: $(PITH) $(VENDOR_QBE)/qbe
	@sh scripts/test_wine.sh

test-darwin: $(PITH) $(VENDOR_QBE)/qbe
	@sh scripts/test_darwin.sh

clean:
	rm -f $(PITH_OBJS) $(RUNTIME_OBJS) $(RUNTIME_LIB) $(PITH) pith.exe test_audit test_ffi test_types test_while test_logical test_fn_call test_os_net test_proc test_proc_exit tests/test_scratch.txt tests/harness/arc_stress tests/harness/arc_cycle tests/harness/tar_security tests/harness/div0 tests/gen_*.pi pith.lock
	rm -rf restored_workspace .pith
	-cd $(VENDOR_QBE) && $(MAKE) clean
	rm -f $(VENDOR_QBE)/libqbe.a

distclean: clean
	-cd $(VENDOR_TCC) && $(MAKE) clean
	rm -f $(VENDOR_TCC)/config.mak $(VENDOR_TCC)/config.h
	-cd $(VENDOR_QBE) && $(MAKE) clean
	rm -f $(VENDOR_QBE)/libqbe.a

help:
	@echo "make           build the pith binary and runtime"
	@echo "make check     build and run the verification tests"
	@echo "make test-wine run Windows cross-compilation and Wine test suite"
	@echo "make test-darwin run Darwin Mach-O cross-compilation test suite"
	@echo "make clean     remove build artifacts"
	@echo "make distclean also clean the vendored tcc build"
