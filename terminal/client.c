#include <curl/curl.h>
#define HTTPS_Port "443"

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  size_t real_size = size * nmemb;
  printf("%.*s", (int)real_size, ptr);
  printf("\n");
  return real_size;
}

int main(int argc, char *argv[]) {

  /**
   * ToDo:
   * Build client side that can connect to HTTPS server
   * Port: 443
   */
  // curl_easy_setopt(); curl struct, type (CURLOPT_URL), request
  CURL *curl;
  CURLcode res; // Result from request
  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, magicPhrase);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    res = curl_easy_perform(curl);
  }
  return 0;
}
