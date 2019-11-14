/**
 * @file
 * parsing mqtt message
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <mqtt.h>
//#include <posix_sockets.h>
#include <parse.h>
//"[{\"key\":\"MartaRum\",\"value\":1,\"datetime\":\"%s\"}]"
#define DEBUG_PUBLISH "[{\"key\":\"MartaRum\",\"value\":1}]"
#define MAX_JSON_SIZE 400
#define MAX_MESSAGES 100
#define CURRENT sensorMess[messCounter]
#define DEV_NUM 6

/**
 * @brief The function saves published message
 * @param[in] message reference to received message.
 *
 * @param[in] size size of the received message.
 *
 * @returns none
 */
typedef struct{
    char jsonStr[MAX_JSON_SIZE];    // 1 - activity1, 2 - Rum1, 3 - Chew1, 4 - Rest1
    uint32_t deviceID;
    uint32_t messageID;
    uint32_t firstMess[DEV_NUM];
    uint32_t lastMess[DEV_NUM];
    uint32_t numMess[DEV_NUM];
    char time[20];
    uint8_t counter;
}sensorMess_t;

size_t messageSize;
sensorMess_t sensorMess[MAX_MESSAGES];           //reserv memory for 200 messages
uint32_t messCounter = 0;
uint32_t messToSend = 0;
uint32_t devicesList[] = {1,2,3,4,5,6};   //devices for calculating connectivity
int rssiValue;
uint32_t rssiSnrCount;
int snrValue;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#define DEV_LIST_SIZE (sizeof(devicesList)/sizeof(uint32_t))

//char* messToSend = NULL;

/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */
int getDevInfo(char*,sensorMess_t*);
/**
 * @brief This is called every time when a new mqtt message arrives
 *
 * @param[in] message - a new mqtt message
 *            size - its size
 *
 * @returns none
 */

 void parse_save(const char* message, size_t size)
 {

    char* startSensorInfo = strchr(message, ',') + 1; //next sympol after comma should be sensor message
    uint32_t deviceInfo = getDevInfo(startSensorInfo,&CURRENT);
    if(deviceInfo == 0)return; //message is not vallid;
    if(DEV_LIST_SIZE > DEV_NUM){
        printf("Error: devices list is more than DEV_NUM");
        return;
    }
    for(uint32_t i = 0; i < DEV_LIST_SIZE; i++){
        if(devicesList[i] == sensorMess[messCounter].deviceID){
            sensorMess[messCounter].lastMess[i] = sensorMess[messCounter].messageID;
            sensorMess[messCounter].numMess[i]++;
        }
    }
 }

 /**
 * @brief This is called every time when a new mqtt_RSSI message arrives
 *
 * @param[in] message - a new mqtt_RSSI message
 *            size - its size
 *
 * @returns none
 */

 void parse_save_RSSI(const char* message, size_t size)
 {
    int RSSI = atoi(message);
    int SNR = atoi(strchr(message,',')+1);
    rssiValue += RSSI;
    snrValue += SNR;
    rssiSnrCount++;
   // printf("%s\n",message);
   // printf("RSSI = %d, SNR = %d\n",RSSI,SNR);
 }

 /**
 * @brief This is called once in period for calculating connectivity
 *
 * @param[in] none
 *
 * @returns none
 */
 float debConn[DEV_NUM];
 uint debConDev = 0;
 uint debFirstMess[DEV_NUM];
 uint debLastMess[DEV_NUM];
 uint debNumMess[DEV_NUM];


