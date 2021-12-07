#include "Client.h"

#include <iostream>
#include <mosquitto.h>

bool MQTT::Client::Init(const char* cafile_path,
                        const char* cert_path,
                        const char* key_path,
                        const bool& debug /*= false*/)
{
    std::lock_guard<std::mutex> lk(m_mosq_lk);
    if (!m_mosq)
    {
        m_connected = false;
        m_debug     = debug;
        int rc;

        /* Required before calling other mosquitto functions */
        rc = mosquitto_lib_init();
        if (rc != MOSQ_ERR_SUCCESS) { return false; }

        m_mosq = mosquitto_new(NULL, true, this);
        if (m_mosq == NULL)
        {
            m_client_listener->OnError("MQTT::Client::Init: Out of memory");
            return false;
        }

        rc = mosquitto_tls_set(m_mosq, cafile_path, NULL, cert_path, key_path, NULL);
        if (rc != MOSQ_ERR_SUCCESS)
        {
            m_client_listener->OnError("MQTT::Client::Init: TLS init failed");
            return false;
        }

        /* Configure callbacks. This should be done before connecting ideally. */
        mosquitto_connect_callback_set(m_mosq, on_connect);
        mosquitto_publish_callback_set(m_mosq, on_publish);
        mosquitto_disconnect_callback_set(m_mosq, on_disconnect);
        mosquitto_log_callback_set(m_mosq, on_log);
        mosquitto_message_callback_set(m_mosq, on_message);
        mosquitto_subscribe_callback_set(m_mosq, on_subscribe);
        mosquitto_unsubscribe_callback_set(m_mosq, on_unsubscribe);

        return true;
    }
    return false;
}

bool MQTT::Client::Connect(const char* host, const int& port, const int& timeout)
{
    int rc;
    /* Connect to test.mosquitto.org on port 1883, with a keepalive of 60 seconds.
     * This call makes the socket connection only, it does not complete the MQTT
     * CONNECT/CONNACK flow, you should use mosquitto_loop_start() or
     * mosquitto_loop_forever() for processing net traffic. */
    std::lock_guard<std::mutex> lk(m_mosq_lk);
    if (m_mosq)
    {
        rc = mosquitto_connect(m_mosq, host, port, timeout);
        if (rc != MOSQ_ERR_SUCCESS)
        {
            mosquitto_destroy(m_mosq);
            m_mosq      = nullptr;
            m_connected = false;
            m_client_listener->OnError(mosquitto_strerror(rc));
            return false;
        }

        /* Run the network loop in a background thread, this call returns quickly. */
        rc = mosquitto_loop_start(m_mosq);
        if (rc != MOSQ_ERR_SUCCESS)
        {
            mosquitto_destroy(m_mosq);
            m_mosq      = nullptr;
            m_connected = false;
            m_client_listener->OnError(mosquitto_strerror(rc));
            return false;
        }
        return true;
    }
    return false;
}

bool MQTT::Client::Publish(const void* payload, const int& payload_len, int& message_id) const
{
    /* Publish the message
     * mosq - our client instance
     * *mid = NULL - we don't want to know what the message id for this message is
     * topic = "example/temperature" - the topic on which this message will be published
     * payloadlen = strlen(payload) - the length of our payload in bytes
     * payload - the actual payload
     * qos = 2 - publish with QoS 2 for this example
     * retain = false - do not use the retained message feature for this message
     */
    std::lock_guard<std::mutex> lk(m_mosq_lk);
    if (m_mosq)
    {
        int rc = mosquitto_publish(m_mosq, &message_id, "/world", payload_len, payload, 2, false);
        if (rc != MOSQ_ERR_SUCCESS)
        {
            m_client_listener->OnError(mosquitto_strerror(rc));
            return false;
        }
        return true;
    }
    return false;
}

bool MQTT::Client::Subscribe(const std::string& topic, int qos, int& message_id)
{
    if (m_connected && m_mosq)
    {
        int rc = mosquitto_subscribe(m_mosq, &message_id, topic.c_str(), qos);
        if (rc != MOSQ_ERR_SUCCESS)
        {
            m_client_listener->OnError(mosquitto_strerror(rc));
            return false;
        }
        return true;
    }
    return false;
}

bool MQTT::Client::IsConnected() const { return m_connected; }

void MQTT::Client::SetDebug(const bool& debug) { m_debug = debug; }

MQTT::Client::Client(MQTT::Client::ClientListener* m_client_listener)
    : m_client_listener(m_client_listener), m_mosq(nullptr), m_connected(false), m_debug(false)
{}

MQTT::Client::~Client()
{
    // Has a reference count! Can be called as many times as you wish
    std::lock_guard<std::mutex> lk(m_mosq_lk);
    mosquitto_disconnect(m_mosq);
    mosquitto_loop_stop(m_mosq, false);
    mosquitto_destroy(m_mosq);
    mosquitto_lib_cleanup();
}

/* Callback called when the client receives a CONNACK message from the broker. */
void MQTT::Client::on_connect(struct mosquitto* mosq, void* obj, int reason_code)
{
    auto client = (MQTT::Client*)obj;
    /* Print out the connection result. mosquitto_connack_string() produces an
     * appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
     * clients is mosquitto_reason_string().
     */

    if (reason_code != 0)
    {
        /* If the connection fails for any reason, we don't want to keep on
         * retrying in this example, so disconnect. Without this, the client
         * will attempt to reconnect. */
        // mosquitto_disconnect(mosq);
        client->m_connected = false;
    }

    client->m_connected = true;
    client->m_client_listener->OnConnect(client->m_connected, mosquitto_connack_string(reason_code));
}

/* Callback called when the client knows to the best of its abilities that a
 * PUBLISH has been successfully sent. For QoS 0 this means the message has
 * been completely written to the operating system. For QoS 1 this means we
 * have received a PUBACK from the broker. For QoS 2 this means we have
 * received a PUBCOMP from the broker. */
void MQTT::Client::on_publish(struct mosquitto* mosq, void* obj, int mid)
{
    auto client = (MQTT::Client*)obj;
    client->m_client_listener->OnPublish(mid);
}

void MQTT::Client::on_disconnect(struct mosquitto* mosq, void* obj, int mid)
{
    auto client         = (MQTT::Client*)obj;
    client->m_connected = false;
    client->m_client_listener->OnDisconnect(mid);
}

void MQTT::Client::on_log(struct mosquitto* mosq, void* obj, int, const char* message)
{
    auto client = (MQTT::Client*)obj;
    if (client->m_debug) client->m_client_listener->OnLog(message);
}

void MQTT::Client::on_message(mosquitto*, void* obj, const mosquitto_message* msg)
{
    auto client = (MQTT::Client*)obj;
    client->m_client_listener->OnMessage(msg->topic, msg->qos, msg->payload, msg->payloadlen);
}

void MQTT::Client::on_subscribe(struct mosquitto*, void* obj, int mid, int qos_count, const int* granted_qos)
{
    auto client = (MQTT::Client*)obj;

    int  i;
    bool have_subscription = false;

    /* In this example we only subscribe to a single topic at once, but a
     * SUBSCRIBE can contain many topics at once, so this is one way to check
     * them all. */
    for (i = 0; i < qos_count; i++)
    {
        if (granted_qos[i] <= 2) { have_subscription = true; }
    }

    client->m_client_listener->OnSubscribe(mid, have_subscription);
}

void MQTT::Client::on_unsubscribe(struct mosquitto* mosq, void* obj, int mid) {
    auto client = (MQTT::Client*)obj;
    client->m_client_listener->OnUnsubscribe(mid);
}
