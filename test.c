#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include <curl/curl.h>

// --- 視窗 [自動連結與樣式] 解決連結錯誤
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "libcurl.lib") 
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// 完整保留原先的 Struct 與 資料庫
typedef struct { char nameTW[50]; char nameAPI[50]; } City;
typedef struct { char countryName[50]; City cities[10]; int cityCount; } Country;
typedef struct { char city[50]; double temperature; int humidity; char description[100]; } WeatherInfo;

struct string { char* ptr; size_t len; };

// 全域變數
HWND hStaticResult, hBtnBack;
HWND hOptionButtons[10]; // 用來動態顯示國家或城市的按鈕
int currentAppState = 0; // 0: 選國家, 1: 選城市
int selectedCountryIdx = -1;
const char* API_KEY = "2fa80dc7ec8be3fac297f88afd028de9"; //

// 4 個地區資料
Country worldData[] = {
    {"台灣", {{"台北", "Taipei"}, {"台中", "Taichung"}, {"高雄", "Kaohsiung"}, {"台南", "Tainan"}, {"花蓮", "Hualien"}}, 5},
    {"日本", {{"東京", "Tokyo"}, {"大阪", "Osaka"}, {"京都", "Kyoto"}, {"北海道", "Hokkaido"}, {"沖繩", "Okinawa"}}, 5},
    {"美國", {{"紐約", "New York"}, {"洛杉磯", "Los Angeles"}, {"舊金山", "San Francisco"}, {"芝加哥", "Chicago"}}, 4},
    {"歐洲地區", {{"倫敦", "London"}, {"巴黎", "Paris"}, {"柏林", "Berlin"}}, 3}
};
int countryCount = 4;

//  編碼轉換工具 (解決亂碼)
wchar_t* ToWide(const char* utf8) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    wchar_t* wstr = (wchar_t*)malloc(wlen * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, wlen);
    return wstr;
}

//  網路功能 (保留同學的邏輯)

size_t writefunc(void* ptr, size_t size, size_t nmemb, struct string* s) {
    size_t new_len = s->len + size * nmemb;
    s->ptr = realloc(s->ptr, new_len + 1);
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0'; s->len = new_len;
    return size * nmemb;
}

//視窗畫面
void FetchWeatherGUI(int cityIdx) {
    City target = worldData[selectedCountryIdx].cities[cityIdx];
    CURL* curl = curl_easy_init();
    if (curl) {
        struct string s = { malloc(1), 0 }; s.ptr[0] = '\0';
        char url[512];
        sprintf(url, "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric&lang=zh_tw", target.nameAPI, API_KEY);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

        if (curl_easy_perform(curl) == CURLE_OK) {
            cJSON* json = cJSON_Parse(s.ptr);
            if (json && cJSON_GetObjectItem(json, "main")) {
                double temp = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "main"), "temp")->valuedouble;
                int hum = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "main"), "humidity")->valueint;
                const char* desc = cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(json, "weather"), 0), "description")->valuestring;

                wchar_t* wCity = ToWide(target.nameTW);
                wchar_t* wDesc = ToWide(desc); // 解決氣候概況亂碼
                wchar_t buf[512];
                swprintf(buf, 512, L"【%s 觀測站】\n氣溫：%.1f °C  |  濕度：%d %%\n現況：%s", wCity, temp, hum, wDesc);
                SetWindowTextW(hStaticResult, buf);
                free(wCity); free(wDesc);
            }
            cJSON_Delete(json);
        }
        curl_easy_cleanup(curl); free(s.ptr);
    }
}

// GUI 介面更新
void UpdateInterface(HWND hwnd) {
    if (currentAppState == 0) { // 狀態 0：顯示國家選單
        SetWindowTextW(hStaticResult, L"=== 環球天氣觀測系統 ===\n請選擇國家/地區");
        ShowWindow(hBtnBack, SW_HIDE);
        for (int i = 0; i < 10; i++) {
            if (i < countryCount) {
                wchar_t* wName = ToWide(worldData[i].countryName);
                SetWindowTextW(hOptionButtons[i], wName);
                ShowWindow(hOptionButtons[i], SW_SHOW);
                free(wName);
            }
            else ShowWindow(hOptionButtons[i], SW_HIDE);
        }
    }
    else { // 狀態 1：顯示城市選單
        wchar_t* wCountry = ToWide(worldData[selectedCountryIdx].countryName);
        wchar_t buf[100]; swprintf(buf, 100, L"當前地區：[%s]\n請選擇觀測城市", wCountry);
        SetWindowTextW(hStaticResult, buf);
        ShowWindow(hBtnBack, SW_SHOW);
        free(wCountry);
        for (int i = 0; i < 10; i++) {
            if (i < worldData[selectedCountryIdx].cityCount) {
                wchar_t* wCity = ToWide(worldData[selectedCountryIdx].cities[i].nameTW);
                SetWindowTextW(hOptionButtons[i], wCity);
                ShowWindow(hOptionButtons[i], SW_SHOW);
                free(wCity);
            }
            else ShowWindow(hOptionButtons[i], SW_HIDE);
        }
    }
}


// 視窗訊息處理

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        HFONT hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft JhengHei");
        hStaticResult = CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 20, 440, 90, hwnd, NULL, NULL, NULL);
        SendMessage(hStaticResult, WM_SETFONT, (WPARAM)hFont, TRUE);

        for (int i = 0; i < 10; i++) {
            hOptionButtons[i] = CreateWindowW(L"BUTTON", L"", WS_CHILD | BS_PUSHBUTTON, 20 + (i % 2) * 225, 120 + (i / 2) * 45, 215, 38, hwnd, (HMENU)(1000 + i), NULL, NULL);
            SendMessage(hOptionButtons[i], WM_SETFONT, (WPARAM)hFont, TRUE);
        }
        hBtnBack = CreateWindowW(L"BUTTON", L"返回上一頁 (重選地區)", WS_CHILD, 20, 355, 440, 35, hwnd, (HMENU)2000, NULL, NULL);
        SendMessage(hBtnBack, WM_SETFONT, (WPARAM)hFont, TRUE);
        UpdateInterface(hwnd);
        break;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId >= 1000 && wmId < 1010) {
            int idx = wmId - 1000;
            if (currentAppState == 0) {
                selectedCountryIdx = idx; currentAppState = 1; UpdateInterface(hwnd);
            }
            else FetchWeatherGUI(idx);
        }
        else if (wmId == 2000) {
            currentAppState = 0; UpdateInterface(hwnd);
        }
        break;
    }
    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int main() {
    InitCommonControls();
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"GlobalWeatherWin";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, L"GlobalWeatherWin", L"衛星天氣觀測系統", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 495, 445, NULL, NULL, NULL, NULL);
    ShowWindow(hwnd, SW_SHOW);
    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}