#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h> 
#include "cJSON.h" 
#include <curl/curl.h>

// 啟用 Windows 現代化外觀 (Visual Styles)
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// --- 定義資料結構 ---
typedef struct {
    char nameTW[50];   // 顯示名稱
    char nameAPI[50];  // 查詢名稱
} City;

typedef struct {
    char countryName[50];
    City cities[30];
    int cityCount;
} Country;

// --- 天氣資訊結構 ---
typedef struct {
    char city[50];
    double temperature;
    int humidity;
    char description[100];
} WeatherInfo;

struct string {
    char* ptr;
    size_t len;
};

// --- 基礎工具函式 ---
void init_string(struct string* s) {
    s->len = 0;
    s->ptr = malloc(s->len + 1);
    if (s->ptr == NULL) exit(EXIT_FAILURE);
    s->ptr[0] = '\0';
}

size_t writefunc(void* ptr, size_t size, size_t nmemb, struct string* s) {
    size_t new_len = s->len + size * nmemb;
    s->ptr = realloc(s->ptr, new_len + 1);
    if (s->ptr == NULL) exit(EXIT_FAILURE);
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;
    return size * nmemb;
}

// UTF-8 轉 ANSI (Big5) 轉換函式
void UTF8ToANSI(char* src, char* dest, int destSize) {
    wchar_t wStr[512];
    MultiByteToWideChar(CP_UTF8, 0, src, -1, wStr, 512);
    WideCharToMultiByte(CP_ACP, 0, wStr, -1, dest, destSize, NULL, NULL);
}

// API Key
const char* API_KEY = "2fa80dc7ec8be3fac297f88afd028de9";

int getWeather(char* searchName, char* displayName, WeatherInfo* info) {
    CURL* curl;
    CURLcode res;
    struct string s;
    init_string(&s);

    curl = curl_easy_init();
    if (curl) {
        char* encodedName = curl_easy_escape(curl, searchName, 0);

        char url[512];
        sprintf(url, "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric&lang=zh_tw", encodedName, API_KEY);

        curl_free(encodedName);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            printf("連線失敗 (Error Code: %d)\n", res);
            printf("原因: %s\n", curl_easy_strerror(res));
            return 0;
        }

        cJSON* json = cJSON_Parse(s.ptr);
        if (json == NULL) return 0;

        cJSON* cod = cJSON_GetObjectItem(json, "cod");
        int statusCode = 0;
        if (cJSON_IsNumber(cod)) statusCode = cod->valueint;
        else if (cJSON_IsString(cod)) statusCode = atoi(cod->valuestring);

        if (statusCode != 200) {
            printf(">> 查詢失敗 (Code: %d) - 請檢查 API Key。\n", statusCode);
            return 0;
        }

        cJSON* main_obj = cJSON_GetObjectItem(json, "main");
        cJSON* temp = cJSON_GetObjectItem(main_obj, "temp");
        cJSON* humidity = cJSON_GetObjectItem(main_obj, "humidity");
        cJSON* weather_arr = cJSON_GetObjectItem(json, "weather");
        cJSON* weather_item = cJSON_GetArrayItem(weather_arr, 0);
        cJSON* desc = cJSON_GetObjectItem(weather_item, "description");

        strcpy(info->city, displayName);
        info->temperature = temp->valuedouble;
        info->humidity = humidity->valueint;

        // 轉換編碼以正常顯示中文
        UTF8ToANSI(desc->valuestring, info->description, sizeof(info->description));

        cJSON_Delete(json);
        curl_easy_cleanup(curl);
        free(s.ptr);
        return 1;
    }
    return 0;
}

void saveHistory(WeatherInfo info) {
    FILE* fp = fopen("history.txt", "a");
    if (fp != NULL) {
        fprintf(fp, "%s | %.1f°C | %s\n", info.city, info.temperature, info.description);
        fclose(fp);
    }
}


//      GUI 介面實作

Country worldData[] = {
    // 1. 台灣 (Taiwan)
    {
        "台灣 (Taiwan)",
        {
            {"基隆市", "Keelung"},
            {"台北市", "Taipei"},
            {"新北市", "New Taipei"},
            {"桃園市", "Taoyuan"},
            {"新竹市", "Hsinchu"},
            {"新竹縣", "Zhubei"},
            {"苗栗縣", "Miaoli"},
            {"台中市", "Taichung"},
            {"彰化縣", "Changhua"},
            {"南投縣", "Nantou"},
            {"雲林縣", "Douliu"},
            {"嘉義市", "Chiayi"},
            {"嘉義縣", "Taibao"},
            {"台南市", "Tainan"},
            {"高雄市", "Kaohsiung"},
            {"屏東縣", "Pingtung"},
            {"宜蘭縣", "Yilan"},
            {"花蓮縣", "Hualien"},
            {"台東縣", "Taitung"},
            {"澎湖縣", "Magong"},
            {"金門縣", "Jincheng"},
            {"連江縣 (馬祖)", "Nangan"}
        },
        22
    },
    // 2. 日本 (Japan)
    {
        "日本 (Japan)",
        {
            {"東京", "Tokyo"},
            {"大阪", "Osaka"},
            {"京都", "Kyoto"},
            {"北海道 (札幌)", "Sapporo"},
            {"沖繩 (那霸)", "Naha"},
            {"福岡", "Fukuoka"},
            {"名古屋", "Nagoya"}
        },
        7
    },
    // 3. 美國 (USA)
    {
        "美國 (USA)",
        {
            {"紐約", "New York"},
            {"洛杉磯", "Los Angeles"},
            {"舊金山", "San Francisco"},
            {"西雅圖", "Seattle"},
            {"芝加哥", "Chicago"},
            {"波士頓", "Boston"}
        },
        6
    },
    // 4. 歐洲地區 (Europe)
    {
        "歐洲 (Europe)",
        {
            {"倫敦 (英國)", "London"},
            {"巴黎 (法國)", "Paris"},
            {"柏林 (德國)", "Berlin"},
            {"羅馬 (義大利)", "Rome"},
            {"馬德里 (西班牙)", "Madrid"},
            {"阿姆斯特丹 (荷蘭)", "Amsterdam"}
        },
        6
    }
};

