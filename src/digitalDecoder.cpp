#include "digitalDecoder.h"
#include "mqtt.h"
#include "mqtt_config.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <csignal>

// #include <algorithm>
// #include <cctype>
// #include <string>

// Pulse checks seem to be about 60-70 minutes apart
#define RX_TIMEOUT_MIN      (90)

// Give each sensor 3 intervals before we flag a problem
#define SENSOR_TIMEOUT_MIN  (90*5)

#define SYNC_MASK    0xFFFF000000000000ul
#define SYNC_PATTERN 0xFFFE000000000000ul

// Don't send these messages more than once per minute unless there is a state change
#define RX_GOOD_MIN_SEC (60)
#define UPDATE_MIN_SEC (60)

#define BASE_NAME std::string("sensors345")
#define BASE_TOPIC "security/sensors345/"
#define SENSOR_TOPIC BASE_TOPIC"sensor/"
#define KEYFOB_TOPIC BASE_TOPIC"keyfob/"
#define KEYPAD_TOPIC BASE_TOPIC"keypad/"
#define AUTODISCOVERY_TOPIC "homeassistant/binary_sensor/sensors345-"

#define LOOP1_NAME "loop1"
#define LOOP2_NAME "loop2"
#define LOOP3_NAME "loop3"
#define TAMPER_NAME "tamper"
#define BATTERY_NAME "battery"
#define HB_NAME "hb"

#define AUTODISCOVERY_INTERVAL 60
#define UPDATE_INTERVAL 2
#define SENSOR_EXPIRATION 4500

#define MANUFACTURER_CODE_UNKNOWN 0
#define MANUFACTURER_CODE_HONEYWELL 1
#define MANUFACTURER_CODE_VIVINT 2
#define MANUFACTURER_CODE_2GIG 3

void addKeyValue(std::ostringstream& stream, const std::string& key, const std::string& value, bool comma=true) {
    stream << "\"" << key << "\": \"" << value << "\"";
    if (comma) {
        stream << ",";
    }
}

void addKeyValue(std::ostringstream& stream, const std::string& key, const int& value, bool comma=true) {
    stream << "\"" << key << "\": " << value;
    if (comma) {
        stream << ",";
    }
}

// std::string toLower(std::string data) 
// {
//     std::transform(data.begin(), data.end(), data.begin(),
//         [](unsigned char c){ return std::tolower(c); });
//     return data;
// }

std::string getDiscoveryTopic(uint32_t serial, const std::string& suffix) {
    std::ostringstream topic;
    topic << AUTODISCOVERY_TOPIC << serial << "/" << BASE_NAME << "-" << serial << "-" << suffix << "/config";
    return topic.str();
}

std::string getSensorTopic(uint32_t serial, const std::string& suffix) {
    std::ostringstream topic;
    topic << SENSOR_TOPIC << serial << "/" << suffix;
    return topic.str();
}

std::string getDeviceName(const std::string& manufacturer, uint32_t serial) {
    std::ostringstream topic;
    topic << manufacturer << "-" << serial;
    return topic.str();
}

std::string getSensorName(const std::string& manufacturer, uint32_t serial, const std::string& suffix) {
    std::ostringstream topic;
    topic << manufacturer << "-" << serial << "-" << suffix;
    return topic.str();
}

void DigitalDecoder::setRxGood(bool state)
{
    std::string topic(BASE_TOPIC);
    timeval now;

    topic += "rx_status";

    gettimeofday(&now, nullptr);

    if (rxGood != state || (now.tv_sec - lastRxGoodUpdateTime) > RX_GOOD_MIN_SEC)
    {
        mqtt.send(topic.c_str(), state ? "OK" : "FAILED");
    }

    // Reset watchdog either way
    alarm(RX_TIMEOUT_MIN*60);

    rxGood = state;
    lastRxGoodUpdateTime = now.tv_sec;
}

