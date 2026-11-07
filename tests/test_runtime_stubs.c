/* Link stubs for tests/test_runtime.c, which links the GPU library but not src/probe.c. */
void bb_write_minidump(const char *reason, void *exception_pointers) { (void)reason; (void)exception_pointers; }
