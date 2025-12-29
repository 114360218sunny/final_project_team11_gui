#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h> 
#include "cJSON.h" 
#include <curl/curl.h>

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// --- 全域變數 ---
int langMode = 0;
const char* API_KEY = "2fa80dc7ec8be3fac297f88afd028de9";
HWND hLabel, hButtons[50], hLangBtns[3], hTimeLabel, hGroupInfo;
int currentCountryIdx = -1;

// --- 語系映射表 ---
const char* GetUIText(int id) {
    const char* texts[2][18] = {
        {"衛星天氣觀測系統", "數據同步中...", "現在氣溫", "相對濕度", "天氣狀況", "空氣品質", "明日氣象預報趨勢", "關閉報告並返回選單", "觀測時間", "<- 返回上一層", "請選擇區域", "良好", "警戒", "狀態", "預測溫度", "預測濕度", "當天外出建議", "退出系統"},
        {"Weather System", "Syncing...", "Temperature", "Humidity", "Condition", "Air Quality", "Tomorrow's Forecast", "Close and Return", "Observed at", "<- Back", "Select Region", "Good", "Warning", "Status", "Forecast Temp", "Forecast Humid", "Outdoor Suggestion", "Exit"}
    };
    return texts[langMode][id];
}

// --- 資料結構 ---
typedef struct { char nameTW[50]; char nameEN[50]; } City;
typedef struct { char countryTW[50]; char countryEN[50]; City cities[30]; int cityCount; } Country;
typedef struct { char city[50]; double temperature; int humidity; char description[100]; char tomorrow_brief[512]; int aqi; char suggestion[256]; } WeatherInfo;

// --- 網路與文字輔助 ---
struct string { char* ptr; size_t len; };
void init_string(struct string* s) { s->len = 0; s->ptr = malloc(1); s->ptr[0] = '\0'; }
size_t writefunc(void* ptr, size_t size, size_t nmemb, struct string* s) {
    size_t new_len = s->len + size * nmemb;
    s->ptr = realloc(s->ptr, new_len + 1);
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0'; s->len = new_len;
    return size * nmemb;
}
void UTF8ToANSI(char* src, char* dest, int destSize) {
    wchar_t wStr[512];
    MultiByteToWideChar(CP_UTF8, 0, src, -1, wStr, 512);
    WideCharToMultiByte(CP_ACP, 0, wStr, -1, dest, destSize, NULL, NULL);
}

// --- API 邏輯 ---
int getAQI(double lat, double lon) {
    CURL* curl; struct string s; init_string(&s); curl = curl_easy_init();
    char url[512]; sprintf(url, "http://api.openweathermap.org/data/2.5/air_pollution?lat=%f&lon=%f&appid=%s", lat, lon, API_KEY);
    curl_easy_setopt(curl, CURLOPT_URL, url); curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc); curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
    int aqi = -1; if (curl_easy_perform(curl) == CURLE_OK) {
        cJSON* json = cJSON_Parse(s.ptr); if (json) {
            cJSON* list = cJSON_GetObjectItem(json, "list"); cJSON* first = cJSON_GetArrayItem(list, 0);
            aqi = cJSON_GetObjectItem(cJSON_GetObjectItem(first, "main"), "aqi")->valueint; cJSON_Delete(json);
        }
    }
    curl_easy_cleanup(curl); free(s.ptr); return aqi;
}

