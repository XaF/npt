
#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER ust_npt

#if !defined(_TRACEPOINT_UST_NPT_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _TRACEPOINT_UST_NPT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lttng/tracepoint.h>

TRACEPOINT_EVENT(ust_npt, nptloop,
	TP_ARGS(
		int, countloop,
		double, duration
	),
	TP_FIELDS(
		ctf_integer(int, countloop, countloop)
		ctf_float(double, duration, duration)
	)
)

#endif /* _TRACEPOINT_UST_NPT_H */

#undef TRACEPOINT_INCLUDE_FILE
#define TRACEPOINT_INCLUDE_FILE ust_npt.h

/* This part must be outside ifdef protection */
#include <lttng/tracepoint-event.h>

#ifdef __cplusplus 
}
#endif