void DigitalDecoder::updateKeyfobState(uint32_t serial, uint64_t payload)
{
    if (payload == lastKeyfobPayload)
    {
        return;
    }

    std::ostringstream topic;
    topic << KEYFOB_TOPIC << serial << "/keypress";
    char c = ((payload & 0x000000F00000) >> 20);
    std::string key;
    if (c == 0x1)
    {
        key = "AWAY";
    }
    else if (c == 0x2)
    {
        key = "DISARM";
    }
    else if (c == 0x4)
    {
        key = "STAY";
    }
    else if (c == 0x8)
    {
        key = "AUX";
    }
    else
    {
        key = "UNK";
    }
    mqtt.send(topic.str().c_str(), key.c_str(), 1, false);

    lastKeyfobPayload = payload;
}

void DigitalDecoder::updateKeypadState(uint32_t serial, uint64_t payload)
{
    timeval now;
    gettimeofday(&now, nullptr);

    struct keypadState_t lastState;
    struct keypadState_t currentState;

    currentState.lastUpdateTime = now.tv_sec;
    currentState.hasLostSupervision = false;

    currentState.sequence = (payload & 0xF00000000000) >> 44;
    currentState.lowBat = payload & 0x000000020000;
    
    bool supervised = payload & 0x000000040000;
    if (supervised) return;

    auto found = keypadStatusMap.find(serial);
    if (found == keypadStatusMap.end())
    {
        lastState.sequence = 0xff;
        lastState.lowBat = !currentState.lowBat;
    }
    else
    {
        lastState = found->second;
    }

    if (currentState.sequence != lastState.sequence)
    {
        std::ostringstream topic;
        topic << KEYPAD_TOPIC << serial << "/keypress";
        char c = ((payload & 0x000000F00000) >> 20);
        
        std::string key;
        if (c == 0xA)
        {
            key = "*";
        }
        else if (c == 0xB)
        {
            key = "0";
        }
        else if (c == 0xC)
        {
            key = "#";
        }
        else if (c == 0xD)
        {
            key = "STAY";
        }
        else if (c == 0xE)
        {
            key = "AWAY";
        }
        else if (c == 0xF)
        {
            key = "FIRE";
        }
        else if (c == 0x0)
        {
            key = "POLICE";
        }
        else
        {
            key = (c + '0');
        }
        mqtt.send(topic.str().c_str(), key.c_str(), 1, false);
        
        if ((c >= 1 && c <= 0xC) && (currentState.lastUpdateTime <= (lastState.lastUpdateTime + UPDATE_INTERVAL)) && (lastState.phrase.length() < 10))
        {
            currentState.phrase = lastState.phrase + key;
            
            std::ostringstream phraseTopic;
            phraseTopic << KEYPAD_TOPIC << serial << "/keyphrase/" << currentState.phrase.length();
            mqtt.send(phraseTopic.str().c_str(), currentState.phrase.c_str(), 1, false);
        }
        else if (c == 0xB || (c >= 1 && c <= 9))
        {
            currentState.phrase = key;
        }
        
        keypadStatusMap[serial] = currentState;
    }
}

