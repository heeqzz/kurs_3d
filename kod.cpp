#include "WindowsProject1.h"
#include <windows.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include <iostream>

using namespace std;

#define M_PI 3.14159265358979323846
#define MAX_LOADSTRING 100

struct Vector3 {
    float x, y, z;
};

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING] = L"3D Viewer - Пропеллер самолета";
WCHAR szWindowClass[MAX_LOADSTRING] = L"My3DViewer";

vector<Vector3> vertices;
vector<vector<int>> faces;

// Массив групп пропеллеров (по одной группе на каждый пропеллер)
vector<vector<int>> propellerGroups;

// Центры пропеллеров
vector<Vector3> propellerCenters;

float ha = 0.0f; // Горизонтальный угол
float va = 0.0f; // Вертикальный угол
float r = 200.0f; // Расстояние до наблюдателя
float scale = 1.0f; // Масштаб модели
int windowWidth = 800, windowHeight = 600;
float propRotation = 0.0f; // Угол вращения пропеллера
bool isPropellerSpinning = false; // Флаг: крутится ли пропеллер

// === Объявления функций ===
bool LoadOBJModel(const string& path);
void ProjectVertex(const Vector3& v, POINT& out, int width, int height);
Vector3 rotateZ(const Vector3& v, float angleDeg);
Vector3 WorldToView(const Vector3& world, float ha, float va, float r);
void printModel(HDC hdc, const vector<Vector3>& vertices, const vector<vector<int>>& faces, int width, int height);

// WinAPI функции
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// === Main ===
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_WINDOWSPROJECT1, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow)) return FALSE;

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSPROJECT1));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

// === Регистрация окна ===
ATOM MyRegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT1));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCE(IDC_WINDOWSPROJECT1);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

// === Создание окна ===
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return FALSE;

    if (!LoadOBJModel("a1.obj")) {
        MessageBox(hWnd, L"Не удалось загрузить модель!", L"Ошибка", MB_OK | MB_ICONERROR);
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    SetTimer(hWnd, 1, 30, NULL); // Анимация каждые 30 мс
    return TRUE;
}

bool LoadOBJModel(const string& path) {
    ifstream file(path);
    if (!file.is_open()) return false;

    string line;
    string currentGroup = "";
    bool inPropellerGroup = false;
    vector<int> currentPropellerGroup;

    while (getline(file, line)) {
        istringstream iss(line);
        string type;
        iss >> type;

        if (type == "v") {
            Vector3 v;
            iss >> v.x >> v.y >> v.z;
            vertices.push_back(v);
        }
        else if (type == "f") {
            vector<int> face;
            string token;
            while (iss >> token) {
                istringstream tokenStream(token);
                int index;
                char slash;
                tokenStream >> index;
                face.push_back(index - 1); // Индексы начинаются с 1
            }
            faces.push_back(face);
        }
        else if (type == "o") { // Объект / группа
            iss >> currentGroup;
            if (currentGroup == "A_AirPlane1.001" || currentGroup == "A_AirPlane1.002") {
                if (inPropellerGroup && !currentPropellerGroup.empty()) {
                    propellerGroups.push_back(currentPropellerGroup);
                    currentPropellerGroup.clear();
                }
                inPropellerGroup = true;
            }
            else {
                if (inPropellerGroup && !currentPropellerGroup.empty()) {
                    propellerGroups.push_back(currentPropellerGroup);
                    currentPropellerGroup.clear();
                }
                inPropellerGroup = false;
            }
        }

        if (inPropellerGroup && type == "v") {
            currentPropellerGroup.push_back(vertices.size() - 1);
        }
    }

    // Если остались незавершённые группы
    if (!currentPropellerGroup.empty()) {
        propellerGroups.push_back(currentPropellerGroup);
    }

    // Центрирование всей модели
    if (!vertices.empty()) {
        Vector3 center = { 0 };
        for (const auto& v : vertices) {
            center.x += v.x;
            center.y += v.y;
            center.z += v.z;
        }
        center.x /= vertices.size();
        center.y /= vertices.size();
        center.z /= vertices.size();

        for (auto& v : vertices) {
            v.x -= center.x;
            v.y -= center.y;
            v.z -= center.z;
        }
    }

    // Вычисление центра для каждого пропеллера
    for (const auto& group : propellerGroups) {
        Vector3 center = { 0 };
        for (int idx : group) {
            center.x += vertices[idx].x;
            center.y += vertices[idx].y;
            center.z += vertices[idx].z;
        }
        float count = static_cast<float>(group.size());
        center.x /= count;
        center.y /= count;
        center.z /= count;
        propellerCenters.push_back(center);
    }

    return true;
}

