#include "Libs/cJSON.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define HTTPS_Port "443"

// Number of Vehicles We Want to Display and get information on
#define MAX_SIZE 2
#define MAX_ID_SIZE 64

int GLOBAL_STOP_INDEX = 70254;
float SENSITIVITY = 0.0008;

typedef enum { PREDICTION, VEHICLE } PARSE_TYPE;

typedef struct {
  char *longitude;
  char *latitude;
  char **inbnd_whitelist;
  int inbnd_size;
  int inbnd_count;
  char **outbnd_whitelist;
  int outbnd_size;
  int outbnd_count;
} config;
typedef config *config_t;

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

config_t init_config() {
  config_t c = (config_t)malloc(sizeof(config));

  c->inbnd_size = 4;
  c->inbnd_count = 0;
  c->inbnd_whitelist = (char **)malloc(sizeof(char *) * c->inbnd_size);
  c->outbnd_size = 4;
  c->outbnd_count = 0;
  c->outbnd_whitelist = (char **)malloc(sizeof(char *) * c->outbnd_size);
  c->latitude = NULL;
  c->longitude = NULL;

  return c;
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
}

void add_whitelist_item(char *elem, char ***list, int *size, int *count) {
  if (*size == *count) {
    (*size) *= 2;
    *list = (char **)realloc(*list, sizeof(char *) * (*size));
  }

  (*list)[*count] = strdup(elem);
  (*count)++;
}

void print_Whitelist(char ***list, int *count) {
  fprintf(stderr, "{");
  for (int i = 0; i < *count; i++) {
    fprintf(stderr, "%s", (*list)[i]);
    if (i != (*count) - 1) {
      fprintf(stderr, ", ");
    }
  }
  fprintf(stderr, "}\n");
}

/**
 * Method to retrieve and save the Long and Lat data of a given address.
 */
void get_GEOCODING(cJSON *addr, config_t config) {
  // https://geocoding.geo.census.gov/geocoder/locations/onelineaddress?address=4600+Silver+Hill+Rd%2C+Washington%2C+DC+20233&benchmark=4&format=json
  CURL *curl;
  CURLcode res;

  curl = curl_easy_init();
  const char *base_url = "https://geocoding.geo.census.gov/geocoder/locations/"
                         "onelineaddress?address=";
  const char *end_params = "&benchmark=4&format=json";
  const char *address = addr->valuestring;

  char *encoded_address = curl_easy_escape(curl, address, strlen(address));
  if (!encoded_address) {
    fprintf(stderr, "URL encoding failed\n");
    curl_easy_cleanup(curl);
    exit(0);
  }

  char buffer[strlen(base_url) + strlen(encoded_address) + strlen(end_params) +
              1];
  buffer[0] = '\0';
  strlcat(buffer, base_url, sizeof(buffer));
  strlcat(buffer, encoded_address, sizeof(buffer));
  strlcat(buffer, end_params, sizeof(buffer));

  struct memory mem = {0};
  curl_easy_setopt(curl, CURLOPT_URL, buffer);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);
  res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    exit(0);
  }

  curl_free(encoded_address);
  curl_easy_cleanup(curl);

  cJSON *root = cJSON_Parse(mem.response);
  cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "result");
  cJSON *matches = cJSON_GetObjectItemCaseSensitive(result, "addressMatches");
  cJSON *firstElem = cJSON_DetachItemFromArray(matches, 0);
  cJSON *coordinates =
      cJSON_GetObjectItemCaseSensitive(firstElem, "coordinates");

  cJSON *x = cJSON_GetObjectItemCaseSensitive(coordinates, "x");
  cJSON *y = cJSON_GetObjectItemCaseSensitive(coordinates, "y");

  // config->longitude = x->valuestring;
  // config->latitude = y->valuestring;
  fprintf(stderr, "%.4f\n", x->valuedouble);
}

/**
 * Fills in the config structure with the set config file.
 * file: config filename.
 * config: struct used for filtering routes.
 */
