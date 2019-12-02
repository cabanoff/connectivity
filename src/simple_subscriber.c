
/**
 * @file
 * A simple program that subscribes to a topic.
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>		/* sigaction */

#include <mqtt.h>
#include <posix_sockets.h>
#include <parse.h>

#define VERSION "1.500"
#define TOPIC_IN "mqtt-kontron/lora-gatway"
#define TOPIC_IN_RSSI "mqtt-kontron/lora-RSSI"
#define ADDR_IN  "localhost"
/**
  for testing with mosquitto
  type - mosquitto_sub -t testing
*/

//#define ADDR_OUT "localhost"
//#define TOPIC_OUT "connectivity/test"
#define ADDR_OUT "mqtt.thethings.io"
/*kontron*/
#define TOPIC_OUT2 "v2/things/LCJzGT3QL6jucKuFcuTyBbvQzYIMunWvHUK1ZKDdfuQ"
/*raspberry pi*/
#define TOPIC_OUT1 "v2/things/1ZtGJvaiCCoVbvlliX16R7tDwh1FxYnQfQgcySsam34"
/*GTW410-L*/
#define TOPIC_OUT3 "v2/things/GfPq9BoJgm73pGrynXafogS_6kzVBo2wWnmzGCKr4J0"


#define CONN_PERIOD_SEC 600


/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

typedef enum MQTTErrors MQTTErrors_t;

/**
 * signal handling variables
 */
struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
int exit_sig; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
int quit_sig; /* 1 -> application terminates without shutting down the hardware */