int getWeather(char* searchName, char* displayName, WeatherInfo* info) {
    CURL* curl; struct string s; init_string(&s); curl = curl_easy_init();
    if (!curl) return 0;
    const char* langCodes[] = { "zh_tw", "en" };
    char url[512]; sprintf(url, "http://api.openweathermap.org/data/2.5/forecast?q=%s&appid=%s&units=metric&lang=%s", searchName, API_KEY, langCodes[langMode]);
    curl_easy_setopt(curl, CURLOPT_URL, url); curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc); curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
    if (curl_easy_perform(curl) != CURLE_OK) { free(s.ptr); return 0; }
    cJSON* json = cJSON_Parse(s.ptr); if (!json) { free(s.ptr); return 0; }
    cJSON* list = cJSON_GetObjectItem(json, "list"); cJSON* cur = cJSON_GetArrayItem(list, 0);
    cJSON* city_obj = cJSON_GetObjectItem(json, "city"); cJSON* coord = cJSON_GetObjectItem(city_obj, "coord");
    double lat = cJSON_GetObjectItem(coord, "lat")->valuedouble; double lon = cJSON_GetObjectItem(coord, "lon")->valuedouble;

    info->temperature = cJSON_GetObjectItem(cJSON_GetObjectItem(cur, "main"), "temp")->valuedouble;
    info->humidity = cJSON_GetObjectItem(cJSON_GetObjectItem(cur, "main"), "humidity")->valueint;
    strcpy(info->city, displayName);
    UTF8ToANSI(cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(cur, "weather"), 0), "description")->valuestring, info->description, 100);

    double t_max = -99, t_min = 99; char tomorrow_desc[100] = ""; int count = 0, h_sum = 0;
    for (int i = 8; i < 16; i++) {
        cJSON* item = cJSON_GetArrayItem(list, i); if (!item) break;
        double t = cJSON_GetObjectItem(cJSON_GetObjectItem(item, "main"), "temp")->valuedouble;
        int h = cJSON_GetObjectItem(cJSON_GetObjectItem(item, "main"), "humidity")->valueint;
        if (t > t_max) t_max = t; if (t < t_min) t_min = t;
        h_sum += h; count++;
        if (i == 12) UTF8ToANSI(cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(item, "weather"), 0), "description")->valuestring, tomorrow_desc, 100);
    }
    info->aqi = getAQI(lat, lon);
    sprintf(info->tomorrow_brief, "  - %s: %.1f ~ %.1f C\r\n  - %s: %d %%\r\n  - %s: %s\r\n  - %s: AQI %d [%s]",
        GetUIText(14), t_min, t_max, GetUIText(15), (count ? h_sum / count : 0), GetUIText(4), tomorrow_desc, GetUIText(5), info->aqi, info->aqi <= 2 ? GetUIText(11) : GetUIText(12));

    if (info->aqi <= 2) strcpy(info->suggestion, langMode == 0 ? "空氣品質良好，非常適合戶外活動。" : "Air quality is good. Great for outdoors.");
    else strcpy(info->suggestion, langMode == 0 ? "空氣品質警戒，建議減少外出並佩戴口罩。" : "Air quality warning. Please wear a mask.");

    cJSON_Delete(json); curl_easy_cleanup(curl); free(s.ptr); return 1;
}

// --- 完整的數據庫 ---
Country worldData[] = {
    {"台灣 (Taiwan)", "Taiwan", {
        {"基隆市", "Keelung"},{"台北市", "Taipei"},{"新北市", "New Taipei"},{"桃園市", "Taoyuan"},
        {"新竹市", "Hsinchu"},{"新竹縣", "Zhubei"},{"苗栗縣", "Miaoli"},{"台中市", "Taichung"},
        {"彰化縣", "Changhua"},{"南投縣", "Nantou"},{"雲林縣", "Douliu"},{"嘉義市", "Chiayi"},
        {"嘉義縣", "Taibao"},{"台南市", "Tainan"},{"高雄市", "Kaohsiung"},{"屏東縣", "Pingtung"},
        {"宜蘭縣", "Yilan"},{"花蓮縣", "Hualien"},{"台東縣", "Taitung"},{"澎湖縣", "Magong"},
        {"金門縣", "Jincheng"},{"連江縣 (馬祖)", "Nangan"}
    }, 22},
    {"日本 (Japan)", "Japan", {
        {"東京", "Tokyo"},{"大阪", "Osaka"},{"京都", "Kyoto"},{"北海道 (札幌)", "Sapporo"},
        {"沖繩 (那霸)", "Naha"},{"福岡", "Fukuoka"},{"名古屋", "Nagoya"}
    }, 7},
    {"美國 (USA)", "USA", {
        {"紐約", "New York"},{"洛杉磯", "Los Angeles"},{"舊金山", "San Francisco"},
        {"西雅圖", "Seattle"},{"芝加哥", "Chicago"},{"波士頓", "Boston"}
    }, 6},
    {"歐洲 (Europe)", "Europe", {
        {"倫敦 (英國)", "London"},{"巴黎 (法國)", "Paris"},{"柏林 (德國)", "Berlin"},
        {"羅馬 (義大利)", "Rome"},{"馬德里 (西班牙)", "Madrid"},{"阿姆斯特丹 (荷蘭)", "Amsterdam"}
    }, 6}
};

