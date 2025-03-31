// HobbitMultiplayerWindowed.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "HobbitMultiplayerWindowed.h"
#include "../Hobbit Multiplayer/HobbitMultiplayer.h"
#include <shellapi.h>

#define MAX_LOADSTRING 100

// Control IDs
#define IDC_CREATE_SERVER        1001
#define IDC_JOIN_SERVER_LABEL    1002
#define IDC_JOIN_SERVER_EDIT     1003
#define IDC_JOIN_SERVER_BTN      1004
#define IDC_EXIT_SERVER          1005
#define IDC_SIDEBAR_PANEL        300
#define IDC_MAIN_PANEL           301
#define IDC_TOGGLE_1             310
#define IDC_TOGGLE_6             315

// Global Variables:
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

// UI Controls
HWND hCreateServerBtn = nullptr;
HWND hJoinServerLabel = nullptr;
HWND hJoinServerEdit = nullptr;
HWND hJoinServerBtn = nullptr;
HWND hExitServerBtn = nullptr;
HWND hSidebarPanel = nullptr;
HWND hMainPanel = nullptr;

// State management
enum UIState { UI_FIRST, UI_SECOND };
UIState currentUI = UI_FIRST;
bool toggleStates[6] = { false };

HobbitMultiplayer hobbitMultiplayer;
std::thread serverThread;

// Forward declarations
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void CreateSidebar(HWND hWnd);
void CreateMainUI(HWND hParent);
void SwitchUI(UIState newState, HWND hParent);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_HOBBITMULTIPLAYERWINDOWED, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_HOBBITMULTIPLAYERWINDOWED));
    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HOBBITMULTIPLAYERWINDOWED));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_HOBBITMULTIPLAYERWINDOWED);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 600, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        CreateSidebar(hWnd);
        CreateMainUI(hMainPanel);
        break;
    }

    case WM_SIZE:
    {
        RECT rc;
        GetClientRect(hWnd, &rc);
        int sidebarWidth = (int)(rc.right * 0.2);
        MoveWindow(hSidebarPanel, 0, 0, sidebarWidth, rc.bottom, TRUE);
        MoveWindow(hMainPanel, sidebarWidth, 0, rc.right - sidebarWidth, rc.bottom, TRUE);
        break;
    }

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);

        // Handle sidebar toggles
        if (wmId >= IDC_TOGGLE_1 && wmId <= IDC_TOGGLE_6)
        {
            int idx = wmId - IDC_TOGGLE_1;
            toggleStates[idx] = !toggleStates[idx];
            CheckDlgButton(hSidebarPanel, wmId, toggleStates[idx] ? BST_CHECKED : BST_UNCHECKED);
            break;
        }

        // Handle main UI
        if (currentUI == UI_FIRST)
        {
            if (wmId == IDC_CREATE_SERVER)
            {
                serverThread = std::thread([&]() { hobbitMultiplayer.startServerClient(); });
                SwitchUI(UI_SECOND, hMainPanel);
            }
            else if (wmId == IDC_JOIN_SERVER_BTN)
            {
                wchar_t buffer[256] = { 0 };
                GetWindowTextW(hJoinServerEdit, buffer, 256);
                std::string str(buffer, buffer + wcslen(buffer));

                serverThread = std::thread([&]() { hobbitMultiplayer.startClient(str); });
                SwitchUI(UI_SECOND, hMainPanel);
            }
        }
        else if (currentUI == UI_SECOND && wmId == IDC_EXIT_SERVER)
        {
            hobbitMultiplayer.stopServer();
            hobbitMultiplayer.stopClient();

            if (serverThread.joinable()) serverThread.join();
            SwitchUI(UI_FIRST, hMainPanel);
        }

        // Handle menu items
        switch (wmId)
        {
        case IDM_ABOUT:
            ShellExecuteW(nullptr, L"open", L"https://www.youtube.com", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void CreateSidebar(HWND hWnd)
{
    // Create sidebar panel
    hSidebarPanel = CreateWindowW(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        0, 0, 150, 600, hWnd, (HMENU)IDC_SIDEBAR_PANEL, hInst, nullptr);

    // Create main content panel
    hMainPanel = CreateWindowW(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE,
        150, 0, 650, 600, hWnd, (HMENU)IDC_MAIN_PANEL, hInst, nullptr);

    // Create toggle buttons in sidebar
    const wchar_t* labels[] = {
        L"Player Health", L"Inventory", L"Chat",
        L"Map", L"Quests", L"Settings"
    };

    for (int i = 0; i < 6; i++)
    {
        CreateWindowW(L"BUTTON", labels[i],
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            10, 20 + (35 * i), 130, 30,
            hSidebarPanel, (HMENU)(IDC_TOGGLE_1 + i), hInst, nullptr);
    }
}

void CreateMainUI(HWND hParent)
{
    // First UI elements
    hCreateServerBtn = CreateWindowW(L"BUTTON", L"Create Server",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        50, 50, 150, 30, hParent, (HMENU)IDC_CREATE_SERVER, hInst, nullptr);

    hJoinServerLabel = CreateWindowW(L"STATIC", L"Join Server:",
        WS_CHILD | WS_VISIBLE,
        50, 100, 100, 20, hParent, (HMENU)IDC_JOIN_SERVER_LABEL, hInst, nullptr);

    hJoinServerEdit = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        160, 95, 200, 25, hParent, (HMENU)IDC_JOIN_SERVER_EDIT, hInst, nullptr);

    hJoinServerBtn = CreateWindowW(L"BUTTON", L"Join Server",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        380, 95, 120, 30, hParent, (HMENU)IDC_JOIN_SERVER_BTN, hInst, nullptr);

    // Second UI element
    hExitServerBtn = CreateWindowW(L"BUTTON", L"Exit Server",
        WS_CHILD | BS_PUSHBUTTON,
        50, 50, 150, 30, hParent, (HMENU)IDC_EXIT_SERVER, hInst, nullptr);
    ShowWindow(hExitServerBtn, SW_HIDE);
}

void SwitchUI(UIState newState, HWND hParent)
{
    currentUI = newState;
    int showFirst = (newState == UI_FIRST) ? SW_SHOW : SW_HIDE;
    int showSecond = (newState == UI_SECOND) ? SW_SHOW : SW_HIDE;

    ShowWindow(hCreateServerBtn, showFirst);
    ShowWindow(hJoinServerLabel, showFirst);
    ShowWindow(hJoinServerEdit, showFirst);
    ShowWindow(hJoinServerBtn, showFirst);
    ShowWindow(hExitServerBtn, showSecond);

    // Force redraw
    InvalidateRect(hParent, nullptr, TRUE);
    UpdateWindow(hParent);
}