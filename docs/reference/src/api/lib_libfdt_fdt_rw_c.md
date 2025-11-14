# lib/libfdt/fdt_rw.c

## `static int fdt_find_add_string_(void *fdt, const char *s, int *allocated) {`


fdt_find_add_string_() - Find or allocate a string

@fdt: pointer to the device tree to check/adjust
@s: string to find/add
@allocated: Set to 0 if the string was found, 1 if not found and so
allocated. Ignored if can_assume(NO_ROLLBACK)

- **Returns**: offset of string in the string table (whether found or added)


---

