/**
 * @file ESP8266.cpp
 * @author Hugo Arganda
 * @date 4 Aug 2016
 * @brief Arduino ESP8266 library.
 */

#include "ESP8266.h"
#include "ESP8266_AT_CMD.h"

/* Macro for debug ESP8266 Responses */
#if (ESP8266_DBG_PARSE_EN == 1)
#define ESP8266_DBG_PARSE(label, data)\
    Serial.print(F("[ESP8266] "));\
    Serial.print(label);\
    Serial.println(data);
#else
#define ESP8266_DBG_PARSE(label, data)
#endif

#if (ESP8266_DBG_HTTP_RES == 1)
#define ESP8266_DBG_HTTP(label, data) ESP8266_DBG_PARSE(label, data)
#else
#define ESP8266_DBG_HTTP(label, data)
#endif

ESP8266::ESP8266(int rst, int en)
{
    /* Save hardware configurations */
    _resetPin = rst;
    _enablePin = en;
}

#if (ESP8266_USE_HW_SW_PORT == 1)
ESP8266::ESP8266(HardwareSerial &serialPort, uint32_t baud, int rst, int en)
{
    /* Save hardware configurations */
    _resetPin = rst;
    _enablePin = en;
    /* Save Serial Port configurations */
    _serialPortHandler.isSoftSerial = false;
    _serialPortHandler._soft = NULL;
    _serialPortHandler._hard = &serialPort;
    /* Setup device */
    setup(baud);
}
#endif

void ESP8266::begin(SoftwareSerial &serialPort, uint32_t baud)
{
    /* Save Serial Port configurations */
#if (ESP8266_USE_HW_SW_PORT == 1)
    _serialPortHandler.isSoftSerial = true;
    _serialPortHandler._soft = &serialPort;
    _serialPortHandler._hard = NULL;
#else
    _serialPort = &serialPort;
    setup(baud);
#endif
}

void ESP8266::setup(uint32_t baud)
{
#if (ESP8266_USE_HW_SW_PORT == 1)
    /* Begin Serial Port */
    if(_serialPortHandler.isSoftSerial)
    {
        if(NULL != _serialPortHandler._soft)
        {
            _serialPortHandler._soft->begin(baud);
        }
    }
    else
    {
        if(NULL != _serialPortHandler._hard)
        {
            _serialPortHandler._hard->begin(baud);
        }
    }
#else
    _serialPort->begin(baud);
#endif
    /* Setup pins */
    pinMode(_resetPin, OUTPUT);
    pinMode(_enablePin, OUTPUT);
    digitalWrite(_resetPin, HIGH);
    digitalWrite(_enablePin, HIGH);
    delay(100);
}

bool ESP8266::hardReset()
{
    digitalWrite(_resetPin, LOW);
    delay(1000);
    digitalWrite(_resetPin, HIGH);
    return (getResponse(NULL, AT_RESPONSE_RST, NULL, NULL, NULL, 1000) > 0);
}

bool ESP8266::test()
{
    sendCommand(AT_TEST, ESP8266_CMD_EXECUTE, NULL);
    return (getResponse(NULL, AT_RESPONSE_OK, NULL, NULL, NULL, 1000) > 0);
}

bool ESP8266::reset()
{
    _serialPort->flush();
    sendCommand(AT_RESET, ESP8266_CMD_EXECUTE, NULL);
    return (getResponse(NULL, AT_RESPONSE_RST, NULL, NULL, NULL, 3000) > 0);
}

bool ESP8266::echo(bool enable)
{
    if (enable)
    {
        sendCommand(AT_ECHO_ENABLE, ESP8266_CMD_EXECUTE, NULL);
    }
    else
    {
        sendCommand(AT_ECHO_DISABLE, ESP8266_CMD_EXECUTE, NULL);
    }
    return (getResponse(NULL, AT_RESPONSE_OK, NULL, NULL, NULL, 3000) > 0);
}