void set_config(char *filename, config_t config) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    printf("Error: Cannot open file %s\n", filename);
    exit(0);
  }

  // Get file size
  fseek(fp, 0, SEEK_END);
  long file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  // Allocate memory for file content
  char *json_str = (char *)malloc(file_size + 1);
  if (!json_str) {
    printf("Error: Memory allocation failed\n");
    fclose(fp);
    exit(0);
  }

  fread(json_str, 1, file_size, fp);
  json_str[file_size] = '\0';
  fclose(fp);

  cJSON *root = cJSON_Parse(json_str);

  fprintf(stderr, "%s\n", cJSON_Print(root));

  // Use this for Long and Lat discovery with Geocoding US CENSUS
  cJSON *address = cJSON_GetObjectItem(root, "address");

  cJSON *inBoundArray = cJSON_GetObjectItemCaseSensitive(root, "inbound");
  cJSON *inbound_obj = NULL;
  cJSON *outBoundArray = cJSON_GetObjectItemCaseSensitive(root, "outbound");
  cJSON *outbound_obj = NULL;
  cJSON_ArrayForEach(inbound_obj, inBoundArray) {
    add_whitelist_item(inbound_obj->valuestring, &config->inbnd_whitelist,
                       &config->inbnd_size, &config->inbnd_count);
  }

  cJSON_ArrayForEach(outbound_obj, outBoundArray) {
    add_whitelist_item(outbound_obj->valuestring, &config->outbnd_whitelist,
                       &config->outbnd_size, &config->outbnd_count);
  }
  print_Whitelist(&config->outbnd_whitelist, &config->outbnd_count);
  print_Whitelist(&config->inbnd_whitelist, &config->inbnd_count);
  // Need to do
  //
  get_GEOCODING(address, config);
  cJSON_Delete(root);
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
 * Fills in information about the Vehicles current position.
 * object - JSON about information on the vehicle and information about the
 * stop. vehicle - a vehicle with an ID and TTA prediction set.
 *
 * Throws: If vehicle has not been set. The method is secondary to fill_TTA_ID
 */
void fill_Vehicle_Pos(cJSON *object, vehicle_t vehicle) {
  cJSON *included = cJSON_GetObjectItemCaseSensitive(object, "included");
  cJSON *data = cJSON_GetObjectItemCaseSensitive(object, "data");

  cJSON *stop = cJSON_GetObjectItemCaseSensitive(
      cJSON_GetObjectItemCaseSensitive(data, "relationships"), "stop");
  cJSON *stop_id = cJSON_GetObjectItemCaseSensitive(
      cJSON_GetObjectItemCaseSensitive(stop, "data"), "id");

  cJSON *status = cJSON_GetObjectItemCaseSensitive(
      cJSON_GetObjectItemCaseSensitive(data, "attributes"), "current_status");

  // cJSON name ----
  cJSON *stop_item = cJSON_DetachItemFromArray(included, 0);
  cJSON *name = cJSON_GetObjectItemCaseSensitive(
      cJSON_GetObjectItemCaseSensitive(stop_item, "attributes"), "name");
  fprintf(stderr, "%s %s %s\n", cJSON_Print(stop_id), cJSON_Print(status),
          cJSON_Print(name));

  vehicle->process = strdup(status->valuestring);
  vehicle->current_stop = strdup(name->valuestring);
  vehicle->num_stops_away = atoi(stop_id->valuestring);
}

/**
 * Takes in JSON with data on Time of Arrival and the Vehicles ID and fills it
 * into a given vehicles data.
 * object: JSON for the prediction filter for this given vehicle.
 * vehicle: item in the list of diplayed vehicles on route.
 *
 * Throws: If vehicle already has data in it.
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

/**
 * Parses and fills in Time to Arrival Data and ID data for up to MAX_SIZE
 * Vehichles in a given query. data: a cJSON representation of MBTA prediction
 * fields for a given filter. list: a list of MAX_SIZE that represents the
 * number of Vehicles we want to display and query at a given time.
 * ---
 * TBA: Limit testing(i.e. max size larger than query)
 */
void handle_Predictions(cJSON *data, vehicle_t list[MAX_SIZE]) {
  cJSON *allObjects = cJSON_GetObjectItemCaseSensitive(data, "data");
  cJSON *object = NULL;

  int count = 0;
  cJSON_ArrayForEach(object, allObjects) {
    if (count >= MAX_SIZE) {
      break;
    }
    fill_TTA_ID(object, list[count]);
    count++;
  }
}

/**
 * Goes through a list of vehicles that has prediction times and ID's and
 * fetches their current location. list: list of vehicles
 *
 * WARNING: This will be where most of your API calls will be chewed up on (1
 * call per Vehicle), if the following information isn't useful then remove the
 * function.
 *  - Number of stops the Vehicle is from the desired stop
 *  - It's current stop location
 *  - The process it is doing (i.e. STOPPED, INCOMING, etc)
 */