void DigitalDecoder::sendSensorDiscovery(const uint32_t serial, const std::string& manufacturer, const std::string& model,  const std::string& nameSuffix, const std::string& suffix, 
                                         const std::string& device_class, const std::string& payload_on, const std::string& payload_off, bool force_update, int expire_after) {
    timeval now;
    gettimeofday(&now, nullptr);
    uint64_t currentTime = now.tv_sec;

    std::string serialStr = std::to_string(serial);

    std::string topic = getDiscoveryTopic(serial, suffix);

    auto found = autoDiscoveryTime.find(topic);
    if (found == autoDiscoveryTime.end() || currentTime - found->second >= AUTODISCOVERY_INTERVAL) {
        std::ostringstream message;
        message << "{";
        addKeyValue(message, "name", manufacturer + "-" + model + "-" +  serialStr + "-" + nameSuffix);
        addKeyValue(message, "state_topic", getSensorTopic(serial, suffix));
        addKeyValue(message, "device_class", device_class);
        if (force_update) {
            addKeyValue(message, "force_update", "true");
        }
        if (expire_after) {
            addKeyValue(message, "expire_after", expire_after);
        }
        addKeyValue(message, "unique_id", BASE_NAME + "-" + serialStr  + "-" + suffix);
        if (payload_on.size()) {
            addKeyValue(message, "payload_on", payload_on);
        }
        if (payload_off.size()) {
            addKeyValue(message, "payload_off", payload_off);
        }
        message << "\"device\": {" ;
        addKeyValue(message, "identifiers", BASE_NAME + "-" + serialStr);
        addKeyValue(message, "name", manufacturer + "-" + model + "-" + serialStr);
        addKeyValue(message, "model", model);
        addKeyValue(message, "manufacturer", manufacturer, false);
        message << "}";
        message << "}";

        mqtt.send(topic.c_str(), message.str().c_str());
        autoDiscoveryTime[topic] = currentTime;
    }
}

void DigitalDecoder::sendSensorsDiscovery(uint32_t serial, uint32_t manufacturer_code, uint64_t typ)
{
    std::string manufacturer = BASE_NAME;
    std::string model = BASE_NAME;
    if (manufacturer_code == MANUFACTURER_CODE_HONEYWELL) {
        manufacturer = "Honeywell";
        if (typ == 132) {
            model = "Opening Sensor";
            sendSensorDiscovery(serial, manufacturer, model, "opening", LOOP2_NAME, "opening", OPEN_SENSOR_MSG, CLOSED_SENSOR_MSG);
        }
        else if (typ == 12 || typ == 4) {
            model = "Glass Break Sensor";
            sendSensorDiscovery(serial, manufacturer, model, "opening", LOOP1_NAME, "opening", OPEN_SENSOR_MSG, CLOSED_SENSOR_MSG);
        } else {
            sendSensorDiscovery(serial, manufacturer, model, LOOP1_NAME, LOOP1_NAME, "opening", OPEN_SENSOR_MSG, CLOSED_SENSOR_MSG);
            sendSensorDiscovery(serial, manufacturer, model, LOOP2_NAME, LOOP2_NAME, "opening", OPEN_SENSOR_MSG, CLOSED_SENSOR_MSG);
            sendSensorDiscovery(serial, manufacturer, model, LOOP3_NAME, LOOP3_NAME, "opening", OPEN_SENSOR_MSG, CLOSED_SENSOR_MSG); 
        }
        
        
    } else {
        manufacturer = BASE_NAME;
        sendSensorDiscovery(serial, manufacturer, model, LOOP1_NAME, LOOP1_NAME, "opening", OPEN_SENSOR_MSG, CLOSED_SENSOR_MSG);
        sendSensorDiscovery(serial, manufacturer, model, LOOP2_NAME, LOOP2_NAME, "opening", OPEN_SENSOR_MSG, CLOSED_SENSOR_MSG);
        sendSensorDiscovery(serial, manufacturer, model, LOOP3_NAME, LOOP3_NAME, "opening", OPEN_SENSOR_MSG, CLOSED_SENSOR_MSG); 
    }

    sendSensorDiscovery(serial, manufacturer, model, TAMPER_NAME, TAMPER_NAME, "safety", TAMPER_MSG, UNTAMPERED_MSG, true);
    
    sendSensorDiscovery(serial, manufacturer, model, BATTERY_NAME, BATTERY_NAME, "battery", LOW_BAT_MSG, OK_BAT_MSG);
    
    sendSensorDiscovery(serial, manufacturer, model, HB_NAME, HB_NAME, "connectivity", "PING", "", true, SENSOR_EXPIRATION);
}