void parse_make_message(void)
{
    time_t timer;
    time(&timer);
    struct tm* tm_info = gmtime(&timer);
    char timebuf[20];
    strftime(timebuf, 20, "%Y%m%d%H%M%S", tm_info);
    memcpy(CURRENT.time,timebuf,20);  // save time
    float connectivity = 0;
    int connectedDev = 0;
    int recMessages = 0;
    int averSNR;
    int averRSSI;
    for(int i = 0; i < DEV_NUM; i++){
        if((sensorMess[messCounter].lastMess[i] - sensorMess[messCounter].firstMess[i])>0){
/**deb section **************************************/
            debFirstMess[i] = sensorMess[messCounter].firstMess[i];
            debLastMess[i] = sensorMess[messCounter].lastMess[i];
            debNumMess[i] = sensorMess[messCounter].numMess[i];
            debConn[i] = (float)debNumMess[i]/(sensorMess[messCounter].lastMess[i] - sensorMess[messCounter].firstMess[i]);

/*****************************************************/
            connectivity += (float)sensorMess[messCounter].numMess[i]/(sensorMess[messCounter].lastMess[i] - sensorMess[messCounter].firstMess[i]);
            connectedDev++;
            recMessages += sensorMess[messCounter].numMess[i];
        }
    }
    debConDev = connectedDev;  /***** debug section *****/
    if(connectedDev)connectivity /= connectedDev;
    connectivity *= 100;
    if(connectivity > 100)connectivity = 100;   //for the first count

    pthread_mutex_lock(&lock);
    if(rssiSnrCount){
        averSNR = snrValue/rssiSnrCount;
        averRSSI = rssiValue/rssiSnrCount;
    }
    snrValue = 0;
    rssiValue = 0;
    rssiSnrCount = 0;
    pthread_mutex_unlock(&lock);

    /*
    {
      "values": [
        {
          "key": "temp1",
          "value": 41.1,
          "datetime": "20190915193200"
        },
        {
          "key": "temp2",
          "value": 50,
          "datetime": "20190915193200"
        }
      ]
    }
    */
    snprintf(sensorMess[messCounter].jsonStr,MAX_JSON_SIZE,\
"{\"values\":[{\"key\":\"Connectivity\",\"value\":%d,\"datatime\":\"%s\"},\
{\"key\":\"Messages\",\"value\":%d,\"datatime\":\"%s\"},\
{\"key\":\"RSSI\",\"value\":%d,\"datatime\":\"%s\"},\
{\"key\":\"SNR\",\"value\":%d,\"datatime\":\"%s\"}]}",\
    (int)connectivity, timebuf,recMessages,timebuf,averRSSI,timebuf,averSNR,timebuf);

    if(messCounter < (MAX_MESSAGES-1)){
        messCounter++;
        /*prepare for next period*/
        pthread_mutex_lock(&lock);
        for(int i = 0; i < DEV_NUM; i++){
            sensorMess[messCounter].firstMess[i] = sensorMess[messCounter-1].lastMess[i];
            sensorMess[messCounter].lastMess[i] = sensorMess[messCounter-1].lastMess[i];
            sensorMess[messCounter].numMess[i] = 0;
        }
        pthread_mutex_unlock(&lock);
    }else messCounter = 0;
    if(messCounter == messToSend)parse_next_mess();   //we've lost one message
    //printf("Message made, messCounter = %d, messToSend = %d\n",messCounter, messToSend);
}

/**
 * @brief returns prepared message
 *     this function is constantly called in the main()
 *      to check if message to publish is ready
 *
 * @param[in] none
 *
 * @returns message to be sent
 */

  char* parse_get_mess(void)
  {
    if(messToSend != messCounter){
        return(sensorMess[messToSend].jsonStr);
    }else return NULL;
  }

/**
 * @brief increments counter for publishing message
 *   is called in main() after publishing mqtt message
 *
 * @param[in] none
 *
 * @returns none
 */
 void parse_next_mess(void)
 {
    if(messToSend < (MAX_MESSAGES-1))messToSend ++;
    else messToSend = 0;
    //printf("Message sent, messCounter = %d, messToSend = %d\n",messCounter, messToSend);
 }

 /* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

 /**
 * @brief returns device ID in string
 *  first 4 bytes of message is device ID
 *  4bytes = 8 digits
 *  01000000 970F0000 00 01 9F000000 00 00 00000001000000000000000000000000
 * @param[in] message received from sensor,
 * @param[in] sensorMess struct for store sensor information,
 *
 * @returns 0 if not apropiate format of the input messsage
 */
#define IS_DIGIT(x)  ((x>='0')&&(x<='9'))||((x>='a')&&(x<='f'))||((x>='A')&&(x<='F'))


 int getDevInfo(char* message,sensorMess_t* sensorMess)
{
    for(int i = 0; i < 64; i++)if(!(IS_DIGIT(message[i])))return 0;  //message should contain only hex digits

    /*find sensor ID*/
    char DevIDstring[9]; //8 digits and \0
    DevIDstring[0] = message[6];
    DevIDstring[1] = message[7];
    DevIDstring[2] = message[4];
    DevIDstring[3] = message[5];
    DevIDstring[4] = message[2];
    DevIDstring[5] = message[3];
    DevIDstring[6] = message[0];
    DevIDstring[7] = message[1];
    DevIDstring[8] = 0;
    uint32_t number = (uint32_t)strtol(DevIDstring, NULL, 16);
    sensorMess->deviceID = number;

    char MessIDstring[9]; //8 digits and \0
    MessIDstring[0] = message[6+8];
    MessIDstring[1] = message[7+8];
    MessIDstring[2] = message[4+8];
    MessIDstring[3] = message[5+8];
    MessIDstring[4] = message[2+8];
    MessIDstring[5] = message[3+8];
    MessIDstring[6] = message[0+8];
    MessIDstring[7] = message[1+8];
    MessIDstring[8] = 0;
    number = (uint32_t)strtol(MessIDstring, NULL, 16);
    sensorMess->messageID = number;

    return 1;

}