void ClearButtons() { for (int i = 0; i < 50; i++) { if (hButtons[i]) { DestroyWindow(hButtons[i]); hButtons[i] = NULL; } } }

void ShowCountryMenu(HWND hwnd) {
    ClearButtons(); currentCountryIdx = -1;
    SetWindowTextA(hLabel, GetUIText(10));
    for (int i = 0; i < 4; i++) {
        const char* name = (langMode == 0) ? worldData[i].countryTW : worldData[i].countryEN;
        hButtons[i] = CreateWindowA("BUTTON", name, WS_TABSTOP | WS_VISIBLE | WS_CHILD, 100, 80 + (i * 70), 600, 50, hwnd, (HMENU)(100 + i), NULL, NULL);
    }
    ShowWindow(hGroupInfo, SW_SHOW);
}

void ShowCityMenu(HWND hwnd, int countryIdx) {
    ClearButtons(); currentCountryIdx = countryIdx;
    ShowWindow(hGroupInfo, SW_HIDE);
    for (int i = 0; i < worldData[countryIdx].cityCount; i++) {
        const char* name = (langMode == 0) ? worldData[countryIdx].cities[i].nameTW : worldData[countryIdx].cities[i].nameEN;
        hButtons[i] = CreateWindowA("BUTTON", name, WS_TABSTOP | WS_VISIBLE | WS_CHILD, 50 + (i % 3) * 235, 80 + (i / 3) * 65, 220, 50, hwnd, (HMENU)(200 + i), NULL, NULL);
    }
    hButtons[49] = CreateWindowA("BUTTON", GetUIText(9), WS_TABSTOP | WS_VISIBLE | WS_CHILD, 100, 750, 600, 55, hwnd, (HMENU)999, NULL, NULL);
}

