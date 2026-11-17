/* Host stand-in for the parts of ESP-IDF's esp_log.h the port uses. */
#ifndef ESP_LOG_H
#define ESP_LOG_H
typedef enum { ESP_LOG_NONE, ESP_LOG_ERROR, ESP_LOG_WARN, ESP_LOG_INFO } esp_log_level_t;
esp_log_level_t esp_log_level_get(const char *tag);
void esp_log_level_set(const char *tag, esp_log_level_t level);
#define ESP_LOG_LEVEL(level, tag, fmt, ...) ((void)(level), (void)(tag))
#endif