void get_real_time_pos(vehicle_t list[MAX_SIZE]) {
  if (list[0]->id == NULL) {
    fprintf(stderr, "LIST HAS NOT RECEIVED PREDICTION DATA\n");
    exit(0);
  }

  char *startphrase = "https://api-v3.mbta.com/vehicles/";
  char *includes = "?include=stop";

  CURL *curl;
  CURLcode res;
  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();
  for (int i = 0; i < MAX_SIZE; i++) {

    struct memory mem = {0};
    // curl_easy_reset(curl);
    char buffer[strlen(startphrase) + strlen(includes) + MAX_ID_SIZE + 1];
    buffer[0] = '\0';
    strlcat(buffer, startphrase, sizeof(buffer));
    strlcat(buffer, list[i]->id, sizeof(buffer));
    strlcat(buffer, includes, sizeof(buffer));
    fprintf(stderr, "%s\n", buffer);

    curl_easy_setopt(curl, CURLOPT_URL, buffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                     write_callback); // Need to add a write data structure
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&mem);
    res = curl_easy_perform(curl);

    cJSON *vehicle_data = cJSON_Parse(mem.response);
    fill_Vehicle_Pos(vehicle_data, list[i]);
    // fprintf(stderr, "%s\n", cJSON_Print(vehicle_data));
  }
  curl_easy_cleanup(curl);
}

/**
 * Routes JSON parsing and handling to the different types of functions to fill
 * in Vehicle data.
 *
 * mem: raw data returned from CURL operation.
 * list: a list of MAX_SIZE of vehicles to be displayed.
 * type: the type of processing this data will go through
 */
void parse_response(struct memory *mem, vehicle_t list[MAX_SIZE],
                    PARSE_TYPE type) {
  // Turn raw data into cJSON first.
  cJSON *data = cJSON_Parse(mem->response);
  // fprintf(stderr, "%s\n", cJSON_Print(data));
  switch (type) {
  case PREDICTION:
    handle_Predictions(data, list);
    break;
  case VEHICLE:
    break;
  default:
    fprintf(stderr, "Type not implemented yet, printing raw data:\n%s\n",
            mem->response);
  }
}

/**
 * Given a Route Map and two stops that are on that Route Map, will return 0 if
 * outbound of source and 1 if inbound of source. Route Map: An array of all the
 * stops on a given route. src: The source stop where we are displaying vehicles
 * dst: The desired location to configure
 */
int find_direction() {}

int main(int argc, char *argv[]) {

  config_t config = init_config();
  set_config("config", config);
  /**
   * ToDo:
   * GEOCODING to get Longitude and Latitude of start and destination (US Census
   * Bureau) MBTA smoothing
   * - Delay Notification, or status field.
   * - Build out "route maps in library"
   *
   * Vehicle Management
   * - If a given address has a stop for a type of vehicle:
   *    We use only vehicles with exact links to the location
   * Project dest stop onto source route.
   * Keep Bus and Train lines seperate for now.
   *
   * CHANGING IDEA (CONFIGURABLE SIGN):
   * - For instance I care only about certain buses and subways into the city
   * - And certain ones outbound, why not just build a config file to set and
   * run. In reality path finding does nothing but adds extra complexity, when
   * all I need is when the inbound train is coming, there are other programs to
   * tell me what else I need to take.
   */
  vehicle_t list[MAX_SIZE];
  for (int i = 0; i < MAX_SIZE; i++) {
    list[i] = init_vehicle();
  }

  CURL *curl;
  CURLcode res; // Result from request
  while (1) {
    struct memory mem = {0};
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
      curl_easy_setopt(curl, CURLOPT_URL, magicPhrase);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                       write_callback); // Need to add a write data structure
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&mem);
      res = curl_easy_perform(curl);

      parse_response(&mem, list, PREDICTION);

      get_real_time_pos(list);

      for (int i = 0; i < MAX_SIZE; i++) {
        fprintf(stderr, "%s: %.2f SECS | %.2f MINS | %s %s | %i\n", list[i]->id,
                list[i]->TTA, list[i]->TTA / 60, list[i]->process,
                list[i]->current_stop, list[i]->num_stops_away);
      }
    }
    for (int i = 0; i < MAX_SIZE; i++) {
      free_vehicle(list[i]);
      list[i] = init_vehicle();
    }
    sleep(20);
  }

  // Every Minute Send in One Request (1)
  // Grab two closest T's and two closest Buses and order them by time
  // Then call the each Vehichle to get stop information and stop name

  for (int i = 0; i < MAX_SIZE; i++) {
    free_vehicle(list[i]);
  }
  return 0;
}