// --- 報告視窗邏輯 ---
LRESULT CALLBACK ReportWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COMMAND && LOWORD(wParam) == 888) { DestroyWindow(hwnd); return 0; }
    if (msg == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void ShowWeatherReport(HWND owner, WeatherInfo info) {
    char report[2048]; char timeStr[100]; time_t now = time(NULL); strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
    sprintf(report, "【 %s - %s 】\r\n\r\n 現在狀態:\r\n ------------------------------------------\r\n  > %s: %.1f C\r\n  > %s: %d %%\r\n  > %s: %s\r\n  > %s: AQI %d\r\n\r\n [ %s ]\r\n ------------------------------------------\r\n%s\r\n\r\n [ %s ]\r\n ------------------------------------------\r\n  %s\r\n\r\n %s: %s",
        info.city, (langMode == 0 ? "衛星觀測" : "Satellite Obv"), GetUIText(2), info.temperature, GetUIText(3), info.humidity, GetUIText(4), info.description, GetUIText(5), info.aqi, GetUIText(6), info.tomorrow_brief, GetUIText(16), info.suggestion, GetUIText(8), timeStr);

    WNDCLASSA rwc = { 0 }; rwc.lpfnWndProc = ReportWndProc; rwc.hInstance = GetModuleHandle(NULL); rwc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH); rwc.lpszClassName = "FinalReportWinV12"; RegisterClassA(&rwc);
    HWND hPop = CreateWindowA("FinalReportWinV12", GetUIText(0), WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 920, 850, owner, NULL, NULL, NULL);

    char weatherStatus[50], aqiStatus[50], asciiWeather[512], asciiAQI[512];
    sprintf(weatherStatus, "--- %s ---", info.description);
    sprintf(aqiStatus, "%s: %s", GetUIText(13), info.aqi <= 2 ? GetUIText(11) : GetUIText(12));

    if (strstr(info.description, "晴")) sprintf(asciiWeather, "      \\   /    \r\n       .-.     \r\n    -- (   ) -- \r\n       `-`     \r\n      /   \\    ");
    else if (strstr(info.description, "雨")) sprintf(asciiWeather, "     .--.      \r\n    (    ).    \r\n   (___(__)    \r\n    ' ' ' '    \r\n   ' ' ' '     ");
    else sprintf(asciiWeather, "      .--.     \r\n   .-(    ).   \r\n  (___.__)__)  \r\n               \r\n               ");

    if (info.aqi <= 2) sprintf(asciiAQI, "      SAFE     \r\n               \r\n     (^_^)     \r\n               \r\n      CLEAN    ");
    else sprintf(asciiAQI, "     WARNING   \r\n               \r\n     (!_!)     \r\n               \r\n      POLLUTED ");

    HWND hWImg = CreateWindowA("STATIC", asciiWeather, WS_CHILD | WS_VISIBLE | SS_LEFT, 680, 50, 200, 110, hPop, NULL, NULL, NULL);
    SendMessage(hWImg, WM_SETFONT, (WPARAM)CreateFont(18, 0, 0, 0, FW_BOLD, 0, 0, 0, 1, 0, 0, 0, 0, "Courier New"), TRUE);
    HWND hAImg = CreateWindowA("STATIC", asciiAQI, WS_CHILD | WS_VISIBLE | SS_LEFT, 680, 260, 200, 110, hPop, NULL, NULL, NULL);
    SendMessage(hAImg, WM_SETFONT, (WPARAM)CreateFont(18, 0, 0, 0, FW_BOLD, 0, 0, 0, 1, 0, 0, 0, 0, "Courier New"), TRUE);

    HWND hWText = CreateWindowA("STATIC", weatherStatus, WS_CHILD | WS_VISIBLE | SS_CENTER, 680, 165, 200, 30, hPop, NULL, NULL, NULL);
    SendMessage(hWText, WM_SETFONT, (WPARAM)CreateFont(18, 0, 0, 0, FW_BOLD, 0, 0, 0, 1, 0, 0, 0, 0, "Microsoft JhengHei"), TRUE);
    HWND hAText = CreateWindowA("STATIC", aqiStatus, WS_CHILD | WS_VISIBLE | SS_CENTER, 680, 375, 200, 30, hPop, NULL, NULL, NULL);
    SendMessage(hAText, WM_SETFONT, (WPARAM)CreateFont(18, 0, 0, 0, FW_BOLD, 0, 0, 0, 1, 0, 0, 0, 0, "Microsoft JhengHei"), TRUE);

    HWND hEdit = CreateWindowA("EDIT", report, WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 30, 30, 620, 640, hPop, NULL, NULL, NULL);
    SendMessage(hEdit, WM_SETFONT, (WPARAM)CreateFont(22, 0, 0, 0, FW_NORMAL, 0, 0, 0, 1, 0, 0, 0, 0, "Microsoft JhengHei"), TRUE);
    HWND hBack = CreateWindowA("BUTTON", GetUIText(7), WS_CHILD | WS_VISIBLE, 30, 690, 620, 65, hPop, (HMENU)888, NULL, NULL);
    SendMessage(hBack, WM_SETFONT, (WPARAM)CreateFont(24, 0, 0, 0, FW_BOLD, 0, 0, 0, 1, 0, 0, 0, 0, "Microsoft JhengHei"), TRUE);
}

