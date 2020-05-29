# NT Object Types

In the NT kernel, almost everything is stored as an object. This allows reusing many common functions: reference counting, visibility from userland, etc. and most importantly access checks when they are accessed.

When you request a handle to access an object, you must specify an access mask (bit field) indicating what actions you intend to perform on the object:

```C
HANDLE OpenProcess(
  DWORD dwDesiredAccess, <<<
  BOOL  bInheritHandle,
  DWORD dwProcessId
);

BOOL OpenThreadToken(
  HANDLE  ThreadHandle,
  DWORD   DesiredAccess, <<<
  BOOL    OpenAsSelf,
  PHANDLE TokenHandle
);
```

Access masks are documented 32-bit bitfields:

- bits 0 to 15 are "specific" object-type-specific;
- bits 16 to 23 are "standard", common to all object types;
- bit 24 is the right to access the object's system control access list;
- bit 25 is a way to request as many access rights as possible;
- bits 28-31 are "generic", mapped to different of the above depending on the object type.

This project is just a small exploration tool to list all these *generic mappings* for each object type.

See `results/` to see the mapping values for some Windows builds. They changed over time as new object types were added, e.g. see EtwRegistration added in Vista with the new Event Tracing for Windows functionnality:

```
$ diff 5.2.3790_XP_x64.txt 6.0.6002_Vista_SP2.txt
1c1
<  [.] Fetched 31 object types
---
>  [.] Fetched 37 object types
>                 EtwRegistration |  0x0002000D  |   0x00020062  |  0x00020E90  |  0x00020EFF | 0x00120FFF | YES
[...]
```

## Links

- https://docs.microsoft.com/en-us/windows/win32/secauthz/access-rights-and-access-masks
- https://docs.microsoft.com/en-us/windows/win32/secauthz/access-mask
