#include "mqtt.h"

#include <mosquittopp.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

Mqtt::Mqtt(const char * _id, const char * _host, int _port, const char * _username, const char * _password, const char * _will_topic, const char * _will_message) : mosquittopp(_id)
{
    int version = MQTT_PROTOCOL_V311;
    mosqpp::lib_init();
    this->keepalive = 30;
    this->id = _id;
    this->port = _port;
    this->host = _host;
    this->will_topic = _will_topic;
    this->will_message = _will_message;
    reinitialise(this->id, true);
    // Set version to 3.1.1
    opts_set(MOSQ_OPT_PROTOCOL_VERSION, &version);
    // Set username and password if non-null
    if (strlen(_username) > 0 && strlen(_password) > 0) {
        std::cerr << "Using credentials: " << _username << ":" << _password << std::endl;
        username_pw_set(_username, _password);
    }
    // Set last will and testament (LWT) message
    if (will_topic != NULL && will_message != NULL) {
        int rc = set_will(will_topic, will_message);
        if ( rc ) {
            std::cout <<">> Mqtt - set LWT message to: " << will_message << std::endl;
        } else {
            std::cout <<">> Mqtt - Failed to set LWT message!" << std::endl;
        }
    }

    // non blocking connection to broker request;
    connect_async(host, port, keepalive);
    // Start thread managing connection / publish / subscribekeepalive);
    loop_start();
};

Mqtt::~Mqtt() {
    // loop_stop();
    mosqpp::lib_cleanup();
}

bool Mqtt::set_will(const char * _topic, const char * _message)
{
    int ret = will_set(_topic, strlen(_message), _message, 1, true);
    return ( ret == MOSQ_ERR_SUCCESS );
}

void Mqtt::on_disconnect(int rc) {
    std::cout << ">> Mqtt - disconnected(" << rc << ")" << std::endl;
}

void Mqtt::on_connect(int rc)
{
    if ( rc == 0 ) {
        std::cout << ">> Mqtt - connected" << std::endl;
    } else {
        std::cout << ">> Mqtt - failed to connect: (" << rc << ")" << std::endl;
    }
}

void Mqtt::on_publish(int mid)
{
    //std::cout << ">> Mqtt - Message (" << mid << ") published " << std::endl;
}

bool Mqtt::send(const char * _topic, const char * _message, int qos, bool retain)
{
    // Send - depending on QoS, mosquitto lib managed re-submission this the thread
    //
    // * NULL : Message Id (int *) this allow to latter get status of each message
    // * topic : topic to be used
    // * length of the message
    // * message
    // * qos (0,1,2)
    // * retain (boolean) - indicates if message is retained on broker or not
    // Should return MOSQ_ERR_SUCCESS
    std::cout << _topic << "    " << _message << ((qos==0)?"*":"") << std::endl;
    int ret = publish(NULL, _topic, strlen(_message), _message, qos, retain);
    return ( ret == MOSQ_ERR_SUCCESS );
}
