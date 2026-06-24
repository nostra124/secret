/*
 * fault.h — compile-time fault injection for coverage of defensive
 * branches (OOM, fork/socket/pipe failures).
 *
 * SECRET_FAULT(name) expands to 0 in normal builds, so production code
 * carries zero overhead and no test-only behaviour. The coverage build
 * (`make coverage`, which defines SECRET_FAULTS) turns it into a runtime
 * check of $SECRET_FAULT (a comma-separated list of armed failpoints),
 * so a test can force, e.g., malloc to return NULL and exercise the
 * out-of-memory guard.
 */
#ifndef SECRET_FAULT_H
#define SECRET_FAULT_H

#ifdef SECRET_FAULTS
int secret_fault(const char *name);
#define SECRET_FAULT(name) secret_fault(name)
#else
#define SECRET_FAULT(name) 0
#endif

#endif /* SECRET_FAULT_H */
