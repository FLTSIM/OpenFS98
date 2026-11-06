/* Bounded native DirectPlay peer for the private actual-owner UI test.
 * Read only the retail session's application GUID, then join through public
 * DirectPlay APIs. No game state is written or owner callback replaced. */
#define COBJMACROS
#define DIRECTPLAY_VERSION 0x0500
#include <windows.h>
#include <objbase.h>
#include <dplay.h>
#include <dplobby.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DPSESSIONDESC2 s_session;
static BOOL s_found;

static BOOL WINAPI peer_session(const DPSESSIONDESC2 *session, DWORD *timeout,
    DWORD flags, void *context)
{
    (void)timeout; (void)context;
    if ((flags & DPESC_TIMEDOUT) || !session) return FALSE;
    s_session = *session;
    s_session.lpszSessionNameA = NULL;
    s_session.lpszPasswordA = NULL;
    s_found = TRUE;
    return FALSE;
}

int main(int argc, char **argv)
{
    LPDIRECTPLAY3A direct = NULL;
    LPDIRECTPLAYLOBBYA lobby = NULL;
    LPDIRECTPLAYLOBBY2A lobby2 = NULL;
    HRESULT (WINAPI *create_lobby)(GUID *, LPDIRECTPLAYLOBBYA *, IUnknown *, void *, DWORD);
    HANDLE host, ready;
    DWORD host_pid, start, attempt, bytes;
    SIZE_T copied;
    GUID provider = DPSPGUID_TCPIP;
    DPSESSIONDESC2 filter = {0};
    DPNAME name = {0};
    DPID player = 0, from, to;
    HRESULT result;
    HMODULE module;
    char event_name[96], log_path[MAX_PATH];
    uint32_t packet[4096];
    uint8_t address[1024];
    DWORD address_size = sizeof(address);
    DPCOMPOUNDADDRESSELEMENT elements[2];
    char hostname[] = "127.0.0.1";
    int received = 0;
    uint32_t handshake = 0;
    if (GetEnvironmentVariableA("MPD_OWNER_PEER_LOG", log_path, sizeof(log_path)))
        (void)freopen(log_path, "w", stdout);
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc != 2) return 2;
    host_pid = strtoul(argv[1], NULL, 0);
    host = OpenProcess(PROCESS_VM_READ, FALSE, host_pid);
    filter.dwSize = sizeof(filter);
    /* Retail MULTIP .data at +0xd000, session descriptor application +0x9d8. */
    if (!host || !ReadProcessMemory(host, (const void *)(uintptr_t)0x2085d9d8,
            &filter.guidApplication, sizeof(GUID), &copied) || copied != sizeof(GUID)) {
        printf("PEER read application failed winerr=%lu\n", (unsigned long)GetLastError());
        if (host) CloseHandle(host);
        return 2;
    }
    /* HOST enables its main HWND before MULTIP finishes session startup.
     * Wait for the real local player, rather than racing provider startup. */
    for (attempt = 0; attempt < 100; ++attempt) {
        DWORD connected = 0, local = 0, local_id = 0, players = 0;
        ReadProcessMemory(host, (const void *)(uintptr_t)0x2085daac, &connected, sizeof(connected), &copied);
        ReadProcessMemory(host, (const void *)(uintptr_t)0x2085da3c, &local, sizeof(local), &copied);
        ReadProcessMemory(host, (const void *)(uintptr_t)0x2085d660, &players, sizeof(players), &copied);
        if (local) ReadProcessMemory(host, (const void *)(uintptr_t)(local + 4), &local_id, sizeof(local_id), &copied);
        if (connected && players && local_id) break;
        Sleep(50);
    }
    if (attempt == 100 || !ReadProcessMemory(host, (const void *)(uintptr_t)0x2085d9d8,
            &filter.guidApplication, sizeof(GUID), &copied) || copied != sizeof(GUID)) {
        printf("PEER host startup not ready\n");
        CloseHandle(host);
        return 1;
    }
    printf("PEER host ready application=%08lx-%04x-%04x\n", (unsigned long)filter.guidApplication.Data1,
        filter.guidApplication.Data2, filter.guidApplication.Data3);
    CloseHandle(host);
    CoInitialize(NULL);
    module = LoadLibraryA("dplayx.dll");
    if (!module) return 2;
    result = CoCreateInstance(&CLSID_DirectPlay, NULL, CLSCTX_INPROC_SERVER,
        &IID_IDirectPlay3A, (void **)&direct);
    printf("PEER create3=%08lx\n", (unsigned long)result);
    if (FAILED(result)) return 1;
    create_lobby = (void *)GetProcAddress(module, "DirectPlayLobbyCreateA");
    if (!create_lobby) return 2;
    result = create_lobby(NULL, &lobby, NULL, NULL, 0);
    printf("PEER lobby=%08lx\n", (unsigned long)result);
    if (FAILED(result)) return 1;
    result = IDirectPlayLobby_QueryInterface(lobby, &IID_IDirectPlayLobby2A, (void **)&lobby2);
    if (FAILED(result)) return 1;
    elements[0].guidDataType = DPAID_ServiceProvider;
    elements[0].dwDataSize = sizeof(provider);
    elements[0].lpData = &provider;
    elements[1].guidDataType = DPAID_INet;
    elements[1].dwDataSize = sizeof(hostname);
    elements[1].lpData = hostname;
    result = IDirectPlayLobby_CreateCompoundAddress(lobby2, elements, 2, address, &address_size);
    printf("PEER address=%08lx bytes=%lu\n", (unsigned long)result, (unsigned long)address_size);
    if (FAILED(result)) return 1;
    result = IDirectPlay3_InitializeConnection(direct, address, 0);
    printf("PEER initialize=%08lx\n", (unsigned long)result);
    if (FAILED(result)) return 1;
    IDirectPlayLobby_Release(lobby2);
    IDirectPlayLobby_Release(lobby);
    for (attempt = 0; attempt < 5 && !s_found; ++attempt) {
        result = IDirectPlay3_EnumSessions(direct, &filter, 1000, peer_session, NULL, DPENUMSESSIONS_ALL);
        printf("PEER enum attempt=%lu result=%08lx found=%u\n",
            (unsigned long)attempt, (unsigned long)result, (unsigned)s_found);
        if (!s_found) Sleep(100);
    }
    if (!s_found) return 1;
    result = IDirectPlay3_Open(direct, &s_session, DPOPEN_JOIN);
    printf("PEER join=%08lx\n", (unsigned long)result);
    if (FAILED(result)) return 1;
    name.dwSize = sizeof(name);
    name.lpszShortNameA = "MP_DIALOG native peer";
    /* FS98's real DPNAME metadata: client / pilot. */
    name.lpszLongNameA = "CP";
    result = IDirectPlay3_CreatePlayer(direct, &player, &name, NULL, NULL, 0, 0);
    printf("PEER player=%08lx id=%08lx\n", (unsigned long)result, (unsigned long)player);
    if (FAILED(result)) return 1;
    start = GetTickCount();
    while (GetTickCount() - start < 45000) {
        from = to = 0;
        bytes = sizeof(packet);
        result = IDirectPlay3_Receive(direct, &from, &to, DPRECEIVE_ALL, packet, &bytes);
        if (SUCCEEDED(result)) {
            printf("PEER receive from=%08lx to=%08lx bytes=%lu tag=%08lx\n",
                (unsigned long)from, (unsigned long)to, (unsigned long)bytes,
                (unsigned long)(bytes >= 4 ? packet[0] : 0));
            if (from != DPID_SYSMSG && bytes >= 8 && packet[0] == 0x1007) {
                uint32_t reply[3] = {0x1003, 4, player};
                result = IDirectPlay3_Send(direct, player, from, DPSEND_GUARANTEED, reply, sizeof(reply));
                printf("PEER type reply=%08lx\n", (unsigned long)result);
                if (SUCCEEDED(result)) handshake |= 1;
            } else if (from != DPID_SYSMSG && bytes >= 8 && packet[0] == 0x100f) {
                char title[260];
                uint32_t reply[72] = {0x100e, 0, 0};
                GetPrivateProfileStringA("AIRtoCONTAINER", "SIM1", "Cessna Skylane 182R RG",
                    title, sizeof(title), "C:\\FS98\\FLTSIM98.CFG");
                reply[1] = (uint32_t)strlen(title) + 1;
                memcpy(reply + 3, title, reply[1]);
                result = IDirectPlay3_Send(direct, player, from, DPSEND_GUARANTEED, reply, reply[1] + 12);
                printf("PEER title reply=%08lx title=%s\n", (unsigned long)result, title);
                if (SUCCEEDED(result)) handshake |= 2;
            }
            if (handshake == 3) {
                wsprintfA(event_name, "Local\\MPDialogPeerReady-%lu", (unsigned long)host_pid);
                ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, event_name);
                if (ready) { SetEvent(ready); CloseHandle(ready); }
                handshake |= 4;
                printf("PEER handshake complete\n");
            }
            if (from != DPID_SYSMSG && bytes >= 8 && packet[0] == 0x1016 && packet[1] == 0) {
                printf("PEER RECEIVED_1016\n");
                wsprintfA(event_name, "Local\\MPDialogPeerRemoved-%lu", (unsigned long)host_pid);
                ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, event_name);
                if (ready) { SetEvent(ready); CloseHandle(ready); }
                received = 1;
                break;
            }
        } else if (result != DPERR_NOMESSAGES) {
            printf("PEER receive error=%08lx\n", (unsigned long)result);
            break;
        } else Sleep(10);
    }
    printf("PEER end received_1016=%d\n", received);
    /* This disposable process owns the peer. The controller bounds process
     * shutdown too, since this old runtime can stall inside Close/Release. */
    IDirectPlay3_DestroyPlayer(direct, player);
    IDirectPlay3_Close(direct);
    IDirectPlay3_Release(direct);
    FreeLibrary(module);
    CoUninitialize();
    return received ? 0 : 1;
}