int countryCount = sizeof(worldData) / sizeof(worldData[0]);

HWND hLabel;
HWND hButtons[50];
int currentCountryIdx = -1;

void ClearButtons() {
    for (int i = 0; i < 50; i++) {
        if (hButtons[i]) {
            DestroyWindow(hButtons[i]);
            hButtons[i] = NULL;
        }
    }
}

void ShowCountryMenu(HWND hwnd) {
    ClearButtons();
    currentCountryIdx = -1;
    SetWindowTextA(hLabel, "衛星天氣觀測系統 - 請選擇國家");

    for (int i = 0; i < countryCount; i++) {
        hButtons[i] = CreateWindowA("BUTTON", worldData[i].countryName,
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            50, 60 + (i * 50), 400, 40, hwnd, (HMENU)(100 + i), (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE), NULL);
    }
}

void ShowCityMenu(HWND hwnd, int countryIdx) {
    ClearButtons();
    currentCountryIdx = countryIdx;

    char title[100];
    sprintf(title, "當前地區: [%s] - 請選擇觀測城市", worldData[countryIdx].countryName);
    SetWindowTextA(hLabel, title);

    int cols = 2;
    int x, y;
    for (int i = 0; i < worldData[countryIdx].cityCount; i++) {
        x = 50 + (i % cols) * 210;
        y = 60 + (i / cols) * 50;

        hButtons[i] = CreateWindowA("BUTTON", worldData[countryIdx].cities[i].nameTW,
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            x, y, 200, 40, hwnd, (HMENU)(200 + i), (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE), NULL);
    }

    // 返回按鈕下移至 630，避免遮擋
    hButtons[49] = CreateWindowA("BUTTON", "返回上一頁 (重選地區)",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        50, 630, 410, 45, hwnd, (HMENU)999, (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE), NULL);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        hLabel = CreateWindowA("STATIC", "初始化中...", WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, 20, 500, 30, hwnd, NULL, NULL, NULL);

        // 設定字體
        HFONT hFont = CreateFont(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Microsoft JhengHei");
        SendMessage(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        ShowCountryMenu(hwnd);
        break;

    case WM_COMMAND:
        int id = LOWORD(wParam);

        if (id == 999) {
            ShowCountryMenu(hwnd);
        }
        else if (id >= 100 && id < 100 + countryCount) {
            ShowCityMenu(hwnd, id - 100);
        }
        else if (id >= 200 && currentCountryIdx != -1) {
            int cityIdx = id - 200;
            City c = worldData[currentCountryIdx].cities[cityIdx];
            WeatherInfo info;

            char buffer[200];
            sprintf(buffer, "正在連線查詢 %s ...", c.nameTW);
            SetWindowTextA(hLabel, buffer);
            UpdateWindow(hLabel);

            if (getWeather(c.nameAPI, c.nameTW, &info)) {
                saveHistory(info);
                sprintf(buffer, "地區: %s\n氣溫: %.1f °C\n濕度: %d %%\n現況: %s",
                    info.city, info.temperature, info.humidity, info.description);

                MessageBoxA(hwnd, buffer, "天氣觀測結果", MB_OK | MB_ICONINFORMATION);
                SetWindowTextA(hLabel, "查詢完成。請選擇下一個城市");
            }
            else {
                MessageBoxA(hwnd, "查詢失敗，請檢查 API Key 或網路連線。", "錯誤", MB_OK | MB_ICONERROR);
                SetWindowTextA(hLabel, "查詢失敗");
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int main() {
    printf("=== 天氣觀測系統 (Global Weather System) ===\n");
    printf("系統初始化中... 請在 GUI 視窗操作。\n");

    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    // 設定背景為白色
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = "WeatherAppClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassA(&wc)) {
        printf("視窗註冊失敗！\n");
        system("pause");
        return -1;
    }

    // 設定視窗高度為 750
    HWND hwnd = CreateWindowA("WeatherAppClass", "衛星天氣觀測系統",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 530, 750,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    printf("系統已關閉。\n");
    return 0;
}