
AM_CFLAGS = -DBUILD_DATE="\"$$(LANG= date)\""

bin_PROGRAMS = $(top_builddir)/npt
__top_builddir__npt_SOURCES = npt.c
if USE_LTTNG_UST
__top_builddir__npt_SOURCES += tracepoint/create_ust_probes.c
endif