bool ESP8266::operationMode(int mode)
{
    char modeStr[2];
    itoa(mode, modeStr, 10); /* Convert current int mode into ASCII (string) */
    sendCommand(AT_SET_WIFI_MODE, ESP8266_CMD_SETUP, modeStr);
    return (getResponse(NULL, AT_RESPONSE_OK, NULL, NULL, NULL, 1000) > 0);
}

bool ESP8266::connectionMode(int mode)
{
    bool ret = false;
    char modeStr[2];
    itoa(mode, modeStr, 10); /* Convert current int mode into ASCII (string) */
    sendCommand(AT_CIPMUX, ESP8266_CMD_SETUP, modeStr);
    return (getResponse(NULL, AT_RESPONSE_OK, NULL, NULL, NULL, 3000) > 0);
}

// Connect to Access Point
bool ESP8266::joinAP(char *ssid, char *ssid_pass)
{
    bool conn = false;

    if (NULL != ssid)
    {
        print(AT_CMD);
        print(AT_CWJAP);
        print("=\"");
        print(ssid);
        print("\"");
        if (NULL != ssid_pass)
        {
            print(",\"");
            print(ssid_pass);
            print("\"");
        }
        print("\r\n");

        conn = (getResponse(NULL, "WIFI CONNECTED", NULL, NULL, NULL, 4000) > 0);
        if (conn)
        {
            conn = (getResponse(NULL, AT_RESPONSE_OK, NULL, NULL, NULL, 5000) > 0);
        }

        if (!conn)
        {
            quitAP();
        }
    }
    return conn;
}

// Disconnect from Access Point
bool ESP8266::quitAP(void)
{
    bool ret = false;
    sendCommand(AT_CWQAP, ESP8266_CMD_EXECUTE, NULL);
    ret = (getResponse(NULL, AT_RESPONSE_OK, NULL, NULL, NULL, 3000) > 0);
    if (ret)
    {
        (void) getResponse(NULL, "WIFI DISCONNECT", NULL, NULL, NULL, 1000);
    }
    return ret;
}

bool ESP8266::version(char *dest)
{
    sendCommand(AT_GMR, ESP8266_CMD_EXECUTE, NULL);
    return (getResponse(dest, "AT version", NULL, ':', '(', 1000) > 0);
}

char* ESP8266::requestAPList(void)
{
    char* ssidName = NULL;
    sendCommand(AT_CWLAP, ESP8266_CMD_EXECUTE, NULL);
    if (getResponse(_ssidBuffer, AT_CWLAP_RX, NULL, '"', '"', 5000) > 0)
    {
        ssidName = (char*) &_ssidBuffer;
    }
    return ssidName;
}

char* ESP8266::getNextAP(void)
{
    char* ssidNext = NULL;
    if (getResponse(_ssidBuffer, AT_CWLAP_RX, NULL, '"', '"', 1000) > 0)
    {
        ssidNext = (char*) &_ssidBuffer;
    }
    return ssidNext;
}

bool ESP8266::ping(char *address)
{
    print("AT");
    print(AT_PING);
    print("=\"");
    print(address);
    print("\"\r\n");
    return (getResponse(NULL, AT_RESPONSE_OK, NULL, NULL, NULL, 5000) > 0);
}

bool ESP8266::startTCP(char *server, char *port)
{
    uint8_t ret = false;
    int8_t conn = 0;

    if ((NULL != server) && (NULL != port))
    {
        _serialPort->flush();

        /* Build command */
        print("AT");
        print(AT_CIPSTART);
        print("=\"TCP\",\"");
        print(server);
        print("\",");
        print(port);
        print("\r\n");

        conn = getResponse(NULL, AT_CIPSTART_RX, AT_CIPSTART_ALRDY, NULL, NULL, 3000);
        ret = ((conn == ESP8266_CMD_RSP_FAILED) || (conn > 0));

        /* If failed to connect, disconnect */
        if (!ret)
        {
            stopTCP();
        }
    }
    return ret;
}

