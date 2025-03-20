// HobbitMultiplayerWindowed.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "HobbitMultiplayerWindowed.h"
#include "../Hobbit Multiplayer/HobbitMultiplayer.h"

#define MAX_LOADSTRING 100

// Control IDs for first UI
#define IDC_CREATE_SERVER    101
#define IDC_JOIN_SERVER_LABEL 102
#define IDC_JOIN_SERVER_EDIT 103
#define IDC_JOIN_SERVER_BTN  104

// Control ID for second UI
#define IDC_EXIT_SERVER      201

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Global handles for our UI controls
// First UI controls
HWND hCreateServerBtn = nullptr;
HWND hJoinServerLabel = nullptr;
HWND hJoinServerEdit = nullptr;
HWND hJoinServerBtn = nullptr;

// Second UI control
HWND hExitServerBtn = nullptr;

// Enum for our UI state
enum UIState { UI_FIRST, UI_SECOND };
UIState currentUI = UI_FIRST;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_HOBBITMULTIPLAYERWINDOWED, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_HOBBITMULTIPLAYERWINDOWED));

    MSG msg;

    // Main message loop:
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


HobbitMultiplayer hobbitMultiplayer;



std::thread serverThread;


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
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

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 600, 400, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // --- First UI: Create Server and Join Server ---
        // Create Server button
        hCreateServerBtn = CreateWindowW(L"BUTTON", L"Create Server",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            50, 50, 150, 30, hWnd, (HMENU)IDC_CREATE_SERVER, hInst, nullptr);

        // Join Server label
        hJoinServerLabel = CreateWindowW(L"STATIC", L"Join Server:",
            WS_CHILD | WS_VISIBLE,
            50, 100, 100, 20, hWnd, (HMENU)IDC_JOIN_SERVER_LABEL, hInst, nullptr);

        // Join Server edit control
        hJoinServerEdit = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            160, 95, 200, 25, hWnd, (HMENU)IDC_JOIN_SERVER_EDIT, hInst, nullptr);

        // Join Server button
        hJoinServerBtn = CreateWindowW(L"BUTTON", L"Join Server",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            380, 95, 120, 30, hWnd, (HMENU)IDC_JOIN_SERVER_BTN, hInst, nullptr);

        // --- Second UI: Exit Server ---
        // Create Exit Server button, but initially hidden.
        hExitServerBtn = CreateWindowW(L"BUTTON", L"Exit Server",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            50, 50, 150, 30, hWnd, (HMENU)IDC_EXIT_SERVER, hInst, nullptr);
        ShowWindow(hExitServerBtn, SW_HIDE);
    }
    break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // --- First UI Button Handlers ---
        if (currentUI == UI_FIRST)
        {
            if (wmId == IDC_CREATE_SERVER)
            {

                // Start the server in a new thread
                serverThread = std::thread([&]() {
                    hobbitMultiplayer.startServerClient();
                    });

                // Proceed to switch UI.
                currentUI = UI_SECOND;
                // Hide first UI controls.
                ShowWindow(hCreateServerBtn, SW_HIDE);
                ShowWindow(hJoinServerLabel, SW_HIDE);
                ShowWindow(hJoinServerEdit, SW_HIDE);
                ShowWindow(hJoinServerBtn, SW_HIDE);

                // Show second UI control.
                ShowWindow(hExitServerBtn, SW_SHOW);
            }
            else if (wmId == IDC_JOIN_SERVER_BTN)
            {
                // Retrieve input from the join server edit control.
                wchar_t buffer[256] = { 0 };
                GetWindowTextW(hJoinServerEdit, buffer, 256);
                std::wstring wstr(buffer);

                // Convert std::wstring to std::string
                std::string str(wstr.begin(), wstr.end());

                // Start the server in a new thread
                serverThread = std::thread([&]() {
                    hobbitMultiplayer.startClient(str);
                    });

                // Simple validation: ensure the input is not empty.
                if (false)
                {
                    MessageBoxW(hWnd, L"Please enter a server name to join.", L"Validation Error", MB_OK | MB_ICONERROR);
                }
                else
                {

                    // Input appears valid, proceed to switch UI.
                    currentUI = UI_SECOND;

                    // Hide first UI controls.
                    ShowWindow(hCreateServerBtn, SW_HIDE);
                    ShowWindow(hJoinServerLabel, SW_HIDE);
                    ShowWindow(hJoinServerEdit, SW_HIDE);
                    ShowWindow(hJoinServerBtn, SW_HIDE);

                    // Show second UI control.
                    ShowWindow(hExitServerBtn, SW_SHOW);
                }
            }
        }
        // --- Second UI Button Handler ---
        else if (currentUI == UI_SECOND)
        {
            if (wmId == IDC_EXIT_SERVER)
            {
                hobbitMultiplayer.stopServer(); // Implement this method as needed
                hobbitMultiplayer.stopClient(); // Implement this method as needed

                // Wait for the server thread to finish
                if (serverThread.joinable())
                {
                    serverThread.join();
                }


                // Return to the first UI.
                currentUI = UI_FIRST;

                // Hide second UI control.
                ShowWindow(hExitServerBtn, SW_HIDE);

                // Reset first UI controls (if needed, you might also clear the edit box).
                ShowWindow(hCreateServerBtn, SW_SHOW);
                ShowWindow(hJoinServerLabel, SW_SHOW);
                ShowWindow(hJoinServerEdit, SW_SHOW);
                ShowWindow(hJoinServerBtn, SW_SHOW);
            }
        }
        // Handle standard menu commands.
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        }
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // Additional drawing logic can be added here if needed.
        EndPaint(hWnd, &ps);
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
