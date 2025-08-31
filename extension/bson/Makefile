# PostgreSQL BSON Extension Makefile
#
# This extension provides a BSON data type with MongoDB compatibility
#
# Copyright (c) 2024, PostgreSQL Global Development Group
#
# contrib/bson/Makefile

MODULE_big = bson
OBJS = \
	$(WIN32RES) \
	src/bson.o \
	src/bson_op.o

EXTENSION = bson
DATA = bson--1.0.sql
PGFILEDESC = "BSON - MongoDB-compatible BSON data type"

# Test configuration
REGRESS = bson_basic bson_operators bson_indexes
REGRESS_OPTS = --inputdir=test

# Documentation
DOCS = README.md doc/*.md

# PostgreSQL build configuration
PG_CONFIG = /usr/local/pgsql/bin/pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)

# mongo-c-driver configuration
MONGO_CFLAGS = $(shell PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig pkg-config --cflags bson2 mongoc2)
MONGO_LIBS = $(shell PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig pkg-config --libs bson2 mongoc2)

# Compiler flags
PG_CPPFLAGS += -I$(srcdir)/include $(MONGO_CFLAGS)
SHLIB_LINK += $(MONGO_LIBS)
CFLAGS += -Wall -Wextra -Werror=implicit-function-declaration

include $(PGXS)
