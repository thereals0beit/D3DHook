#include "stdafx.h"

App app;

void* GetDirectXPointerFromMemory(void* pvReplica, DWORD dwVTable, DWORD dwAddress, DWORD dwSize)
{
    for(DWORD* i = (DWORD*) dwAddress; (DWORD) i < (dwAddress + dwSize); i++) { // I don't anticipate this will be an issue, if it is one use BYTE*
        try {
            DWORD dwValue = (DWORD) &i;
            DWORD dwVTableAddress = *(DWORD*) dwValue;

            if(dwValue == (DWORD) pvReplica)
                continue; // We don't want to catch the pointer WE created

            if(dwVTableAddress == dwVTable) {
                app.log("VTable Match (0x%X, 0x%X, 0x%X)\n", i, dwValue, dwVTableAddress);

                return (IDirect3DDevice9*) dwValue;
            }
        } catch( ... ) { /* No other way to check for valid pointers */ }
    }

    return NULL;
}

// Creds: learn_more, Ksbunker (aka [ENiGMA] on doxcoding)
void* FindHeapAddressWithVTable(void* pvReplica, DWORD dwVTable)
{
    app.log("d3d9.dll = 0x%X\n", GetModuleHandle("d3d9.dll"));

    HANDLE hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPHEAPLIST, GetCurrentProcessId() );

    HEAPLIST32 lphl = {0};
    lphl.dwSize = sizeof(HEAPLIST32);

    if( Heap32ListFirst( hSnap, &lphl ) ) {
        do {
            HEAPENTRY32 he = {0};
            he.dwSize = sizeof(HEAPENTRY32);

            if( Heap32First( &he, lphl.th32ProcessID, lphl.th32HeapID ) ) {
                do {
                    void* ptr = GetDirectXPointerFromMemory(pvReplica, dwVTable, he.dwAddress, he.dwBlockSize);

                    if(ptr) {
                        return ptr;
                    }
                } while( Heap32Next( &he ) );
            }

        } while( Heap32ListNext( hSnap, &lphl ) );
    }

    app.log("Failed.\n");

    return NULL;
}

BOOL CreateSearchDevice(IDirect3D9** d3d, IDirect3DDevice9** device)
{
    if(!d3d || !device)
        return FALSE;

    *d3d = NULL;
    *device = NULL;

    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    
    if(!pD3D) {
        return FALSE;
    }

    HWND hWindow = GetDesktopWindow();

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory( &pp, sizeof( pp ) );
    pp.Windowed = TRUE;
    pp.hDeviceWindow = hWindow;
    pp.BackBufferCount = 1;
    pp.BackBufferWidth = 4;
    pp.BackBufferHeight = 4;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    
    IDirect3DDevice9* pDevice = NULL;

    if( SUCCEEDED( pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &pDevice ) ) ) {
        if( pDevice != NULL ) {
            *d3d = pD3D;
            *device = pDevice;
        }
    }

    BOOL returnSuccess = (*d3d != NULL);

    if(returnSuccess == FALSE) {
        if(pD3D) {
            pD3D->Release();
        }
    }

    return returnSuccess;
}

BOOL HookDirectX(IDirect3DDevice9* pDevice)
{
    // Hook DirectX pointer

    return TRUE;
}

DWORD WINAPI lpHookThread(LPVOID lpParam)
{
    IDirect3D9* pSearchD3D = NULL;
    IDirect3DDevice9* pSearchDevice = NULL;

    while( CreateSearchDevice(&pSearchD3D, &pSearchDevice) == FALSE ) {
        Sleep( 1000 );
    }

    app.log("Created Search Device! (0x%X, 0x%X)\n",
        pSearchD3D, pSearchDevice);

    IDirect3DDevice9* pDevice = NULL;

    while(pDevice == NULL) {
        pDevice = (IDirect3DDevice9*) FindHeapAddressWithVTable( pSearchDevice, *(DWORD*) pSearchDevice );

        Sleep(1000);
    }

    app.log("Found Device (0x%X)\n",
        pDevice);

    if(pSearchD3D)
        pSearchD3D->Release();

    if(pSearchDevice)
        pSearchDevice->Release();

    // Hook DirectX here...
    HookDirectX(pDevice);

    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule, DWORD dwReason, LPVOID lpReserved )
{
    if(dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        app.module(hModule);
        app.log("Injected!\n");

        CreateThread(0, 0, lpHookThread, 0, 0, 0);
    }

    return TRUE;
}  
