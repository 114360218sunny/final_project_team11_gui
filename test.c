#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h> 
#include "cJSON.h" 
#include <curl/curl.h>

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
    char *ptr;
    size_t len;
};

// --- 基礎工具函式 ---
void init_string(struct string *s) {
    s->len = 0;
    s->ptr = malloc(s->len + 1);
    if (s->ptr == NULL) exit(EXIT_FAILURE);
    s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s) {
    size_t new_len = s->len + size * nmemb;
    s->ptr = realloc(s->ptr, new_len + 1);
    if (s->ptr == NULL) exit(EXIT_FAILURE);
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;
    return size * nmemb;
}

// ★記得填入你的 API Key
const char* API_KEY = "2fa80dc7ec8be3fac297f88afd028de9"; 

int getWeather(char* searchName, char* displayName, WeatherInfo* info) {
    CURL *curl;
    CURLcode res;
    struct string s;
    init_string(&s);

    curl = curl_easy_init();
    if(curl) {
        char *encodedName = curl_easy_escape(curl, searchName, 0);

        char url[512];
        sprintf(url, "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric&lang=zh_tw", encodedName, API_KEY);

        curl_free(encodedName); 

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
        
        res = curl_easy_perform(curl);
        
        if(res != CURLE_OK) {
            printf("連線失敗 (Error Code: %d)\n", res);
            printf("原因: %s\n", curl_easy_strerror(res)); 
            return 0;
        }

        cJSON *json = cJSON_Parse(s.ptr);
        if (json == NULL) return 0;

        cJSON *cod = cJSON_GetObjectItem(json, "cod");
        int statusCode = 0;
        if (cJSON_IsNumber(cod)) statusCode = cod->valueint;
        else if (cJSON_IsString(cod)) statusCode = atoi(cod->valuestring);

        if (statusCode != 200) {
             printf(">> 查詢失敗 (Code: %d) - 請檢查 API Key。\n", statusCode);
             return 0;
        }

        cJSON *main_obj = cJSON_GetObjectItem(json, "main");
        cJSON *temp = cJSON_GetObjectItem(main_obj, "temp");
        cJSON *humidity = cJSON_GetObjectItem(main_obj, "humidity");
        cJSON *weather_arr = cJSON_GetObjectItem(json, "weather");
        cJSON *weather_item = cJSON_GetArrayItem(weather_arr, 0);
        cJSON *desc = cJSON_GetObjectItem(weather_item, "description");

        strcpy(info->city, displayName);
        info->temperature = temp->valuedouble;
        info->humidity = humidity->valueint;
        strcpy(info->description, desc->valuestring);

        cJSON_Delete(json);
        curl_easy_cleanup(curl);
        free(s.ptr);
        return 1;
    }
    return 0;
}

void saveHistory(WeatherInfo info) {
    FILE *fp = fopen("history.txt", "a");
    if (fp != NULL) {
        fprintf(fp, "%s | %.1f°C | %s\n", info.city, info.temperature, info.description);
        fclose(fp);
    }
}

int main() {
    SetConsoleOutputCP(65001);

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
    int countryChoice, cityChoice;
    WeatherInfo currentData;

    printf("=== 天氣觀測系統 (Global Weather System) ===\n");

    while(1) { 
        printf("\n請選擇國家/地區:\n");
        for(int i=0; i<countryCount; i++) {
            printf("%d. %s\n", i+1, worldData[i].countryName);
        }
        printf("0. 離開系統\n");
        printf("輸入選項: ");
        
        if (scanf("%d", &countryChoice) != 1) {
            while(getchar() != '\n');
            continue;
        }

        if (countryChoice == 0) break; 

        if (countryChoice > 0 && countryChoice <= countryCount) {
            
            Country selectedCountry = worldData[countryChoice - 1];

            while(1) {
                printf("\n-- 您選擇了 [%s] --\n", selectedCountry.countryName);
                printf("請選擇城市:\n");
                for(int j=0; j<selectedCountry.cityCount; j++) {
                    printf("%2d. %-14s", j+1, selectedCountry.cities[j].nameTW);
                    if ((j+1) % 2 == 0) printf("\n");
                }
                if (selectedCountry.cityCount % 2 != 0) printf("\n"); 
                
                printf(" 0. 返回上一頁\n");
                printf("輸入選項: ");

                if (scanf("%d", &cityChoice) != 1) {
                    while(getchar() != '\n');
                    continue;
                }

                if (cityChoice == 0) break; 

                if (cityChoice > 0 && cityChoice <= selectedCountry.cityCount) {
                    City targetCity = selectedCountry.cities[cityChoice - 1];

                    printf("\n正在連線至衛星查詢 %s ...\n", targetCity.nameTW);

                    if (getWeather(targetCity.nameAPI, targetCity.nameTW, &currentData)) {
                        printf("\n============================\n");
                        printf(" 地區: %s\n", currentData.city);
                        printf(" 氣溫: %.1f °C\n", currentData.temperature);
                        printf(" 濕度: %d %%\n", currentData.humidity);
                        printf(" 現況: %s\n", currentData.description);
                        printf("============================\n");
                        saveHistory(currentData);
                        
                        printf("\n按 Enter 繼續...");
                        getchar(); getchar(); 
                    }
                } else {
                    printf("無效的城市選項。\n");
                }
            } 
        } else {
            printf("無效的國家選項。\n");
        }
    } 

    printf("系統已關閉。\n");
    return 0;
}