bool ESP8266::stopTCP(void)
{
    int8_t conn = false;
    sendCommand(AT_CIPCLOSE, ESP8266_CMD_EXECUTE, NULL);
    conn = getResponse(NULL, AT_RESPONSE_OK, AT_RESPONSE_ERROR, NULL, NULL, 1000);
    return ((conn == ESP8266_CMD_RSP_FAILED) || (conn > 0));
}

bool ESP8266::localIP(char *ip)
{
    sendCommand(AT_CIFSR, ESP8266_CMD_EXECUTE, NULL);
    return (getResponse(ip, AT_CIFSR_STATIP, NULL, '"', '"', 1000) > 0);
}

bool ESP8266::localMAC(char *mac)
{
    sendCommand(AT_CIFSR, ESP8266_CMD_EXECUTE, NULL);
    return (getResponse(mac, AT_CIFSR_STAMAC, NULL, '"', '"', 1000) > 0);
}

/* TODO: Remove string usage */
bool ESP8266::send(String data)
{
    uint8_t ret = false;
    // uint16_t status = 404;

    data += "\r\n\r\n";

    sendCommand(AT_CIPSEND, ESP8266_CMD_SETUP, (char*) String(data.length()).c_str());
    ret = (getResponse(NULL, ">", NULL, NULL, NULL, 1000) > 0);

    if (ret)
    {
        /* Send data */
        print(data);
        ret = (getResponse(NULL, AT_CIPSEND_OK, NULL, NULL, NULL, 5000) > 0);
    }
    else
    {
        /* End connection if request failed */
        stopTCP();
    }

    return ret;
}

bool ESP8266::startSendTCP(int len)
{
    uint8_t ret = false;
  
    sendCommand(AT_CIPSEND, ESP8266_CMD_SETUP, (char*) String(len).c_str());
    return (getResponse(NULL, ">", NULL, NULL, NULL, 1000) > 0);
}

bool ESP8266::endSendTCP(void)
{
    return (getResponse(NULL, AT_CIPSEND_OK, NULL, NULL, NULL, 5000) > 0);
}

#if 0
uint16_t ESP8266::httpReceive(httpResponse* response)
{
    uint16_t rxLength = 0;
    int16_t remaining = 0;

    char *ucpStart = NULL;
    char *ucpEnd = NULL;

    uint16_t count = (uint16_t) getResponse(response->content, AT_IPD, NULL, ',', '\r', 1000);
    if (0 < count)
    {
        /* Get response length */
        ucpEnd = (char *) memchr(response->content, ':', count - (ucpStart - response->content));
        if (NULL != ucpEnd)
        {
            if ((ucpEnd - response->content) > 1)
            {
                *ucpEnd = '\0';
                rxLength = (uint16_t) atoi(response->content);
            }
        }

        /* Get response status */
        ucpStart = (char *) memchr(ucpEnd, ' ', count);
        if (NULL != ucpStart)
        {
            ucpStart++;
            ucpEnd = (char *) memchr(ucpStart, ' ', count - (ucpStart - response->content));
            if (NULL != ucpEnd)
            {
                if ((ucpEnd - ucpStart) > 1)
                {
                    *ucpEnd = '\0';
                    response->status = (uint16_t) atoi(ucpStart);
                }
            }
        }

        rxLength = (uint16_t) atoi(response->content);
        count -= (strlen(AT_IPD) + strlen(response->content) + 2);
        remaining = rxLength - count;

        ESP8266_DBG_HTTP("CNT: ", count);
        ESP8266_DBG_HTTP("RMN: ", remaining);

        /* TODO: Add timeout */
        while (0 < remaining)
        {
            if (remaining < ESP8266_RX_BUFF_LEN)
            {
                count = (uint16_t) _serialPortHandler->readBytesUntil('\n', response->content, remaining);
            }
            else
            {
                count = (uint16_t) _serialPortHandler->readBytesUntil('\n', response->content, ESP8266_RX_BUFF_LEN);
            }

            if (0 < count)
            {
                if (remaining != count)
                {
                    /* Always add NULL terminator */
                    if (count < ESP8266_RX_BUFF_LEN)
                    {
                        response->content[count] = '\0';
                        count++;
                    }
                    else
                    {
                        response->content[ESP8266_RX_BUFF_LEN - 1] = '\0';
                    }
                    ESP8266_DBG_HTTP("CNT: ", count);

                    /* Get content length */
                    if (0 == strncmp("Content-Length:", response->content, strlen("Content-Length:")))
                    {
                        ucpStart = (char *) memchr(response->content, ' ', count);
                        if (NULL != ucpStart)
                        {
                            ucpStart++;
                            ucpEnd = (char *) memchr(ucpStart, '\0', count - (ucpStart - response->content));
                            if (NULL != ucpEnd)
                            {
                                if ((ucpEnd - ucpStart) > 1)
                                {
                                    *ucpEnd = '\0';
                                    response->len = (uint16_t) atoi(ucpStart);
                                }
                            }
                        }
                    }

                }
                else
                {
                    /* Last batch of data is the response content */
                    /* There's nothing to do since the data is already on the content body at this point */
                }

                remaining -= count;
                ESP8266_DBG_HTTP("RMN: ", remaining);
            }
        }
    }
    return rxLength;
}
#endif

