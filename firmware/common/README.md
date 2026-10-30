# firmware/common

Shared headers consumed by `analyzer-node` via `-I ../common/include`.

| Header | Purpose |
|--------|---------|
| `usb_analyzer/Role.h` | Role enum + name helpers |

Keep this tree header-only until multiple firmware images need shared `.cpp` units.
