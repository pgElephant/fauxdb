# PostgreSQL BSON Extension Makefile
#
# This extension provides a BSON data type with MongoDB compatibility
#
# Copyright (c) 2024, PostgreSQL Global Development Group
#
# contrib/bson/Makefile

# Use PGXS for extension building
USE_PGXS = 1

MODULE_big = bson
OBJS = \
	src/bson.o \
	src/bson_op.o \
	src/bson_agg.o

EXTENSION = bson
DATA = bson--1.0.sql
PGFILEDESC = "BSON - MongoDB-compatible BSON data type"

# Test configuration
REGRESS = bson_basic bson_operators bson_indexes
REGRESS_OPTS = --inputdir=test

# Documentation
DOCS = README.md

# PostgreSQL configuration - prefer explicit path, fallback to PATH
ifndef PG_CONFIG
	PG_CONFIG := $(shell which /usr/local/pgsql.17/bin/pg_config 2>/dev/null || which pg_config 2>/dev/null)
endif

# Verify pg_config exists and is executable
ifeq ($(shell test -x "$(PG_CONFIG)" && echo yes),yes)
	PGXS := $(shell $(PG_CONFIG) --pgxs)
else
	$(error pg_config not found. Please install PostgreSQL development packages or set PG_CONFIG environment variable)
endif

# MongoDB libbson configuration using pkg-config
# First check if pkg-config and bson2 are available
BSON_PKG_EXISTS := $(shell pkg-config --exists bson2 2>/dev/null && echo yes)
ifeq ($(BSON_PKG_EXISTS),yes)
	BSON_CFLAGS := $(shell pkg-config --cflags bson2)
	BSON_LIBS := $(shell pkg-config --libs bson2)
else
	# Fallback to manual configuration
	BSON_CFLAGS := -I/opt/homebrew/Cellar/mongo-c-driver/2.1.0/include/bson-2.1.0
	BSON_LIBS := -L/opt/homebrew/Cellar/mongo-c-driver/2.1.0/lib -lbson2
endif

# Compiler and linker flags
PG_CPPFLAGS += -I$(srcdir)/include $(BSON_CFLAGS)
SHLIB_LINK += $(BSON_LIBS)
PG_CFLAGS += -std=c99 -Wno-declaration-after-statement

include $(PGXS)