size_t ESP8266::write(uint8_t character) 
{
#if (ESP8266_USE_HW_SW_PORT == 1)
    if(_serialPortHandler.isSoftSerial)
    {
        if(NULL != _serialPortHandler._soft)
        {
            _serialPortHandler._soft->write(character);
        }
    }
    else
    {
        if(NULL != _serialPortHandler._hard)
        {
            _serialPortHandler._hard->write(character);
        }
    }
#else
    _serialPort->write(character);
#endif
}

int ESP8266::read()
{
#if (ESP8266_USE_HW_SW_PORT == 1)
    if(_serialPortHandler.isSoftSerial)
    {
        if(NULL != _serialPortHandler._soft)
        {
            return _serialPortHandler._soft->read();
        }
    }
    else
    {
        if(NULL != _serialPortHandler._hard)
        {
            return _serialPortHandler._hard->read();
        }
    }
#else
    return _serialPort->read();
#endif
}

int ESP8266::peek()
{
#if (ESP8266_USE_HW_SW_PORT == 1)
    if(_serialPortHandler.isSoftSerial)
    {
        if(NULL != _serialPortHandler._soft)
        {
            return _serialPortHandler._soft->peek();
        }
    }
    else
    {
        if(NULL != _serialPortHandler._hard)
        {
            return _serialPortHandler._hard->peek();
        }
    }
#else
    return _serialPort->peek();
#endif
}

void ESP8266::flush()
{
#if (ESP8266_USE_HW_SW_PORT == 1)
    if(_serialPortHandler.isSoftSerial)
    {
        if(NULL != _serialPortHandler._soft)
        {
            _serialPortHandler._soft->flush();
        }
    }
    else
    {
        if(NULL != _serialPortHandler._hard)
        {
            _serialPortHandler._hard->flush();
        }
    }
#else
    _serialPort->flush();
#endif
}

int ESP8266::available()
{
#if (ESP8266_USE_HW_SW_PORT == 1)
    if(_serialPortHandler.isSoftSerial)
    {
        if(NULL != _serialPortHandler._soft)
        {
            return _serialPortHandler._soft->available();
        }
    }
    else
    {
        if(NULL != _serialPortHandler._hard)
        {
            return _serialPortHandler._hard->available();
        }
    }
#else
    _serialPort->available();
#endif
}

/* Private functions */

void ESP8266::sendCommand(const char *cmd, at_cmd_type type, char *params)
{
    ESP8266_DBG_PARSE(F("CMD: "), cmd);
    print("AT");
    print(cmd);
    if (ESP8266_CMD_QUERY == type)
    {
        print('?');
    }
    else if ((ESP8266_CMD_SETUP == type) && (NULL != params))
    {
        ESP8266_DBG_PARSE(F("PRM: "), params);
        print('=');
        print(params);
    }
    print("\r\n");
}