struct mqtt_client clientIn, clientInRSSI, clientOut;
uint8_t sendbufIn[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbufIn[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
uint8_t sendbufInRSSI[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbufInRSSI[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
uint8_t sendbufOut[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbufOut[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
int sockfdIn = -1;
int sockfdInRSSI = -1;
int sockfdOut = -1;
const char* addrIn;
const char* addrOut;
const char* port;
const char* topicIn;
const char* topicInRSSI;
const char* topicOut;

pthread_t client_daemonOut;
pthread_t client_daemonIn;
pthread_t client_daemonInRSSI;


/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

void sig_handler(int sigio);

/**
 * @brief The function will be called whenever a PUBLISH message is received.
 */
void publish_callback(void** unused, struct mqtt_response_publish *published);
void subscribe_callback(void** unused, struct mqtt_response_publish *published);
void subscribe_callback_RSSI(void** unused, struct mqtt_response_publish *published);
//void parse_save(const char* message, size_t size);

/**
 * @brief The client's refresher. This function triggers back-end routines to
 *        handle ingress/egress traffic to the broker.
 *
 * @note All this function needs to do is call \ref __mqtt_recv and
 *       \ref __mqtt_send every so often. I've picked 100 ms meaning that
 *       client ingress/egress traffic will be handled every 100 ms.
 */
void* publish_client_refresher(void* client);
void* subscribe_client_refresher(void* client);
int makePublisher(void);
/**
 * @brief Safelty closes the \p sockfd and cancels the \p client_daemon before \c exit.
 */
void exit_example(int status,int sockfdIn,int sockfdInRSSI,int sockfdOut,\
                pthread_t *client_daemonIn, pthread_t *client_daemonInRSSI, pthread_t *client_daemonOut);

int main(int argc, const char *argv[])
{


    exit_sig = 0;
    quit_sig = 0;

    addrIn = ADDR_IN;
    port = "1883";
    topicIn = TOPIC_IN;
    addrOut = ADDR_OUT;



    if (argc > 1) {
        switch ((uint8_t)strtol(argv[1], NULL, 10)){
            case 1:
            topicOut = TOPIC_OUT1;
            printf("Launched on Raspberry Pi\n");
            break;
            case 2:
            topicOut = TOPIC_OUT2;
            printf("Launched on Kontron\n");
            break;
            case 3:
            topicOut = TOPIC_OUT3;
            printf("Launched on GTW410-L\n");
            break;
            default:
            printf("argument should be 1 - 3\n");
            exit_example(EXIT_FAILURE,sockfdIn,sockfdInRSSI,sockfdOut,&client_daemonIn,&client_daemonInRSSI,&client_daemonOut);
        }
    } else {
        printf("you should call this file with argument 1 - 3\n");
        printf("1 - RaspberryPi BS \n");
        printf("2 - Kontron BS \n");
        printf("3 - GTW410-L BS \n");
        exit_example(EXIT_FAILURE,sockfdIn,sockfdInRSSI,sockfdOut,&client_daemonIn,&client_daemonInRSSI,&client_daemonOut);
    }
    /* get address (argv[1] if present) */
    /*
    if (argc > 1) {
        addrIn = argv[1];
    } else {
        addrIn = ADDR_IN;
    }
    */


    topicInRSSI = TOPIC_IN_RSSI;
    /* open the non-blocking TCP socket (connecting to the broker) */
    sockfdIn = open_nb_socket(addrIn, port);

    if(sockfdIn == -1){
        perror("Failed to open input socket: ");
        exit_example(EXIT_FAILURE,sockfdIn,sockfdInRSSI,sockfdOut,&client_daemonIn,&client_daemonInRSSI,&client_daemonOut);
    }


    mqtt_init(&clientIn, sockfdIn, sendbufIn, sizeof(sendbufIn), recvbufIn, sizeof(recvbufIn), subscribe_callback);
    mqtt_connect(&clientIn, "subscribing_client", NULL, NULL, 0, NULL, NULL, 0, 400);

    /* check that we don't have any errors */
    if (clientIn.error != MQTT_OK) {
        fprintf(stderr, "connect subscriber error: %s\n", mqtt_error_str(clientIn.error));
        exit_example(EXIT_FAILURE,sockfdIn,sockfdInRSSI,sockfdOut,&client_daemonIn,&client_daemonInRSSI,&client_daemonOut);
    }

    /* start a thread to refresh the subscriber client (handle egress and ingree client traffic) */

    if(pthread_create(&client_daemonIn, NULL, subscribe_client_refresher, &clientIn)) {
        fprintf(stderr, "Failed to start subscriber client daemon.\n");
        exit_example(EXIT_FAILURE,sockfdIn,sockfdInRSSI,sockfdOut,&client_daemonIn,&client_daemonInRSSI,&client_daemonOut);
    }

    /* subscribe */
    mqtt_subscribe(&clientIn, topicIn, 0);

    /* open the non-blocking TCP socket (connecting to the broker) for RSSI */
    sockfdInRSSI = open_nb_socket(addrIn, port);
    if(sockfdInRSSI == -1){
        perror("Failed to open input RSSI socket: ");
        exit_example(EXIT_FAILURE,sockfdIn,sockfdInRSSI,sockfdOut,&client_daemonIn,&client_daemonInRSSI,&client_daemonOut);
    }

    mqtt_init(&clientInRSSI, sockfdInRSSI, sendbufInRSSI, sizeof(sendbufInRSSI), recvbufInRSSI, sizeof(recvbufInRSSI), subscribe_callback_RSSI);
    mqtt_connect(&clientInRSSI, "subscribing_client_RSSI", NULL, NULL, 0, NULL, NULL, 0, 400);

    /* check that we don't have any errors */
    if (clientInRSSI.error != MQTT_OK) {
        fprintf(stderr, "connect subscriber RSSI error: %s\n", mqtt_error_str(clientInRSSI.error));
        exit_example(EXIT_FAILURE,sockfdIn,sockfdInRSSI,sockfdOut,&client_daemonIn,&client_daemonInRSSI,&client_daemonOut);
    }

    /* start a thread to refresh the subscriber client (handle egress and ingree client traffic) */

    if(pthread_create(&client_daemonInRSSI, NULL, subscribe_client_refresher, &clientInRSSI)) {
        fprintf(stderr, "Failed to start subscriber RSSI client daemon.\n");
        exit_example(EXIT_FAILURE,sockfdIn,sockfdInRSSI,sockfdOut,&client_daemonIn,&client_daemonInRSSI,&client_daemonOut);
    }

    /* subscribe */
    mqtt_subscribe(&clientInRSSI, topicInRSSI, 0);

    printf("Calculate connectivity software v%s\n", VERSION);
    printf("%s listening for '%s' and '%s' messages.\n", argv[0], topicIn, topicInRSSI);

    /* configure signal handling */
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = sig_handler;
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
    /* block */
    time_t timer = time(NULL);
    while ((quit_sig != 1) && (exit_sig != 1))
    {

        char* messageToPublish = parse_get_mess();
        int sent_ok = 0;
        if((timer + CONN_PERIOD_SEC) < time(NULL)){
            parse_make_message();
            timer = time(NULL);
        }
        if(messageToPublish != NULL){  //there is a mesasage to publish

            if(makePublisher() != -1){
                printf("%s published : \"%s\"\n", argv[0], messageToPublish);

                 /* republish the message */
                mqtt_publish(&clientOut, topicOut, messageToPublish, strlen(messageToPublish), MQTT_PUBLISH_QOS_0);
                if (clientOut.error == MQTT_OK) {
                    if(mqtt_sync(&clientOut) == MQTT_OK) sent_ok = 1;
                }
                usleep(2000); //delay 2ms
            }
            if(sent_ok != 0){
                parse_next_mess();
            }else{
                fprintf(stderr, "publisher error: %s\n", mqtt_error_str(clientOut.error));
                printf("publisher error: %s\n", mqtt_error_str(clientOut.error));
                usleep(1000000U);
            }
            if (sockfdOut != -1) close(sockfdOut);  //close publisher
        }

        //usleep(100000);
    }

    /* disconnect */
    printf("\n%s disconnecting from %s\n", argv[0], addrIn);
    printf("\n%s disconnecting from %s\n", argv[0], addrOut);
    sleep(1);

    /* exit */
    //exit_example(EXIT_SUCCESS, sockfdIn, sockfdOut, &client_daemonIn, &client_daemonOut);
    exit_example(EXIT_SUCCESS,sockfdIn,sockfdInRSSI,sockfdOut,&client_daemonIn,&client_daemonInRSSI,&client_daemonOut);
}

/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

int makePublisher(void){

    sockfdOut = open_nb_socket(addrOut, port);

    if(sockfdOut == -1){
        perror("Failed to open input socket: ");
        //exit_example(EXIT_FAILURE, sockfdIn, sockfdOut, NULL, NULL);
        return -1;
    }
    mqtt_init(&clientOut, sockfdOut, sendbufOut, sizeof(sendbufOut), recvbufOut, sizeof(recvbufOut), publish_callback);
    mqtt_connect(&clientOut, "publishing_client", NULL, NULL, 0, NULL, NULL, 0, 400);

    /* check that we don't have any errors */
    if (clientOut.error != MQTT_OK) {
        fprintf(stderr, "connect publisher error: %s\n", mqtt_error_str(clientOut.error));
        //exit_example(EXIT_FAILURE, sockfdIn, sockfdOut,NULL,NULL);
        if (sockfdOut != -1) close(sockfdOut);
        return -1;

    }
    return 0;
     /* start a thread to refresh the publisher client (handle egress and ingree client traffic) */
//
//    if(pthread_create(&client_daemonOut, NULL, publish_client_refresher, &clientOut)) {
//        fprintf(stderr, "Failed to start publisher client daemon.\n");
//        //exit_example(EXIT_FAILURE, sockfdIn, sockfdOut,NULL, NULL);
//        if (sockfdOut != -1) close(sockfdOut);
//        return;
//    }

}

void sig_handler(int sigio) {
	if (sigio == SIGQUIT) {
		quit_sig = 1;
	} else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
		exit_sig = 1;
	}
}
void exit_example(int status,int sockfdIn,int sockfdInRSSI,int sockfdOut, pthread_t *client_daemonIn, pthread_t *client_daemonInRSSI, pthread_t *client_daemonOut)
{
    if (sockfdIn != -1) close(sockfdIn);
    if (sockfdInRSSI != -1) close(sockfdInRSSI);
    if (sockfdOut != -1) close(sockfdOut);
    if (client_daemonIn != NULL) pthread_cancel(*client_daemonIn);
    if (client_daemonInRSSI != NULL) pthread_cancel(*client_daemonInRSSI);
    if (client_daemonOut != NULL) pthread_cancel(*client_daemonOut);
    exit(status);
}


void publish_callback(void** unused, struct mqtt_response_publish *published)
{
     /* not used in this example */
}


void subscribe_callback(void** unused, struct mqtt_response_publish *published)
{
    /* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
    char* topic_name = (char*) malloc(published->topic_name_size + 1);
    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';

    /*save only 32 bytes message, for example:
    2019-11-08 18:49:04.413Z,02000000BF070000000000000000000000000000000000000000000000000000 - 89 chars
    */

    if(published->application_message_size == 89)parse_save((const char*) published->application_message,published->application_message_size);

    //printf("Received publish from '%s' : %s\n", topic_name, (const char*) published->application_message);

    free(topic_name);
}

void subscribe_callback_RSSI(void** unused, struct mqtt_response_publish *published)
{
    /* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
    char* inMessage = (char*) malloc(published->application_message_size + 1);
    memcpy(inMessage, published->application_message, published->application_message_size);
    inMessage[published->application_message_size] = '\0';

    /*save only 32 bytes message, for example:
    2019-11-08 18:49:04.413Z,02000000BF070000000000000000000000000000000000000000000000000000 - 89 chars
    */

    parse_save_RSSI((const char*) inMessage,published->application_message_size+1);

    //printf("Received publish from '%s' : %s\n", topic_name, (const char*) published->application_message);

    free(inMessage);
}

void* publish_client_refresher(void* client)
{
    MQTTErrors_t error;
    while(1)
    {
        error = mqtt_sync((struct mqtt_client*) client);
        if( error != MQTT_OK){
            fprintf(stderr, "publisher error: %s\n", mqtt_error_str(error));
        }
        usleep(100000U);
    }
    return NULL;
}
void* subscribe_client_refresher(void* client)
{
    while(1)
    {
        mqtt_sync((struct mqtt_client*) client);
        usleep(100000U);
    }
    return NULL;
}