void DigitalDecoder::updateSensorState(uint32_t serial, uint32_t manufacturer_code, uint64_t typ, uint64_t payload)
{
    if (this->sendDiscovery) {
        this->sendSensorsDiscovery(serial, manufacturer_code, typ);
    }

    timeval now;
    gettimeofday(&now, nullptr);

    struct sensorState_t lastState;
    struct sensorState_t currentState;

    currentState.lastUpdateTime = now.tv_sec;
    currentState.hasLostSupervision = false;

    currentState.loop1 = payload  & 0x000000800000;
    currentState.loop2 = payload  & 0x000000200000;
    currentState.loop3 = payload  & 0x000000100000;
    currentState.tamper = payload & 0x000000400000;
    currentState.lowBat = payload & 0x000000080000;

    // bool supervised = payload & 0x000000040000;
    // bool repeated = payload & 0x000000020000;

    //std::cout << "Payload:" << std::hex << payload << " Serial:" << std::dec << serial << std::boolalpha << " Loop1:" << currentState.loop1 << std::endl;

    auto found = sensorStatusMap.find(serial);
    if (found == sensorStatusMap.end())
    {
        // if there wasn't a state, make up a state that is opposite to our current state
        // so that we send everything.

        lastState.hasLostSupervision = !currentState.hasLostSupervision;
        lastState.loop1 = !currentState.loop1;
        lastState.loop2 = !currentState.loop2;
        lastState.loop3 = !currentState.loop3;
        lastState.tamper = !currentState.tamper;
        lastState.lowBat = !currentState.lowBat;
        lastState.lastUpdateTime = 0;
    }
    else
    {
        lastState = found->second;
    }
    
    // Since the sensor will frequently blast out the same signal many times, we only want to treat
    // the first detected signal as the supervisory signal. 
    bool supervised = (payload & 0x000000040000) && ((currentState.lastUpdateTime - lastState.lastUpdateTime) > UPDATE_INTERVAL);

    if ((currentState.loop1 != lastState.loop1) || supervised)
    {
        std::string topic = getSensorTopic(serial, LOOP1_NAME);
        mqtt.send(topic.c_str(), currentState.loop1 ? OPEN_SENSOR_MSG : CLOSED_SENSOR_MSG, supervised ? 0 : 1);
    }

    if ((currentState.loop2 != lastState.loop2) || supervised)
    {
        std::string topic = getSensorTopic(serial, LOOP2_NAME);
        mqtt.send(topic.c_str(), currentState.loop2 ? OPEN_SENSOR_MSG : CLOSED_SENSOR_MSG, supervised ? 0 : 1);
    }

    if ((currentState.loop3 != lastState.loop3) || supervised)
    {
        std::string topic = getSensorTopic(serial, LOOP3_NAME);
        mqtt.send(topic.c_str(), currentState.loop3 ? OPEN_SENSOR_MSG : CLOSED_SENSOR_MSG, supervised ? 0 : 1);
    }

    if ((currentState.tamper != lastState.tamper) || supervised)
    {
        std::string topic = getSensorTopic(serial, TAMPER_NAME);
        mqtt.send(topic.c_str(), currentState.tamper ? TAMPER_MSG : UNTAMPERED_MSG, supervised ? 0 : 1);
    }

    if ((currentState.lowBat != lastState.lowBat) || supervised)
    {
        std::string topic = getSensorTopic(serial, BATTERY_NAME);
        mqtt.send(topic.c_str(), currentState.lowBat ? LOW_BAT_MSG : OK_BAT_MSG, supervised ? 0 : 1);
    }

    if (currentState.lastUpdateTime - lastState.lastUpdateTime > UPDATE_INTERVAL) {
        std::string topic = getSensorTopic(serial, HB_NAME);
        mqtt.send(topic.c_str(), "PING", supervised ? 0 : 1);
    }

    sensorStatusMap[serial] = currentState;
}

// /* Checks all devices for last time updated */
// void DigitalDecoder::checkForTimeouts()
// {
//     timeval now;
//     std::ostringstream status;

//     status << "TIMEOUT";
//     gettimeofday(&now, nullptr);

