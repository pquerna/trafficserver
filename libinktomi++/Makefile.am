# libinktomi++ Makefile.am
#
#

noinst_PROGRAMS = mkdfa CompileParseRules
check_PROGRAMS = test_atomic test_freelist test_arena
TESTS = $(check_PROGRAMS)
noinst_LIBRARIES = libinktomi++.a

libinktomi___a_CCASFLAGS = -Wa,--32
libinktomi___a_SOURCES = \
  Allocator.cc \
  Allocator.h \
  Arena.cc \
  Arena.h \
  Bitops.cc \
  Bitops.h \
  Compatability.h \
  DAllocator.h \
  Diags.cc \
  Diags.h \
  DynArray.h \
  HostLookup.cc \
  HostLookup.h \
  ink_aiocb.h \
  ink_align.h \
  ink_apidefs.h \
  ink_args.cc \
  ink_args.h \
  ink_assert.cc \
  ink_assert.h \
  ink_atomic.h \
  ink_auth_api.cc \
  ink_auth_api.h \
  ink_base64.cc \
  ink_base64.h \
  ink_bool.h \
  ink_code.cc \
  ink_code.h \
  ink_defs.cc \
  ink_defs.h \
  InkErrno.h \
  ink_error.cc \
  ink_error.h \
  ink_exception.h \
  ink__exit.c \
  ink_file.cc \
  ink_file.h \
  ink_hash_table.cc \
  ink_hash_table.h \
  ink_hrtime.cc \
  ink_hrtime.h \
  ink_inet.cc \
  ink_inet.h \
  ink_inout.h \
  ink_isolatin_table.cc \
  ink_isolatin_table.h \
  ink_killall.cc \
  ink_killall.h \
  ink_llqueue.h \
  ink_lockfile.h \
  INK_MD5.h \
  ink_memory.cc \
  ink_memory.h \
  ink_mutex.cc \
  ink_mutex.h \
  ink_platform.h \
  InkPool.h \
  ink_port.h \
  ink_pread.h \
  ink_queue.cc \
  ink_queue.h \
  ink_queue_utils.c \
  ink_rand.cc \
  ink_rand.h \
  ink_regex-3.6.h \
  ink_res_init.cc \
  ink_res_mkquery.cc \
  ink_resolver.h \
  ink_resource.cc \
  ink_resource.h \
  ink_rwlock.cc \
  ink_rwlock.h \
  ink_snprintf.cc \
  ink_snprintf.h \
  ink_sock.cc \
  ink_sock.h \
  ink_spill.cc \
  ink_spill.h \
  ink_sprintf.cc \
  ink_sprintf.h \
  ink_stack_trace.cc \
  ink_stack_trace.h \
  ink_string.cc \
  ink_string++.cc \
  ink_string.h \
  ink_string++.h \
  ink_syslog.cc \
  ink_syslog.h \
  ink_thread.cc \
  ink_thread.h \
  ink_time.cc \
  InkTime.cc \
  ink_time.h \
  InkTime.h \
  inktomi++.h \
  ink_unused.h \
  I_Version.h \
  List.h \
  llqueue.cc \
  llqueue.h \
  lockfile.cc \
  MatcherUtils.cc \
  MatcherUtils.h \
  MimeTable.cc \
  MimeTable.h \
  MMH.cc \
  MMH.h \
  ParseRules.h \
  ptrdef.h \
  Ptr.h \
  RawHashTable.cc \
  RawHashTable.h \
  Regex.cc \
  Regex.h \
  Regression.cc \
  Regression.h \
  Resource.cc \
  Resource.h \
  SimpleDBM.cc \
  SimpleDBM.h \
  SimpleTokenizer.h \
  TextBuffer.cc \
  TextBuffer.h \
  Tokenizer.cc \
  Tokenizer.h \
  Version.cc

if SUPPORT_ATOMICS
# support in header
else
libinktomi___a_SOURCES += ink_atomic_solaris_i86pc.s
endif

libinktomi___a_LIBADD = ParseRules.o @LIBOBJS@

mkdfa_SOURCES = mkdfa.c

test_atomic_SOURCES = test_atomic.cc
test_atomic_LDADD = libinktomi++.a @LIBTHREAD@ @LIBTCL@
test_atomic_LDFLAGS = @EXTRA_CXX_LDFLAGS@

test_freelist_SOURCES = test_freelist.cc
test_freelist_LDADD = libinktomi++.a @LIBTHREAD@ @LIBTCL@
test_freelist_LDFLAGS = @EXTRA_CXX_LDFLAGS@

test_arena_SOURCES = test_arena.cc
test_arena_LDADD = libinktomi++.a @LIBTHREAD@ @LIBTCL@
test_arena_LDFLAGS = @EXTRA_CXX_LDFLAGS@

CompileParseRules_SOURCES = CompileParseRules.cc

ParseRules.o: ParseRules.cc CompileParseRules
	./CompileParseRules
	$(CXXCOMPILE) -c $<

clean-local:
	rm -f ParseRulesCType ParseRulesCTypeToLower ParseRulesCTypeToUpper
