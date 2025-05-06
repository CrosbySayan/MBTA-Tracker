#include "Libs/cJSON.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define HTTPS_Port "443"

// Number of Vehicles We Want to Display and get information on
#define MAX_SIZE 4
#define MAX_ID_SIZE 32

typedef enum { PREDICTION, VEHICLE } PARSE_TYPE;

typedef struct {
  char *id;           // Name of Vehicle
  double TTA;         // Expected time of Arrival
  char *current_stop; // Current Stop Name
  int num_stops_away; // Number of stops from target
  char *process;      // Process between stops
} Vehicle;
typedef Vehicle *vehicle_t;

vehicle_t init_vehicle() {
  vehicle_t v = (vehicle_t)malloc(sizeof(Vehicle));
  v->id = NULL;
  v->TTA = 0.0;
  v->current_stop = NULL;
  v->num_stops_away = -1;
  v->process = NULL;
  return v;
}

void free_vehicle(vehicle_t v) {
  free(v->id);
  free(v->current_stop);
  free(v->process);
  free(v);
}

/**
 * Helper function that translates ISO time into seconds from current
 */
time_t ISO_to_Seconds(const char *iso_timestamp) {
  struct tm tm = {0};
  int year, month, day, hour, minute, second;
  int tz_hour = 0, tz_minute = 0;
  char tz_sign = '+';

  // Parse the main part of the timestamp
  sscanf(iso_timestamp, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour,
         &minute, &second);

  // Parse timezone info if present
  const char *tz_part = strchr(iso_timestamp, '+');
  if (!tz_part) {
    tz_part = strchr(iso_timestamp, '-');
    if (tz_part &&
        tz_part > iso_timestamp + 10) { // Make sure it's not the date hyphen
      tz_sign = '-';
    } else {
      tz_part = NULL;
    }
  }

  if (tz_part) {
    sscanf(tz_part + 1, "%d:%d", &tz_hour, &tz_minute);
  }

  // Set up the tm structure
  tm.tm_year = year - 1900; // Years since 1900
  tm.tm_mon = month - 1;    // Months 0-11
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = second;
  tm.tm_isdst = -1; // Let the system determine DST

  // Convert to time_t (seconds since epoch)
  time_t timestamp_time = mktime(&tm);

  // Apply timezone offset (convert from local to UTC)
  int tz_offset_seconds = tz_hour * 3600 + tz_minute * 60;
  if (tz_sign == '+') {
    timestamp_time -= tz_offset_seconds;
  } else {
    timestamp_time += tz_offset_seconds;
  }

  return timestamp_time;
}

double Seconds_from_Current(const char *timestamp) {
  time_t arr_time = ISO_to_Seconds(timestamp);
  time_t current_time;
  time(&current_time);

  return difftime(arr_time, current_time);
}
/**
 * Turns our JSON request of a given prediction into human readable format.
 */
char *processJSON(cJSON *object) {
  cJSON *attributes = cJSON_GetObjectItemCaseSensitive(object, "attributes");
  cJSON *arrTime = cJSON_GetObjectItemCaseSensitive(attributes, "arrival_time");
  fprintf(stderr, "%s\n", arrTime->valuestring);
  double time = Seconds_from_Current(arrTime->valuestring);
  fprintf(stderr, "%.2f SECS\n", time);
  fprintf(stderr, "%.2f MINS\n", (time / 60));
  return NULL;
}

/**
 * Takes in JSON with data on Time of Arrival and the Vehicles ID and fills it
 * into a given vehicles data. Throws: If vehicle already has data in it.
 */
void fill_TTA_ID(cJSON *object, vehicle_t vehicle) {
  if (vehicle->id != NULL) {
    fprintf(stderr,
            "ERROR: vehichle is either not initiliazed or prepopulated\n");
    exit(0);
  }

  cJSON *attributes = cJSON_GetObjectItemCaseSensitive(object, "attributes");
  cJSON *relationships =
      cJSON_GetObjectItemCaseSensitive(object, "relationships");
  cJSON *json_vehicle =
      cJSON_GetObjectItemCaseSensitive(relationships, "vehicle");
  cJSON *arrival_time =
      cJSON_GetObjectItemCaseSensitive(attributes, "arrival_time");
  if (cJSON_IsString(arrival_time) && (arrival_time->valuestring != NULL)) {
    vehicle->TTA = Seconds_from_Current(arrival_time->valuestring);
  }

  cJSON *id = cJSON_GetObjectItemCaseSensitive(
      cJSON_GetObjectItemCaseSensitive(json_vehicle, "data"), "id");

  vehicle->id = (char *)malloc(sizeof(char) * MAX_ID_SIZE);
  strlcpy(vehicle->id, id->valuestring, sizeof(id->valuestring));
}

struct memory {
  char *response;
  size_t size;
};

size_t write_callback(char *data, size_t size, size_t nmemb, void *userdata) {
  size_t realsize = size * nmemb;
  struct memory *mem = (struct memory *)userdata;
  char *ptr = realloc(mem->response, mem->size + realsize + 1);
  if (ptr == NULL)
    return 0; /* out of memory! */

  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), data, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;

  return realsize;

  cJSON *json = cJSON_Parse(ptr);
  // printf("\n");
  fprintf(stderr, "%s\n", cJSON_Print(json));
  cJSON *allObjects = cJSON_GetObjectItemCaseSensitive(json, "data");
  cJSON *object = NULL;
  int first = 1;
  cJSON_ArrayForEach(object, allObjects) { processJSON(object); }
}

void parse_response() {}

int main(int argc, char *argv[]) {
  // 1 means inbound 0 means outbound
  /**
   * ToDo:
   * Build client side that can connect to HTTPS server
   * Trains are inconsistent, provide stop data as well for accurate gauging
   */
  // curl_easy_setopt(); curl struct, type (CURLOPT_URL), request
  vehicle_t veh = init_vehicle();

  CURL *curl;
  CURLcode res; // Result from request

  struct memory mem = {0};
  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, magicPhrase);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                     write_callback); // Need to add a write data structure
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&mem);
    res = curl_easy_perform(curl);

    fprintf(stderr, "%s\n", mem.response);
  }

  // Every Minute Send in One Request (1)
  // Grab two closest T's and two closest Buses and order them by time
  // Then call the each Vehichle to get stop information and stop name
  return 0;
}
