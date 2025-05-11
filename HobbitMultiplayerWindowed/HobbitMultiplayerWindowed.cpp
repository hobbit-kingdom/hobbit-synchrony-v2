// HobbitMultiplayerWindowed.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "HobbitMultiplayerWindowed.h"
#include "../Hobbit Multiplayer/HobbitMultiplayer.h"
#include <shellapi.h>
#include <map>

#define MAX_LOADSTRING 100

// Control IDs for first UI
#define IDC_CREATE_SERVER            1001
#define IDC_JOIN_SERVER_LABEL        1002
#define IDC_JOIN_SERVER_EDIT         1003
#define IDC_JOIN_SERVER_BTN          1004

// Control ID for second UI
#define IDC_EXIT_SERVER              1005

// Checkbox IDs for sync toggles
#define IDC_CHECKBOX_PLAYER_SNAP     2001
#define IDC_CHECKBOX_ENEMY_HEALTH    2002
#define IDC_CHECKBOX_PLAYER_LEVEL    2003
#define IDC_CHECKBOX_INVENTORY       2004
#define IDC_CHECKBOX_BILBO_ZERO_DAMAGE      2005

// Global Variables
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

// UI Control Handles
HWND hCreateServerBtn = nullptr;
HWND hJoinServerLabel = nullptr;
HWND hJoinServerEdit = nullptr;
HWND hJoinServerBtn = nullptr;
HWND hExitServerBtn = nullptr;

// Sync Toggle Handles (Sidebar)
HWND hCheckPlayerSnap = nullptr;
HWND hCheckEnemyHealth = nullptr;
HWND hCheckPlayerLevel = nullptr;
HWND hCheckInventory = nullptr;
HWND hCheckBilbo0Damage = nullptr;

// UI State
enum UIState { UI_FIRST, UI_SECOND };
UIState currentUI = UI_FIRST;

// Multiplayer Manager
HobbitMultiplayer hobbitMultiplayer;
std::thread serverThread;

// Forward Declarations
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void UpdateSyncCheckboxes(HWND hWnd);
void ToggleSyncLabel(DataLabel label, bool enable);

// Entry Point
int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow
) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_HOBBITMULTIPLAYERWINDOWED, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	if (!InitInstance(hInstance, nCmdShow)) {
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_HOBBITMULTIPLAYERWINDOWED));
	MSG msg;

	while (GetMessage(&msg, nullptr, 0, 0)) {
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

// Window Class Registration
ATOM MyRegisterClass(HINSTANCE hInstance) {
	WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HOBBITMULTIPLAYERWINDOWED));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_HOBBITMULTIPLAYERWINDOWED);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

// Initialize Main Window
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
	hInst = hInstance;
	HWND hWnd = CreateWindowW(
		szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 600, 400, nullptr, nullptr, hInstance, nullptr
	);

	if (!hWnd) {
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	return TRUE;
}

// Update Sync Checkbox States
void UpdateSyncCheckboxes(HWND hWnd) {
	auto labelStates = hobbitMultiplayer.getMessageLabelStates();

	SendMessage(hCheckPlayerSnap, BM_SETCHECK,
		labelStates[DataLabel::CONNECTED_PLAYER_SNAP] ? BST_CHECKED : BST_UNCHECKED, 0);
	SendMessage(hCheckEnemyHealth, BM_SETCHECK,
		labelStates[DataLabel::ENEMIES_HEALTH] ? BST_CHECKED : BST_UNCHECKED, 0);
	SendMessage(hCheckPlayerLevel, BM_SETCHECK,
		labelStates[DataLabel::CONNECTED_PLAYER_LEVEL] ? BST_CHECKED : BST_UNCHECKED, 0);
	SendMessage(hCheckInventory, BM_SETCHECK,
		labelStates[DataLabel::INVENTORY] ? BST_CHECKED : BST_UNCHECKED, 0);
}

// Toggle Sync Label
void ToggleSyncLabel(DataLabel label, bool enable) {
	hobbitMultiplayer.setMessageLabelProcessing(label, enable);
}