Vector3 rotateZ(const Vector3& v, float angleDeg) {
    float rad = angleDeg * M_PI / 180.0f;
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    return {
        v.x * cosA - v.y * sinA,
        v.x * sinA + v.y * cosA,
        v.z
    };
}

Vector3 WorldToView(const Vector3& world, float ha, float va, float r) {
    Vector3 rotated = rotateZ(world, va); // Вверх/вниз
    rotated = Vector3{
        rotated.x * cosf(ha * M_PI / 180.0f) + rotated.z * sinf(ha * M_PI / 180.0f),
        rotated.y,
        -rotated.x * sinf(ha * M_PI / 180.0f) + rotated.z * cosf(ha * M_PI / 180.0f)
    };
    rotated.z -= r;
    return rotated;
}

void ProjectVertex(const Vector3& v, POINT& out, int width, int height) {
    float depth = 500.0f;
    float invZ = depth / (v.z + depth);
    out.x = static_cast<LONG>((v.x * invZ) * scale + width / 2.0f);
    out.y = static_cast<LONG>((-v.y * invZ) * scale + height / 2.0f);
}

void printModel(HDC hdc, const vector<Vector3>& vertices, const vector<vector<int>>& faces, int width, int height) {
    HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 255));
    SelectObject(hdc, hBrush);

    for (const auto& face : faces) {
        if (face.empty()) continue;

        POINT pStart;
        Vector3 viewStart = WorldToView(vertices[face[0]], ha, va, r);
        ProjectVertex(viewStart, pStart, width, height);
        MoveToEx(hdc, pStart.x, pStart.y, nullptr);

        for (size_t i = 1; i < face.size(); ++i) {
            Vector3 world = vertices[face[i]];
            bool isPropeller = false;
            size_t propIndex = 0;

            // Проверяем принадлежность вершины к любому из пропеллеров
            for (size_t j = 0; j < propellerGroups.size(); ++j) {
                if (find(propellerGroups[j].begin(), propellerGroups[j].end(), face[i]) != propellerGroups[j].end()) {
                    isPropeller = true;
                    propIndex = j;
                    break;
                }
            }

            if (isPropeller && isPropellerSpinning) {
                Vector3 local = {
                    world.x - propellerCenters[propIndex].x,
                    world.y - propellerCenters[propIndex].y,
                    world.z - propellerCenters[propIndex].z
                };
                local = rotateZ(local, propRotation);
                world = {
                    local.x + propellerCenters[propIndex].x,
                    local.y + propellerCenters[propIndex].y,
                    local.z + propellerCenters[propIndex].z
                };
            }

            Vector3 view = WorldToView(world, ha, va, r);
            POINT p;
            ProjectVertex(view, p, width, height);
            LineTo(hdc, p.x, p.y);
        }

        LineTo(hdc, pStart.x, pStart.y); // замкнуть полигон
    }

    DeleteObject(hBrush);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        break;

    case WM_TIMER:
        if (isPropellerSpinning) {
            propRotation += 10.0f;
            if (propRotation >= 360.0f) propRotation -= 360.0f;
        }
        InvalidateRect(hWnd, NULL, FALSE);
        break;

    case WM_KEYDOWN:
        switch (wParam) {
        case VK_LEFT: ha -= 5.0f; break;
        case VK_RIGHT: ha += 5.0f; break;
        case VK_UP: va += 5.0f; break;
        case VK_DOWN: va -= 5.0f; break;
        case VK_ADD: scale *= 1.1f; break;
        case VK_SUBTRACT: scale /= 1.1f; break;
        case VK_SPACE:
            isPropellerSpinning = !isPropellerSpinning;
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        break;

    case WM_SIZE:
        windowWidth = LOWORD(lParam);
        windowHeight = HIWORD(lParam);
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect;
        GetClientRect(hWnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, width, height);
        HGDIOBJ oldBmp = SelectObject(hdcMem, hbmMem);

        FillRect(hdcMem, &rect, (HBRUSH)(COLOR_WINDOW + 1));
        printModel(hdcMem, vertices, faces, width, height);
        BitBlt(hdc, 0, 0, width, height, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, oldBmp);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);

        EndPaint(hWnd, &ps);
    } break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