int8_t ESP8266::getResponse(char* dest, const char* pass, const char* fail, char delimA, char delimB, uint32_t timeout)
{
    static char _rxBuffer[ESP8266_RX_BUFF_LEN];
    int8_t ret = ESP8266_CMD_RSP_WAIT;
    uint8_t idx = 0;
    uint32_t ulStartTime = 0;

    char *ucpStart = NULL;
    char *ucpEnd = NULL;

    /* Validate arguments */
    if (NULL == pass)
    {
        ret = ESP8266_CMD_RSP_ERROR;
    }
    else
    {
        ESP8266_DBG_PARSE(F("EXP: "), pass);

        for (ulStartTime = millis(); ESP8266_CMD_RSP_WAIT == ret;)
        {
            /* Check for timeout */
            if (timeout < (millis() - ulStartTime))
            {
                ESP8266_DBG_PARSE(F("TIMEOUT: "), (millis() - ulStartTime));
                ret = ESP8266_CMD_RSP_TIMEOUT;
                break;
            }

            /* Read line */
#if (ESP8266_USE_HW_SW_PORT == 1)
            if(_serialPortHandler.isSoftSerial)
            {
                if(NULL != _serialPortHandler._soft)
                {
                    idx = _serialPortHandler._soft->readBytesUntil('\n', _rxBuffer, sizeof(_rxBuffer));
                }
            }
            else
            {
                if(NULL != _serialPortHandler._hard)
                {
                    idx = _serialPortHandler._hard->readBytesUntil('\n', _rxBuffer, sizeof(_rxBuffer));
                }
            }
#else
            idx = _serialPort->readBytesUntil('\n', _rxBuffer, sizeof(_rxBuffer));
#endif

            if (0 < idx)
            {
                /* Always add NULL terminator */
                if (idx < ESP8266_RX_BUFF_LEN)
                {
                    _rxBuffer[idx] = '\0';
                }
                else
                {
                    _rxBuffer[ESP8266_RX_BUFF_LEN - 1] = '\0';
                }

                ESP8266_DBG_PARSE(F("ACT: "), _rxBuffer);

                /* Check for expected response */
                if (0 == strncmp(pass, _rxBuffer, strlen(pass)))
                {
                    ESP8266_DBG_PARSE(F("FND: "), _rxBuffer);

                    /* Search for delimeters */
                    if ((delimA != NULL) && (delimB != NULL))
                    {
                        ucpStart = (char *) memchr(_rxBuffer, delimA, idx);
                        if (NULL != ucpStart)
                        {
                            ucpStart++;
                            ucpEnd = (char *) memchr(ucpStart, delimB, idx - (ucpStart - _rxBuffer));
                            if (NULL != ucpEnd)
                            {
                                if ((ucpEnd - ucpStart) > 1)
                                {
                                    *ucpEnd = '\0';
                                    ESP8266_DBG_PARSE("INS: ", ucpStart);
                                    if (NULL != dest)
                                    {
                                        strcpy(dest, ucpStart);
                                    }
                                    ret = idx;
                                    if (idx < ESP8266_RX_BUFF_LEN)
                                    {
                                        ret++;
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        /* Expected response found and no need to find instances */
                        ret = ESP8266_CMD_RSP_SUCCESS;
                    }
                }
                /* Check for failed response */
                else if (NULL != fail)
                {
                    if (0 == strncmp(fail, _rxBuffer, strlen(fail)))
                    {
                        ret = ESP8266_CMD_RSP_FAILED;
                    }
                }
                /* Check if device is busy */
                else if (0 == strncmp(AT_RESPONSE_BUSY, _rxBuffer, strlen(AT_RESPONSE_BUSY)))
                {
                    ret = ESP8266_CMD_RSP_BUSY;
                }
                /* Check if there is an error */
                else if (0 == strncmp(AT_RESPONSE_ERROR, _rxBuffer, strlen(AT_RESPONSE_ERROR)))
                {
                    ret = ESP8266_CMD_RSP_ERROR;
                }
            }
        }
    }
    return ret;
}
