# extfs/util.h

## `typedef enum {`


Status codes for relative path calculation


---

## `rel_status calculate_relative_path(char *relative, const char *from, const char *to, size_t size);`


Calculate relative path from one absolute path to another

- **`relative`**: Output buffer for relative path
- **`from`**: Source absolute path
- **`to`**: Target absolute path
- **`size`**: Size of output buffer
- **Returns**: rel_status code indicating success or specific error


---