//     for(const auto &dd : sensorStatusMap)
//     {
//         if ((now.tv_sec - dd.second.lastUpdateTime) > SENSOR_TIMEOUT_MIN*60)
//         {
//             if (false == dd.second.hasLostSupervision)
//             {
//                 std::ostringstream statusTopic;

//                 sensorStatusMap[dd.first].hasLostSupervision = true;
//                 statusTopic << BASE_TOPIC << dd.first << "/status";
//                 mqtt.send(statusTopic.str().c_str(), status.str().c_str());
//             }
//         }
//     }
// }

bool DigitalDecoder::isPayloadValid(uint64_t payload, uint64_t polynomial) const 
{
    uint32_t manufacturer_code = 0;
    return isPayloadValid(payload, polynomial, manufacturer_code);
}

bool DigitalDecoder::isPayloadValid(uint64_t payload, uint64_t polynomial, uint32_t& manufacturer_code) const
{
    uint64_t sof = (payload & 0xF00000000000) >> 44;
    uint64_t ser = (payload & 0x0FFFFF000000) >> 24;
    // uint64_t typ = (payload & 0x000000FF0000) >> 16;
    uint64_t crc = (payload & 0x00000000FFFF) >>  0;

    //
    // Check CRC
    //
    manufacturer_code = MANUFACTURER_CODE_UNKNOWN;
    if (polynomial == 0)
    {
        if (sof == 0x2 /* 2gig smoke */ 
            || sof == 0x3 /* 2gig panic */ 
            || sof == 0x4 /* 2gig PIR */
            || sof == 0x7 /* 2gig flood/temp */ 
            || sof == 0x9 /* 2gig glass break */
            || sof == 0xA /* 2gig door window */ 
            || sof == 0xB /* 2gig carbon monoxide */
            || sof == 0xC /* 2gig Tilt */ 
            || sof == 0xF /* Remote keyfob */) {
            // 2GIG brand
            manufacturer_code = MANUFACTURER_CODE_2GIG;
            #ifdef __arm__
            printf("2GIG Sensor %llu/0x%llX", sof, sof);
            #else
            printf("2GIG Sensor %lu/0x%lX", sof, sof);
            #endif
            polynomial = 0x18050;
        } else if (sof == 0x8) {
            // Honeywell Sensor
            manufacturer_code = MANUFACTURER_CODE_HONEYWELL;
            printf("Honeywell Sensor");
            polynomial = 0x18005;
        } else if (sof == 0xD || sof == 0xE) {
            // Vivint
            manufacturer_code = MANUFACTURER_CODE_VIVINT;
            #ifdef __arm__
            printf("Vivint Sensor %llu/0x%llX", sof, sof);
            #else
            printf("Vivint Sensor %lu/0x%lX", sof, sof);
            #endif
            polynomial = 0x18050; // Don't know if this is correct
        } else {
            // Something else?
            #ifdef __arm__
            printf("Unknown Brans Sensor %llu/0x%llX", sof, sof);
            #else
            printf("Unknown Brand Sensor %lu/0x%lX", sof, sof);
            #endif
            polynomial = 0x18050;
        }
        printf(" - ");
    }
    uint64_t sum = payload & (~SYNC_MASK);
    uint64_t current_divisor = polynomial << 31;

    while(current_divisor >= polynomial)
    {
        #ifdef __arm__
        if(__builtin_clzll(sum) == __builtin_clzll(current_divisor))
        #else
        if(__builtin_clzl(sum) == __builtin_clzl(current_divisor))
        #endif
        {
            sum ^= current_divisor;
        }
        current_divisor >>= 1;
    }

    return (sum == 0);
}

