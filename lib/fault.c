/*
 * fault.c — the fault-injection registry (only active under SECRET_FAULTS).
 *
 * $SECRET_FAULT is a comma-separated list of armed failpoint names; a
 * matching SECRET_FAULT("name") call returns 1 once per program run for
 * each armed name, then disarms it (so a single fault fires deterministi-
 * cally without looping). In a normal build this file is empty.
 */
#ifdef SECRET_FAULTS
#include <stdlib.h>
#include <string.h>

int secret_fault(const char *name)
{
	static char fired[64][32];
	static int  nfired = 0;

	const char *env = getenv("SECRET_FAULT");
	if (!env || !*env)
		return 0;

	size_t nl = strlen(name);
	for (const char *p = env; *p; ) {
		const char *comma = strchr(p, ',');
		size_t len = comma ? (size_t)(comma - p) : strlen(p);
		if (len == nl && strncmp(p, name, nl) == 0) {
			/* fire only once per name per run */
			for (int i = 0; i < nfired; i++)
				if (strcmp(fired[i], name) == 0)
					return 0;
			if (nfired < 64 && nl < 32) {
				memcpy(fired[nfired], name, nl);
				fired[nfired][nl] = '\0';
				nfired++;
			}
			return 1;
		}
		p = comma ? comma + 1 : p + len;
	}
	return 0;
}
#endif /* SECRET_FAULTS */
