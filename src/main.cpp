#include "digitalDecoder.h"
#include "analogDecoder.h"
#include "mqtt.h"
#include "mqtt_config.h"

#include <rtl-sdr.h>

#include <iostream>
#include <cmath>
#include <csignal>
#include <unistd.h>
#include <sys/time.h>
#include <cstdlib>
#include <string>
#include <sstream>

// TODO: MQTT Will doesn't seem to be working with HA as expected

float magLut[0x10000];

// void alarmHandler(int signal)
// {
//     dDecoder.setRxGood(false);
// }

void usage(const char *argv0)
{
    std::cout << "Usage: " << std::endl
        << argv0 << " [-d <device-id>] [-f <frequency in Hz]" << std::endl;
}

int main(int argc, char ** argv)
{
    const char *mqttHost = std::getenv("MQTT_HOST");
    if ((mqttHost == NULL) || (std::char_traits<char>::length(mqttHost) == 0))
    {
        mqttHost = MQTT_HOST;
    }
    const char *mqttPortStr = std::getenv("MQTT_PORT");
    int mqttPort = MQTT_PORT;
    if ((mqttPortStr) && (std::char_traits<char>::length(mqttPortStr) > 0))
    {
        mqttPort = std::stoi(mqttPortStr);
    }
    const char *mqttUsername = std::getenv("MQTT_USERNAME");
    if ((mqttUsername == NULL) || (std::char_traits<char>::length(mqttUsername) == 0))
    {
        mqttUsername = MQTT_USERNAME;
    }
    const char *mqttPassword = std::getenv("MQTT_PASSWORD");
    if ((mqttPassword == NULL) || (std::char_traits<char>::length(mqttPassword) == 0))
    {
        mqttPassword = MQTT_PASSWORD;
    }

    // bool sendDiscovery = true;
    // const char *_sendDiscovery = std::getenv("DISCOVERY");
    // if ((_sendDiscovery == NULL) || (std::char_traits<char>::length(_sendDiscovery) == 0))
    // {
    //     sendDiscovery = false;
    // }


    Mqtt mqtt = Mqtt("sensors345", mqttHost, mqttPort, mqttUsername, mqttPassword, "security/sensors345/rx_status", "FAILED");

    
    int devId = 0;
    int freq = 345000000;
    signed char c;
    bool sendDiscovery = false;
    while ((c = getopt(argc, argv, "hd:f:a:")) != -1)
    {
        switch(c)
        {
            case 'h':
            {
                usage(argv[0]);
                exit(0);
            }
            case 'd':
            {
                devId = atoi(optarg);
                break;
            }
            case 'f':
            {
                freq = atoi(optarg);
                break;
            }
            case 'a':
            {
                std::istringstream(optarg) >> sendDiscovery;
                break;
            }
            default: // including '?' unknown character
            {
                std::cerr << "Unknown flag '" << c << std::endl;
                usage(argv[0]);
                exit(1);
            }
        }
    }
    
    std::cout << "Discovery messages are " << (sendDiscovery ? "enabled" : "disabled") << std::endl;
    DigitalDecoder dDecoder = DigitalDecoder(mqtt, sendDiscovery);
    AnalogDecoder aDecoder;

    //
    // Open the device
    //
    if(rtlsdr_get_device_count() < 1)
    {
        std::cout << "Could not find any devices" << std::endl;
        return -1;
    }
        
    rtlsdr_dev_t *dev = nullptr;
        
    if(rtlsdr_open(&dev, devId) < 0)
    {
        std::cout << "Failed to open device" << std::endl;
        return -1;
    }
    
    //
    // Set the frequency
    //
    if(rtlsdr_set_center_freq(dev, freq) < 0)
    {
        std::cout << "Failed to set frequency" << std::endl;
        return -1;
    }
    
    std::cout << "Successfully set the frequency to " << rtlsdr_get_center_freq(dev) << std::endl;
    
    //
    // Set the gain
    //
    if(rtlsdr_set_tuner_gain_mode(dev, 1) < 0)
    {
        std::cout << "Failed to set gain mode" << std::endl;
        return -1;
    }
    
    if(rtlsdr_set_tuner_gain(dev, 350) < 0)
    {
        std::cout << "Failed to set gain" << std::endl;
        return -1;
    }
    
    std::cout << "Successfully set gain to " << rtlsdr_get_tuner_gain(dev) << std::endl;
    
    //
    // Set the sample rate
    //
    if(rtlsdr_set_sample_rate(dev, 1000000) < 0)
    {
        std::cout << "Failed to set sample rate" << std::endl;
        return -1;
    }
    
    std::cout << "Successfully set the sample rate to " << rtlsdr_get_sample_rate(dev) << std::endl;
    
    //
    // Prepare for streaming
    //
    rtlsdr_reset_buffer(dev);
    
    for(uint32_t ii = 0; ii < 0x10000; ++ii)
    {
        uint8_t real_i = ii & 0xFF;
        uint8_t imag_i = ii >> 8;
        
        float real = (((float)real_i) - 127.4) * (1.0f/128.0f);
        float imag = (((float)imag_i) - 127.4) * (1.0f/128.0f);
        
        float mag = std::sqrt(real*real + imag*imag);
        magLut[ii] = mag;
    }
    
    //
    // Common Receive
    //
    
    aDecoder.setCallback([&](char data){dDecoder.handleData(data);});
    
    //
    // Async Receive
    //
    
    typedef void(*rtlsdr_read_async_cb_t)(unsigned char *buf, uint32_t len, void *ctx);
    
    auto cb = [](unsigned char *buf, uint32_t len, void *ctx)
    {
        AnalogDecoder *adec = (AnalogDecoder *)ctx;
        
        int n_samples = len/2;
        for(int i = 0; i < n_samples; ++i)
        {
            float mag = magLut[*((uint16_t*)(buf + i*2))];
            adec->handleMagnitude(mag);
        }
    };

    // Setup watchdog to check for a common-mode failure (e.g. antenna disconnection)
    //std::signal(SIGALRM, alarmHandler);
  
    // Initialize RX state to good
    dDecoder.setRxGood(true);
    const int err = rtlsdr_read_async(dev, cb, &aDecoder, 0, 0);
    std::cout << "Read Async returned " << err << std::endl;
   
/*    
    //
    // Synchronous Receive
    //
    static const size_t BUF_SIZE = 1024*256;
    uint8_t buffer[BUF_SIZE];
    
    while(true)
    {
        int n_read = 0;
        if(rtlsdr_read_sync(dev, buffer, BUF_SIZE, &n_read) < 0)
        {
            std::cout << "Failed to read from device" << std::endl;
            return -1;
        }
        
        int n_samples = n_read/2;
        for(int i = 0; i < n_samples; ++i)
        {
            float mag = magLut[*((uint16_t*)(buffer + i*2))];
            aDecoder.handleMagnitude(mag);
        }
    }
*/    
    //
    // Shut down
    //
    rtlsdr_close(dev);
    return 0;
}




