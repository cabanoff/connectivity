#ifndef __PARSE_H__
#define __PARSE_H__



/**
 * @brief The function saves published message
 * @param[in] message reference to received message.
 *
 * @param[in] size size of the received message.
 *
 * @returns none
 */
 void parse_save(const char* message, size_t size);

/**
 * @brief This is called every time when a new mqtt_RSSI message arrives
 *
 * @param[in] message - a new mqtt_RSSI message
 *            size - its size
 *
 * @returns none
 */

 void parse_save_RSSI(const char* message, size_t size);

/**
 * @brief returns first message in stack.
 *
 * @param[in] none
 *
 * @returns NULL if no mesages stored
 */
 char* parse_get_mess(void);

/**
 * @brief increments counter for publishing message
 *   is called in main() after publishing mqtt message
 *
 * @param[in] none
 *
 * @returns none
 */
 void parse_next_mess(void);

/**
 * @brief This is called once in period for calculating connectivity
 *
 * @param[in] none
 *
 * @returns none
 */
void parse_make_message(void);

#endif

