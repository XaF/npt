/**
 * Non-Preempt Test (npt) tool
 * Copyright 2012-2013  Raphaël Beamonte <raphael.beamonte@gmail.com>
 *
 * This file is part of Foobar.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version
 * 2 as published by the Free Software Foundation.
 *
 * npt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 * or see <http://www.gnu.org/licenses/>.
 */

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
