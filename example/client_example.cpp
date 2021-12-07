/*
 * This example shows how to publish messages from outside of the Mosquitto network loop.
 */

#include "Client.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

const char* cafile = "ca.crt";
const char* cert   = "client.crt";
const char* key    = "client.key";

class Listener : public MQTT::Client::ClientListener
{
  public:
    MQTT::Client client;
    Listener() : client(this) {}
    bool Init() { return client.Init(cafile, cert, key) && client.Connect("127.0.0.1", 8883, 5); }

    void OnConnect(const bool& success, const std::string& reason) override
    {
        std::cout << "OnConnect: " << reason << std::endl;
        if (success)
        {
            static auto* t = new std::thread([this]() {
                for (int i = 0; i < 100; ++i)
                {
                    publish_sensor_data();
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            });
        }
    }

    void OnPublish(int mid) override { std::cout << "OnPublish: " << mid << std::endl; }
    void OnDisconnect(int mid) override { std::cout << "OnDisconnect: " << mid << std::endl; }
    void OnLog(const std::string& message) override { std::cout << "OnLog: " << std::endl << message << std::endl; }
    void OnMessage(const std::string& topic, const int& qos, const void* payload, const int& len) override
    {
        std::cout << "OnMessage: " << std::endl;
        std::cout << "Topic: " << topic << std::endl;
        std::cout << "QOS: " << qos << std::endl;
        std::string message((const char*)payload, len);
        std::cout << "Message: " << message << std::endl;
    }
    void OnSubscribe(const int& mid, const bool& have_subscription) override
    {
        if (have_subscription)
            std::cout << "OnSubscribe: success: " << map[mid] << std::endl;
        else
            std::cout << "OnSubscribe: failed : " << map[mid] << std::endl;
    }
    void OnUnsubscribe(int mid) override { std::cout << "OnUnsubscribe: " << mid << std::endl; }
    void OnError(const std::string& error) override { std::cout << "OnError: " << error << std::endl; }

    int get_temperature(void) { return random() % 100; }

    /* This function pretends to read some data from a sensor and publish it.*/
    void publish_sensor_data()
    {
        static int message = 0;
        char       payload[20];
        int        temp;

        /* Get our pretend data */
        temp = get_temperature();
        /* Print it to a string for easy human reading - payload format is highly
         * application dependent. */
        snprintf(payload, sizeof(payload), "%d - %d", message++, temp);

        int mid;
        client.Publish(payload, strlen(payload), mid);
        std::cout << "Sending mid: " << mid << std::endl;
    }

    std::unordered_map<int, std::string> map;

    void Sub(const std::string& topic)
    {
        int mid;
        if (client.Subscribe(topic, 2, mid))
        {
            map[mid] = topic;
            std::cout << "Subscribe returned true." << std::endl;
        }
        else
            std::cout << "Subscribe returned false." << std::endl;
    }
};

int main(int argc, char* argv[])
{
    Listener listener;
    if (!listener.Init()) return 1;

    int  topic = 0;
    char c;
    do
    {
        c = (char) getchar();
        if (c == 's')
            listener.Sub("topic-" + std::to_string(topic++));
        else
        {
            int mid;
            listener.publish_sensor_data();
        }

    } while (c != 'x');

    return 0;
}
