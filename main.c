#include <Windows.h>
#include <stdio.h>

// Types are indexed in the kernel using a UCHAR so their can be only 256
// and a type info buffer contains a struct with a UNICODE_STRING whose size
// is stored in a USHORT (65536 bytes at most)
#define MAX_TYPES_BUFFER_SIZE (256 * (sizeof(OBJECT_TYPE_INFORMATION) + 65536))
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004)
#define NT_SUCCESS(status) (((NTSTATUS)(status)) >= 0)

// Undocumented types from https://processhacker.sourceforge.io/doc/

typedef enum {
    ObjectBasicInformation,
    ObjectNameInformation,
    ObjectTypeInformation,
    ObjectAllInformation
} OBJECT_INFORMATION_CLASS, * POBJECT_INFORMATION_CLASS;

typedef struct
{
   USHORT Length;
   USHORT MaximumLength;
   _Field_size_bytes_part_(MaximumLength, Length) PWCH Buffer;
} UNICODE_STRING, * PUNICODE_STRING;

typedef struct
{
   UNICODE_STRING TypeName;
   ULONG TotalNumberOfObjects;
   ULONG TotalNumberOfHandles;
   ULONG TotalPagedPoolUsage;
   ULONG TotalNonPagedPoolUsage;
   ULONG TotalNamePoolUsage;
   ULONG TotalHandleTableUsage;
   ULONG HighWaterNumberOfObjects;
   ULONG HighWaterNumberOfHandles;
   ULONG HighWaterPagedPoolUsage;
   ULONG HighWaterNonPagedPoolUsage;
   ULONG HighWaterNamePoolUsage;
   ULONG HighWaterHandleTableUsage;
   ULONG InvalidAttributes;
   GENERIC_MAPPING GenericMapping;
   ULONG ValidAccessMask;
   BOOLEAN SecurityRequired;
   BOOLEAN MaintainHandleCount;
   UCHAR TypeIndex; // since WINBLUE
   CHAR ReservedByte;
   ULONG PoolType;
   ULONG DefaultPagedPoolCharge;
   ULONG DefaultNonPagedPoolCharge;
} OBJECT_TYPE_INFORMATION, * POBJECT_TYPE_INFORMATION;

typedef struct {
   ULONG                   NumberOfObjectsTypes;
   OBJECT_TYPE_INFORMATION ObjectTypeInformation[1];
} OBJECT_ALL_INFORMATION, * POBJECT_ALL_INFORMATION;

typedef NTSTATUS (NTAPI *PNtQueryObject)(
   _In_opt_ HANDLE Handle,
   _In_ OBJECT_INFORMATION_CLASS ObjectInformationClass,
   _Out_writes_bytes_opt_(ObjectInformationLength) PVOID ObjectInformation,
   _In_ ULONG ObjectInformationLength,
   _Out_opt_ PULONG ReturnLength
);

int main()
{
   int res = 0;
   HMODULE hNtdll = NULL;
   PNtQueryObject pNtQueryObject = NULL;
   POBJECT_ALL_INFORMATION pBuffer = NULL;
   ULONG ulBufferSize = MAX_TYPES_BUFFER_SIZE;
   NTSTATUS status = 0;
   
   hNtdll = LoadLibrary(TEXT("ntdll.dll"));
   if (hNtdll == NULL)
   {
      res = GetLastError();
      fprintf(stderr, " [!] Unable to get a handle to ntdll.dll (error %u)\n", res);
      goto cleanup;
   }

   pNtQueryObject = (PNtQueryObject)GetProcAddress(hNtdll, "NtQueryObject");
   if (pNtQueryObject == NULL)
   {
      res = GetLastError();
      fprintf(stderr, " [!] Unable to get a pointer to NtQueryObject (error %u)\n", res);
      goto cleanup;
   }

   pBuffer = (POBJECT_ALL_INFORMATION)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ulBufferSize);
   if (pBuffer == NULL)
   {
      res = GetLastError();
      fprintf(stderr, " [!] Unable to allocate memory (error %u)\n", res);
      goto cleanup;
   }

   // We can't just call NtQueryObject(NULL, ObjectAllInformation, NULL, 0, &getMeTheSize)
   // because the first 4 userland bytes are directly used as the final counter by the kernel,
   // so it needs them be allocated... Plus the returned "required buffer size" is not
   // computed correctly, which often triggers 16-byte heap overflows/corruption...
   // This syscall is so broken, at this point it's safer to use a "larger than will ever be
   // necessary" hardcoded buffer size.
   status = pNtQueryObject(NULL, ObjectAllInformation, (PVOID)pBuffer, ulBufferSize, &ulBufferSize);
   if (!NT_SUCCESS(status))
   {
      res = status;
      fprintf(stderr, " [!] NtQueryObject(ObjectAllInformation) failed (status 0x%08X)\n", status);
      goto cleanup;
   }

   printf(" [.] Fetched %u object types\n\n", pBuffer->NumberOfObjectsTypes);
   printf(" %30s | GENERIC_READ | GENERIC_WRITE | GENERIC_EXEC | GENERIC_ALL | VALID_MASK | SEC_REQUIRED\n", "Object type name");
   printf(" %30s-+--------------+---------------+--------------+-------------+------------+-------------\n", "----------------");
   
   POBJECT_TYPE_INFORMATION pTypeInfo = &(pBuffer->ObjectTypeInformation[0]);
   for (ULONG i = 0; i < pBuffer->NumberOfObjectsTypes; i++)
   {
      // Type structures are aligned on pointer size
      DWORD dwOffsetToNextType = (pTypeInfo->TypeName.MaximumLength + sizeof(PVOID) - 1) & ~(sizeof(PVOID) - 1);

      printf(" %30.*ws |  0x%08X  |   0x%08X  |  0x%08X  |  0x%08X | 0x%08X | %s\n",
             pTypeInfo->TypeName.Length / 2,
             pTypeInfo->TypeName.Buffer,
             pTypeInfo->GenericMapping.GenericRead,
             pTypeInfo->GenericMapping.GenericWrite,
             pTypeInfo->GenericMapping.GenericExecute,
             pTypeInfo->GenericMapping.GenericAll,
             pTypeInfo->ValidAccessMask,
             pTypeInfo->SecurityRequired ? "YES" : "NO");

      pTypeInfo = (POBJECT_TYPE_INFORMATION)(((PBYTE)pTypeInfo->TypeName.Buffer) + dwOffsetToNextType);
   }

cleanup:
   if (pBuffer != NULL)
      HeapFree(GetProcessHeap(), 0, pBuffer);
   return 0;
}
