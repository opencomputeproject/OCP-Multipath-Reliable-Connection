# -------- Config --------
CC          ?= gcc
CFLAGS      ?= -O2 -Wall -Wextra -std=gnu11
LDFLAGS     ?=
PKG_CONFIG  ?= pkg-config

SRC         ?= mrc_example.c utils.c
TARGET      ?= $(basename $(SRC))

# ---- Locate MRC headers ----
ifndef MRC_H_PATH
  ifneq ($(wildcard $(CURDIR)/../../mrc.h),)
    MRC_H_PATH := $(CURDIR)/../..
  else
    $(error MRC_H_PATH is not defined and mrc.h not found in parent dir)
  endif
endif
CFLAGS += -I$(MRC_H_PATH)

# ---- Provider .so path  ----
MRC_LIB_DIR ?= $(MRC_H_PATH)

# ---- ibverbs via pkg-config (fallback to -libverbs) ----
IBVERBS_CFLAGS := $(shell $(PKG_CONFIG) --cflags libibverbs 2>/dev/null)
IBVERBS_LIBS   := $(shell $(PKG_CONFIG) --libs   libibverbs 2>/dev/null)
ifeq ($(strip $(IBVERBS_LIBS)),)
  IBVERBS_LIBS := -libverbs
endif

# Link flags
LDFLAGS += -L$(MRC_LIB_DIR)
LDLIBS  += -Wl,-rpath,$(MRC_LIB_DIR) $(MRC_LIB_DIR)/lib_mrc.so -lm

# -------- Rules --------
.PHONY: all clean check print-vars

all: check $(TARGET)


$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(IBVERBS_CFLAGS) -o $@ $^ \
	      $(LDFLAGS) $(LDLIBS) $(IBVERBS_LIBS)
	@echo "[OK] Built $@"

# Fail fast with clear messages
check:
	@test -f "$(MRC_H_PATH)/mrc.h" || { echo "[ERR] mrc.h not found in $(MRC_H_PATH)"; exit 1; }
	@test -f "$(MRC_LIB_DIR)/lib_mrc.so" || { echo "[ERR] lib_mrc.so not found in $(MRC_LIB_DIR)"; exit 1; }

clean:
	@rm -f $(TARGET)

print-vars:
	@echo "MRC_H_PATH  = $(MRC_H_PATH)"
	@echo "MRC_LIB_DIR = $(MRC_LIB_DIR)"
	@echo "IBVERBS_LIBS= $(IBVERBS_LIBS)"

# Formatting
FORMAT_SOURCES=$(SRC) utils.h

.PHONY: format
format:
	clang-format --verbose -i $(FORMAT_SOURCES)


.PHONY: lint
lint:
	clang-format --verbose --dry-run $(FORMAT_SOURCES)