// Main Window Procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_CREATE:
	{
		// Main UI Controls
		hCreateServerBtn = CreateWindowW(
			L"BUTTON", L"Create Server",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			50, 50, 150, 30, hWnd,
			(HMENU)IDC_CREATE_SERVER, hInst, nullptr
		);

		hJoinServerLabel = CreateWindowW(
			L"STATIC", L"Join Server:",
			WS_CHILD | WS_VISIBLE,
			50, 100, 100, 20, hWnd,
			(HMENU)IDC_JOIN_SERVER_LABEL, hInst, nullptr
		);

		hJoinServerEdit = CreateWindowW(
			L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_BORDER,
			160, 95, 200, 25, hWnd,
			(HMENU)IDC_JOIN_SERVER_EDIT, hInst, nullptr
		);

		hJoinServerBtn = CreateWindowW(
			L"BUTTON", L"Join Server",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			380, 95, 120, 30, hWnd,
			(HMENU)IDC_JOIN_SERVER_BTN, hInst, nullptr
		);

		hExitServerBtn = CreateWindowW(
			L"BUTTON", L"Exit Server",
			WS_CHILD | BS_PUSHBUTTON,
			50, 50, 150, 30, hWnd,
			(HMENU)IDC_EXIT_SERVER, hInst, nullptr
		);
		ShowWindow(hExitServerBtn, SW_HIDE);

		// Sync Toggle Checkboxes (Sidebar)
		hCheckPlayerSnap = CreateWindowW(
			L"BUTTON", L"Player Snap Sync",
			WS_CHILD | BS_AUTOCHECKBOX,
			320, 50, 200, 20, hWnd,
			(HMENU)IDC_CHECKBOX_PLAYER_SNAP, hInst, nullptr
		);
		ShowWindow(hCheckPlayerSnap, SW_HIDE);

		hCheckEnemyHealth = CreateWindowW(
			L"BUTTON", L"Enemy Health Sync",
			WS_CHILD | BS_AUTOCHECKBOX,
			320, 80, 200, 20, hWnd,
			(HMENU)IDC_CHECKBOX_ENEMY_HEALTH, hInst, nullptr
		);
		ShowWindow(hCheckEnemyHealth, SW_HIDE);

		hCheckPlayerLevel = CreateWindowW(
			L"BUTTON", L"Player Level Sync",
			WS_CHILD | BS_AUTOCHECKBOX,
			320, 110, 200, 20, hWnd,
			(HMENU)IDC_CHECKBOX_PLAYER_LEVEL, hInst, nullptr
		);
		ShowWindow(hCheckPlayerLevel, SW_HIDE);

		hCheckInventory = CreateWindowW(
			L"BUTTON", L"Inventory Sync",
			WS_CHILD | BS_AUTOCHECKBOX,
			320, 140, 200, 20, hWnd,
			(HMENU)IDC_CHECKBOX_INVENTORY, hInst, nullptr
		);
		ShowWindow(hCheckInventory, SW_HIDE);

		hCheckBilbo0Damage = CreateWindowW(
			L"BUTTON", L"Inventory Sync",
			WS_CHILD | BS_AUTOCHECKBOX,
			320, 140, 200, 20, hWnd,
			(HMENU)IDC_CHECKBOX_INVENTORY, hInst, nullptr
		);
		ShowWindow(hCheckBilbo0Damage, SW_HIDE);
		break;
	}

	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		int wmEvent = HIWORD(wParam);

		// Handle Checkbox Toggles
		if (wmEvent == BN_CLICKED) {
			switch (wmId) {
			case IDC_CHECKBOX_PLAYER_SNAP:
				ToggleSyncLabel(DataLabel::CONNECTED_PLAYER_SNAP,
					SendMessage(hCheckPlayerSnap, BM_GETCHECK, 0, 0) == BST_CHECKED);
				break;
			case IDC_CHECKBOX_ENEMY_HEALTH:
				ToggleSyncLabel(DataLabel::ENEMIES_HEALTH,
					SendMessage(hCheckEnemyHealth, BM_GETCHECK, 0, 0) == BST_CHECKED);
				break;
			case IDC_CHECKBOX_PLAYER_LEVEL:
				ToggleSyncLabel(DataLabel::CONNECTED_PLAYER_LEVEL,
					SendMessage(hCheckPlayerLevel, BM_GETCHECK, 0, 0) == BST_CHECKED);
				break;
			case IDC_CHECKBOX_INVENTORY:
				ToggleSyncLabel(DataLabel::INVENTORY,
					SendMessage(hCheckInventory, BM_GETCHECK, 0, 0) == BST_CHECKED);
			case IDC_CHECKBOX_BILBO_ZERO_DAMAGE:
				ToggleSyncLabel(DataLabel::INVENTORY,
					SendMessage(hCheckInventory, BM_GETCHECK, 0, 0) == BST_CHECKED);
				break;
			}
		}

		// Main UI Logic
		if (currentUI == UI_FIRST) {
			if (wmId == IDC_CREATE_SERVER) {
				serverThread = std::thread([&]() { hobbitMultiplayer.startServerClient(); });
				currentUI = UI_SECOND;
				ShowWindow(hCreateServerBtn, SW_HIDE);
				ShowWindow(hJoinServerLabel, SW_HIDE);
				ShowWindow(hJoinServerEdit, SW_HIDE);
				ShowWindow(hJoinServerBtn, SW_HIDE);
				ShowWindow(hExitServerBtn, SW_SHOW);
				ShowWindow(hCheckPlayerSnap, SW_SHOW);
				ShowWindow(hCheckEnemyHealth, SW_SHOW);
				ShowWindow(hCheckPlayerLevel, SW_SHOW);
				ShowWindow(hCheckInventory, SW_SHOW);
				ShowWindow(hCheckBilbo0Damage, SW_SHOW);
				UpdateSyncCheckboxes(hWnd);
			}
			else if (wmId == IDC_JOIN_SERVER_BTN) {
				wchar_t buffer[256] = { 0 };
				GetWindowTextW(hJoinServerEdit, buffer, 256);
				std::string serverAddress(buffer, buffer + wcslen(buffer));

				serverThread = std::thread([&]() { hobbitMultiplayer.startClient(serverAddress); });
				currentUI = UI_SECOND;
				ShowWindow(hCreateServerBtn, SW_HIDE);
				ShowWindow(hJoinServerLabel, SW_HIDE);
				ShowWindow(hJoinServerEdit, SW_HIDE);
				ShowWindow(hJoinServerBtn, SW_HIDE);
				ShowWindow(hExitServerBtn, SW_SHOW);
				ShowWindow(hCheckPlayerSnap, SW_SHOW);
				ShowWindow(hCheckEnemyHealth, SW_SHOW);
				ShowWindow(hCheckPlayerLevel, SW_SHOW);
				ShowWindow(hCheckInventory, SW_SHOW);
				ShowWindow(hCheckBilbo0Damage, SW_SHOW);
				UpdateSyncCheckboxes(hWnd);
			}
		}
		else if (currentUI == UI_SECOND) {
			if (wmId == IDC_EXIT_SERVER) {
				hobbitMultiplayer.stopServer();
				hobbitMultiplayer.stopClient();
				if (serverThread.joinable()) serverThread.join();
				currentUI = UI_FIRST;
				ShowWindow(hExitServerBtn, SW_HIDE);
				ShowWindow(hCreateServerBtn, SW_SHOW);
				ShowWindow(hJoinServerLabel, SW_SHOW);
				ShowWindow(hJoinServerEdit, SW_SHOW);
				ShowWindow(hJoinServerBtn, SW_SHOW);
				ShowWindow(hCheckPlayerSnap, SW_HIDE);
				ShowWindow(hCheckEnemyHealth, SW_HIDE);
				ShowWindow(hCheckPlayerLevel, SW_HIDE);
				ShowWindow(hCheckInventory, SW_HIDE);
			}
		}

		// Menu Commands
		switch (wmId) {
		case IDM_ABOUT:
			ShellExecuteW(nullptr, L"open", L"https://www.youtube.com", nullptr, nullptr, SW_SHOWNORMAL);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		}
		break;
	}

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
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