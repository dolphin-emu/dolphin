#ifndef MINIUPNPC_DECLSPEC_H_INCLUDED
#define MINIUPNPC_DECLSPEC_H_INCLUDED

#if defined(__GNUC__) && __GNUC__ >= 4
	/* fix dynlib for OS X 10.9.2 and Apple LLVM version 5.0 */
	#define MINIUPNP_LIBSPEC __attribute__ ((visibility ("default")))
#else
	#define MINIUPNP_LIBSPEC
#endif

#endif /* MINIUPNPC_DECLSPEC_H_INCLUDED */

