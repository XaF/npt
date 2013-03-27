
#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER npt

#if !defined(_TRACEPOINT_UST_NPT_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _TRACEPOINT_UST_NPT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lttng/tracepoint.h>

TRACEPOINT_EVENT(npt, start,
	TP_ARGS(),
	TP_FIELDS()
)

TRACEPOINT_EVENT(npt, loop,
	TP_ARGS(
		int, countloop,
		long, ticks,
		double, duration
	),
	TP_FIELDS(
		ctf_integer(int, countloop, countloop)
		ctf_integer(long, ticks, ticks)
		ctf_float(double, duration, duration)
	)
)

TRACEPOINT_EVENT(npt, stop,
	TP_ARGS(),
	TP_FIELDS()
)

#endif /* _TRACEPOINT_UST_NPT_H */

#undef TRACEPOINT_INCLUDE_FILE
#define TRACEPOINT_INCLUDE_FILE npt/tracepoints.h

/* This part must be outside ifdef protection */
#include <lttng/tracepoint-event.h>

#ifdef __cplusplus 
}
#endif
