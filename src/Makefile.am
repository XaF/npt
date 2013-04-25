
AM_CFLAGS = -DBUILD_DATE="\"$$(LANG= date)\""

bin_PROGRAMS = npt
npt_SOURCES = npt.c
if USE_LTTNG_UST
npt_SOURCES += tracepoint/create_ust_probes.c
endif

