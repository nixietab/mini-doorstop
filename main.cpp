#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ORIGINAL_DLL "EOSSDK-Win64-Shipping_orig.dll"

static char g_dllDir[MAX_PATH] = {0};

static void GetExeDir(char *out, size_t cap) {
  char path[MAX_PATH];
  GetModuleFileNameA(NULL, path, MAX_PATH);
  char *slash = strrchr(path, '\\');
  if (!slash) slash = strrchr(path, '/');
  if (slash) *(slash + 1) = '\0';
  else path[0] = '\0';
  strncpy(out, path, cap - 1);
  out[cap - 1] = '\0';
}

typedef int (*CoreClrInitializeFn)(const char *, const char *, int,
                                   const char **, const char **, void **,
                                   unsigned int *);
typedef int (*CoreClrCreateDelegateFn)(void *, unsigned int, const char *,
                                       const char *, const char *, void **);

// Appends every *.dll under dir to the ';'-joined TPA list.
static void AppendDllsToTpa(const char *dir, char *tpa, size_t cap) {
  char pattern[MAX_PATH];
  size_t dlen = strlen(dir);
  if (dlen > 0 && (dir[dlen - 1] == '\\' || dir[dlen - 1] == '/'))
    snprintf(pattern, sizeof(pattern), "%s*.dll", dir);
  else
    snprintf(pattern, sizeof(pattern), "%s\\*.dll", dir);
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE) return;
  char prefix[MAX_PATH];
  snprintf(prefix, sizeof(prefix), "%s\\", dir);
  do {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      size_t have = strlen(tpa);
      size_t need = have + (have ? 1 : 0) + strlen(prefix) + strlen(fd.cFileName) + 1;
      if (need < cap) {
        if (have) strcat(tpa, ";");
        strcat(tpa, prefix);
        strcat(tpa, fd.cFileName);
      }
    }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
}

static DWORD WINAPI BootBepInExThread(LPVOID param) {
  (void)param;

  char exeDir[MAX_PATH];
  char exePath[MAX_PATH];
  GetExeDir(exeDir, sizeof(exeDir));
  GetModuleFileNameA(NULL, exePath, MAX_PATH);

  // Wait until the IL2CPP runtime has been loaded by UnityPlayer.
  int waitedMs = 0;
  while (!GetModuleHandleA("GameAssembly.dll")) {
    if (waitedMs >= 60000) {
      return 0;
    }
    Sleep(100);
    waitedMs += 100;
  }
  Sleep(500);

  char coreclrPath[MAX_PATH], corlibDir[MAX_PATH], targetDll[MAX_PATH];
  char targetDir[MAX_PATH], appPaths[MAX_PATH];
  snprintf(coreclrPath, sizeof(coreclrPath), "%sdotnet\\coreclr.dll", exeDir);
  snprintf(corlibDir, sizeof(corlibDir), "%sdotnet", exeDir);
  snprintf(targetDll, sizeof(targetDll), "%sBepInEx\\core\\BepInEx.Unity.IL2CPP.dll", exeDir);
  snprintf(targetDir, sizeof(targetDir), "%sBepInEx\\core", exeDir);
  snprintf(appPaths, sizeof(appPaths), "%sdotnet;%sBepInEx\\core", exeDir, exeDir);

  if (GetFileAttributesA(coreclrPath) == INVALID_FILE_ATTRIBUTES ||
      GetFileAttributesA(targetDll) == INVALID_FILE_ATTRIBUTES) {
    return 0;
  }

  // Build an explicit trusted platform assembly list from the runtime folder
  // and BepInEx core folder (more robust than relying on APP_PATHS auto-TPA,
  // which fails delegate resolution with 0x80070002).
  char tpaList[65536];
  tpaList[0] = '\0';
  AppendDllsToTpa(corlibDir, tpaList, sizeof(tpaList));
  AppendDllsToTpa(targetDir, tpaList, sizeof(tpaList));

  // Env vars exactly as Unity Doorstop 4.5.0 sets them.
  SetEnvironmentVariableA("DOORSTOP_INITIALIZED", "TRUE");
  SetEnvironmentVariableA("DOORSTOP_INVOKE_DLL_PATH", targetDll);
  SetEnvironmentVariableA("DOORSTOP_PROCESS_PATH", exePath);
  SetEnvironmentVariableA("DOORSTOP_MANAGED_FOLDER_DIR", corlibDir);
  SetEnvironmentVariableA("DOORSTOP_DLL_SEARCH_DIRS", appPaths);

  HMODULE coreclr = LoadLibraryA(coreclrPath);
  if (!coreclr) {
    return 0;
  }
  CoreClrInitializeFn fnInit = (CoreClrInitializeFn)GetProcAddress(coreclr, "coreclr_initialize");
  CoreClrCreateDelegateFn fnDelegate = (CoreClrCreateDelegateFn)GetProcAddress(coreclr, "coreclr_create_delegate");
  if (!fnInit || !fnDelegate) {
    return 0;
  }

  const char *props[] = {"TRUSTED_PLATFORM_ASSEMBLIES", "APP_PATHS"};
  const char *propValues[] = {tpaList, appPaths};
  void *host = NULL;
  unsigned int domainId = 0;
  int rc = fnInit(exePath, "Doorstop Domain", 2, props, propValues, &host, &domainId);
  if (rc != 0) {
    return 0;
  }

  void (*startup)(void) = NULL;
  rc = fnDelegate(host, domainId, "BepInEx.Unity.IL2CPP",
                  "Doorstop.Entrypoint", "Start", (void **)&startup);
  if (rc != 0 || !startup) {
    return 0;
  }

  startup();
  return 0;
}

static void InitDllDir(HMODULE hModule) {
  char path[MAX_PATH];
  GetModuleFileNameA(hModule, path, MAX_PATH);
  strncpy(g_dllDir, path, MAX_PATH);
  char *dirSlash = strrchr(g_dllDir, '\\');
  if (!dirSlash) dirSlash = strrchr(g_dllDir, '/');
  if (dirSlash) *(dirSlash + 1) = '\0';
  else g_dllDir[0] = '\0';
}

static BOOL LoadOriginalEosSdk() {
  char path[MAX_PATH];
  strncpy(path, g_dllDir, MAX_PATH);
  strcat(path, ORIGINAL_DLL);
  HMODULE h = LoadLibraryA(path);
  return h != NULL;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH: {
    DisableThreadLibraryCalls(hModule);
    InitDllDir(hModule);
    LoadOriginalEosSdk();

    HANDLE hBoot = CreateThread(NULL, 0, BootBepInExThread, NULL, 0, NULL);
    if (hBoot) {
      CloseHandle(hBoot);
    }
    break;
  }
  case DLL_PROCESS_DETACH:
    break;
  }
  return TRUE;
}