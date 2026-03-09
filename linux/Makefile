# Makefile for Linux build of Craig's VI editor
#
# Uses ncurses for terminal I/O.  Builds with GCC.
#
# Usage:
#   make            - build vi
#   make clean      - remove build artifacts
#   make debug      - build with debug symbols

CC      = gcc
CFLAGS  = -D__UNIX__ -D__LINUX__ \
          -I. -I../h \
          -Wall
LDFLAGS = -lncurses

# For debug builds
ifdef DEBUG
  CFLAGS += -g -O0 -DDEBUG
else
  CFLAGS += -O2
endif

BUILDDIR = obj

TARGET = vi

# ---- Core editor sources (c/*.c) ----
# All the platform-independent editor logic.
# NOTE: dosdir.c is excluded — replaced by unix/unixdir.c
CORE_SRCS = \
	../c/abandon.c ../c/addstr.c ../c/alias.c ../c/autosave.c \
	../c/bnddata.c ../c/change.c ../c/cledit.c ../c/clglob.c \
	../c/clread.c ../c/clset.c ../c/clsubs.c ../c/cmdline.c \
	../c/cstatus.c ../c/cut.c ../c/dat.c ../c/dc.c \
	../c/delete.c ../c/dir.c ../c/dirdisp.c ../c/dotmode.c \
	../c/editdc.c ../c/editins.c ../c/editmain.c ../c/editmv.c \
	../c/error.c ../c/ex.c ../c/exappend.c ../c/exdata.c \
	../c/expandfn.c ../c/expr.c ../c/fcb.c ../c/fcbblock.c \
	../c/fcbdisk.c ../c/fcbdmp.c ../c/fcbdup.c ../c/fcbems.c \
	../c/fcbmem.c ../c/fcbmerge.c ../c/fcbsplit.c ../c/fcbswap.c \
	../c/fcbxmem.c ../c/fcbxmint.c ../c/fcbxms.c ../c/file.c \
	../c/filelast.c ../c/filemove.c ../c/filenew.c ../c/filesave.c \
	../c/filestk.c ../c/filter.c ../c/findcmd.c ../c/findrx.c \
	../c/fini.c ../c/fmatch.c ../c/fts.c ../c/gcmd.c \
	../c/gencfg.c ../c/getautoi.c ../c/getdir.c ../c/getspcmd.c \
	../c/globals.c ../c/grep.c ../c/help.c ../c/hide.c \
	../c/hist.c ../c/init.c ../c/io.c ../c/key.c \
	../c/lang.c ../c/linecfb.c ../c/linedel.c ../c/linedisp.c \
	../c/linefind.c ../c/lineins.c ../c/linemisc.c ../c/linenew.c \
	../c/linenum.c ../c/lineptr.c ../c/linework.c ../c/lineyank.c \
	../c/llrtns.c ../c/mapkey.c ../c/mark.c ../c/match.c \
	../c/mem.c ../c/misc.c ../c/move.c ../c/name.c \
	../c/op.c ../c/opmove.c ../c/parse.c ../c/parsecl.c \
	../c/pdata.c ../c/printf.c ../c/rcs.c ../c/readstr.c \
	../c/regsub.c ../c/replace.c ../c/rxsupp.c ../c/rxwrap.c \
	../c/samefile.c ../c/savebuf.c ../c/select.c ../c/selrgn.c \
	../c/shove.c ../c/source.c ../c/srcassgn.c ../c/srccs.c \
	../c/srcdata.c ../c/srcexpnd.c ../c/srcexpr.c ../c/srcfile.c \
	../c/srcgen.c ../c/srcgoto.c ../c/srchook.c ../c/srcif.c \
	../c/srcinp.c ../c/srcnextw.c ../c/srcvar.c ../c/sstyle.c \
	../c/sstyle_c.c ../c/sstyle_f.c ../c/sstyle_g.c ../c/sstyle_h.c \
	../c/sstyle_m.c ../c/sstyle_p.c ../c/status.c ../c/tab_e_c.c \
	../c/tab_hell.c ../c/tags.c ../c/time.c ../c/txtobj.c \
	../c/undo.c ../c/undo_do.c ../c/undoclne.c ../c/undostks.c \
	../c/version.c ../c/wingen.c ../c/word.c ../c/xmem.c

# ---- UI layer sources (ui/*.c) ----
UI_SRCS = \
	../ui/adjwin.c ../ui/filesel.c ../ui/hdlmouse.c ../ui/menu.c \
	../ui/mouse.c ../ui/mouseev.c ../ui/ui8086.c ../ui/uiglob.c \
	../ui/winaux.c ../ui/winbrdr.c ../ui/windisp.c ../ui/wininit.c \
	../ui/winnew.c ../ui/winover.c ../ui/winscrl.c ../ui/winshow.c \
	../ui/winsize.c ../ui/winswap.c ../ui/winthumb.c ../ui/wintica.c

# ---- Unix directory support (replaces dosdir.c) ----
UNIX_SRCS = ../unix/unixdir.c

# ---- Linux platform layer (replaces nt/*.c) ----
LINUX_SRCS = \
	linuxmain.c linuxsys.c bioslinux.c int.c spawn.c \
	linuxmous.c linuxcmds.c linuxcompat.c

ALL_SRCS = $(CORE_SRCS) $(UI_SRCS) $(UNIX_SRCS) $(LINUX_SRCS)

# Generate object file names in build directory
OBJS = $(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(ALL_SRCS)))

# VPATH so make can find sources in multiple directories
VPATH = ../c:../ui:../unix:.:../linux

# ---- edbind utility ----
EDBIND       = edbind
EDBIND_SRC   = ../bind/main.c
EDBIND_FLAGS = -D__UNIX__ -D__LINUX__ -I. -I../h -Wall -O2 -include clibext.h

.PHONY: all clean debug bind

all: $(BUILDDIR) $(TARGET)

# Build edbind, then bind dat files into the vi executable.
# Requires EDPATH to point at the dat/ directory.
bind: $(EDBIND)
	EDPATH=../dat ./$(EDBIND) -q $(TARGET)

$(EDBIND): $(EDBIND_SRC) banner.h clibext.h
	$(CC) $(EDBIND_FLAGS) -o $@ $(EDBIND_SRC)

debug:
	$(MAKE) DEBUG=1

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(BUILDDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR) $(TARGET) $(EDBIND)
