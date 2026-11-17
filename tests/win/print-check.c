/* print-check: where printing stops in a Wine prefix. Run it in the app's prefix.
 *
 *   print-check.exe
 *
 * First the folders this program is given for printer drivers and print processors, and
 * whether Wine's driver (wineps.drv) and processor (winprint.dll) are in them: an x64 program
 * on an ARM64 Mac was given the x64 folders, which hold neither, so nothing printed. Then it
 * loads wineps.drv by name as gdi32 does, lists the printers Windows
 * sees (from the Mac's CUPS) and the default one, then opens a device context on it and starts
 * and cancels a document into a file (never the printer), as an app's Print does. Each step prints its result or error code.
 *
 * Build: toolchains/llvm-mingw/bin/x86_64-w64-mingw32-clang -Os -municode -o print-check.exe tests/win/print-check.c -lgdi32 -lwinspool
 */
#include <windows.h>
#include <stdio.h>

static BOOL in_folder(const wchar_t *dir, const wchar_t *file)
{
    wchar_t path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%ls\\%ls", dir, file);
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

int wmain(void)
{
    wchar_t drivers[MAX_PATH], processors[MAX_PATH];
    DWORD got;
    BOOL ready = TRUE;
    if (GetPrinterDriverDirectoryW(NULL, NULL, 1, (BYTE *)drivers, sizeof(drivers), &got))
    {
        wcscat(drivers, L"\\3");
        printf("driver folder: %ls, wineps.drv %s\n", drivers, in_folder(drivers, L"wineps.drv") ? "there" : "missing");
        ready &= in_folder(drivers, L"wineps.drv");
    }
    else
        printf("driver folder: none (error %lu)\n", GetLastError()), ready = FALSE;
    if (GetPrintProcessorDirectoryW(NULL, NULL, 1, (BYTE *)processors, sizeof(processors), &got))
    {
        printf("processor folder: %ls, winprint.dll %s\n", processors,
               in_folder(processors, L"winprint.dll") ? "there" : "missing");
        ready &= in_folder(processors, L"winprint.dll");
    }
    else
        printf("processor folder: none (error %lu)\n", GetLastError()), ready = FALSE;
    if (!ready) printf("not ready to print: Wine's driver isn't where this program looks\n");

    HMODULE driver = LoadLibraryW(L"wineps.drv");
    printf("load wineps.drv: %s (error %lu)\n", driver ? "loaded" : "failed", driver ? 0 : GetLastError());

    DWORD needed = 0, count = 0;
    EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, NULL, 0, &needed, &count);
    BYTE *buffer = needed ? malloc(needed) : NULL;
    if (buffer && EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, buffer, needed, &needed, &count))
    {
        PRINTER_INFO_2W *printers = (PRINTER_INFO_2W *)buffer;
        for (DWORD i = 0; i < count; i++)
            printf("printer: %ls, driver %ls, port %ls\n", printers[i].pPrinterName, printers[i].pDriverName,
                   printers[i].pPortName);
    }
    printf("printers: %lu\n", count);

    wchar_t name[256];
    DWORD size = 256;
    if (!GetDefaultPrinterW(name, &size))
    {
        printf("default printer: none (error %lu)\n", GetLastError());
        return ready ? 0 : 1;
    }
    printf("default printer: %ls\n", name);

    HDC dc = CreateDCW(L"WINSPOOL", name, NULL, NULL);
    printf("device context: %s (error %lu)\n", dc ? "opened" : "failed", dc ? 0 : GetLastError());
    if (!dc) return 1;
    DOCINFOW doc = {.cbSize = sizeof(doc), .lpszDocName = L"print-check", .lpszOutput = L"C:\\ea-check\\out\\print-check.ps"};
    int job = StartDocW(dc, &doc);
    /* a document that starts is cancelled at once: nothing reaches the printer */
    printf("start document: %s (job %d, error %lu)\n", job > 0 ? "started" : "failed", job, job > 0 ? 0 : GetLastError());
    if (job > 0) AbortDoc(dc);
    DeleteDC(dc);
    return job > 0 && ready ? 0 : 1;
}