void DigitalDecoder::handlePayload(uint64_t payload)
{
    uint64_t ser = (payload & 0x0FFFFF000000) >> 24;
    uint64_t typ = (payload & 0x000000FF0000) >> 16; 

    uint32_t manufacturer_code = MANUFACTURER_CODE_UNKNOWN;
    const bool validSensorPacket = isPayloadValid(payload, 0, manufacturer_code);
    const bool validKeypadPacket = isPayloadValid(payload, 0x18050) && (typ & 0x01);
    const bool validKeyfobPacket = isPayloadValid(payload, 0x18050) && (typ & 0x02);

    //
    // Print Packet
    //
 #ifdef __arm__
    printf("%s Payload: %llX (Serial %llu/%llX, Status %llX)\n", (validSensorPacket | validKeypadPacket | validKeyfobPacket) ? "Valid" : "Invalid", payload, ser, ser, typ);
 #else
    printf("%s Payload: %lX (Serial %lu/%lX, Status %lX)\n", (validSensorPacket | validKeypadPacket | validKeyfobPacket) ? "Valid" : "Invalid", payload, ser, ser, typ);
 #endif

    packetCount++;
    if(!validSensorPacket && !validKeypadPacket && !validKeyfobPacket)
    {
        errorCount++;
        printf("%u/%u packets failed CRC", errorCount, packetCount);
        std::cout << std::endl;
    }

    //
    // Tell the world
    //
    if(validSensorPacket && !validKeypadPacket && !validKeyfobPacket && keypadStatusMap.find(ser) == keypadStatusMap.end())
    {
        printf("Sensor Packet\n");
        // We received a valid packet so the receiver must be working
        setRxGood(true);
        // Update the device
        updateSensorState(ser, manufacturer_code, typ, payload);
    }
    else if (validKeypadPacket)
    {
        printf("Keypad Packet\n");
        setRxGood(true);
        updateKeypadState(ser, payload);
    }
    else if (validKeyfobPacket)
    {
        printf("Keyfob Packet\n");
        setRxGood(true);
        updateKeyfobState(ser, payload);
    }
}



void DigitalDecoder::handleBit(bool value)
{
    static uint64_t payload = 0;

    payload <<= 1;
    payload |= (value ? 1 : 0);

//#ifdef __arm__
//    printf("Got bit: %d, payload is now %llX\n", value?1:0, payload);
//#else
//    printf("Got bit: %d, payload is now %lX\n", value?1:0, payload);
//#endif

    if((payload & SYNC_MASK) == SYNC_PATTERN)
    {
        handlePayload(payload);
        payload = 0;
    }
}

void DigitalDecoder::decodeBit(bool value)
{
    enum ManchesterState
    {
        LOW_PHASE_A,
        LOW_PHASE_B,
        HIGH_PHASE_A,
        HIGH_PHASE_B
    };

    static ManchesterState state = LOW_PHASE_A;

    switch(state)
    {
        case LOW_PHASE_A:
        {
            state = value ? HIGH_PHASE_B : LOW_PHASE_A;
            break;
        }
        case LOW_PHASE_B:
        {
            handleBit(false);
            state = value ? HIGH_PHASE_A : LOW_PHASE_A;
            break;
        }
        case HIGH_PHASE_A:
        {
            state = value ? HIGH_PHASE_A : LOW_PHASE_B;
            break;
        }
        case HIGH_PHASE_B:
        {
            handleBit(true);
            state = value ? HIGH_PHASE_A : LOW_PHASE_A;
            break;
        }
    }
}

void DigitalDecoder::handleData(char data)
{
    static const int samplesPerBit = 8;


    if(data != 0 && data != 1) return;

    const bool thisSample = (data == 1);

    if(thisSample == lastSample)
    {
        samplesSinceEdge++;

        //if(samplesSinceEdge < 100)
        //{
        //    printf("At %d for %u\n", thisSample?1:0, samplesSinceEdge);
        //}

        if((samplesSinceEdge % samplesPerBit) == (samplesPerBit/2))
        {
            // This Sample is a new bit
            decodeBit(thisSample);
        }
    }
    else
    {
        samplesSinceEdge = 1;
    }
    lastSample = thisSample;
}