// --- 主程式視窗與訊息循環 ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        hLabel = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 20, 800, 40, hwnd, NULL, NULL, NULL);
        SendMessage(hLabel, WM_SETFONT, (WPARAM)CreateFont(28, 0, 0, 0, FW_BOLD, 0, 0, 0, 1, 0, 0, 0, 0, "Microsoft JhengHei"), TRUE);

        hGroupInfo = CreateWindowA("BUTTON", "開發小組資訊 / Team Info", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 100, 380, 600, 230, hwnd, NULL, NULL, NULL);
        SendMessage(hGroupInfo, WM_SETFONT, (WPARAM)CreateFont(20, 0, 0, 0, FW_BOLD, 0, 0, 0, 1, 0, 0, 0, 0, "Microsoft JhengHei"), TRUE);
        const char* teamDetail = "小組 11\r\n\r\n114360204 柯冠義\r\n114360218 吳智揚\r\n114360240 謝宇豐";
        HWND hTeamText = CreateWindowA("STATIC", teamDetail, WS_CHILD | WS_VISIBLE | SS_CENTER, 20, 40, 560, 170, hGroupInfo, NULL, NULL, NULL);
        SendMessage(hTeamText, WM_SETFONT, (WPARAM)CreateFont(24, 0, 0, 0, FW_NORMAL, 0, 0, 0, 1, 0, 0, 0, 0, "Microsoft JhengHei"), TRUE);

        hLangBtns[0] = CreateWindowA("BUTTON", "繁體中文", WS_CHILD | WS_VISIBLE, 150, 650, 150, 45, hwnd, (HMENU)700, NULL, NULL);
        hLangBtns[1] = CreateWindowA("BUTTON", "English", WS_CHILD | WS_VISIBLE, 320, 650, 150, 45, hwnd, (HMENU)701, NULL, NULL);
        hLangBtns[2] = CreateWindowA("BUTTON", "Exit / 退出", WS_CHILD | WS_VISIBLE, 490, 650, 150, 45, hwnd, (HMENU)702, NULL, NULL);

        hTimeLabel = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 715, 800, 30, hwnd, NULL, NULL, NULL);
        SendMessage(hTimeLabel, WM_SETFONT, (WPARAM)CreateFont(22, 0, 0, 0, FW_BOLD, 0, 0, 0, 1, 0, 0, 0, 0, "Consolas"), TRUE);
        SetTimer(hwnd, 1, 1000, NULL);
        ShowCountryMenu(hwnd); break;

    case WM_TIMER: {
        char fullTime[100]; time_t now = time(NULL);
        strftime(fullTime, sizeof(fullTime), "INTERNATIONAL TAIPEI TIME (UTC+8): %Y-%m-%d %H:%M:%S", localtime(&now));
        SetWindowTextA(hTimeLabel, fullTime); break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 700 || id == 701) { langMode = id - 700; ShowCountryMenu(hwnd); }
        else if (id == 702) PostQuitMessage(0);
        else if (id == 999) ShowCountryMenu(hwnd);
        else if (id >= 100 && id < 104) ShowCityMenu(hwnd, id - 100);
        else if (id >= 200 && currentCountryIdx != -1) {
            City c = worldData[currentCountryIdx].cities[id - 200];
            WeatherInfo info; if (getWeather(c.nameEN, (char*)(langMode ? c.nameEN : c.nameTW), &info)) ShowWeatherReport(hwnd, info);
        }
        break;
    }
    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int main() {
    WNDCLASSA wc = { 0 }; wc.lpfnWndProc = WndProc; wc.hInstance = GetModuleHandle(NULL); wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH); wc.lpszClassName = "FinalUnifiedWeatherV12"; RegisterClassA(&wc);
    HWND hwnd = CreateWindowA("FinalUnifiedWeatherV12", "Satellite Weather System", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 850, 880, NULL, NULL, wc.hInstance, NULL);
    MSG msg; while (GetMessageA(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    return 0;
}