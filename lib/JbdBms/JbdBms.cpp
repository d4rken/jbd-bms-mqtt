/*
 * \brief Source file for JbdBms class
 * \author rahmaevao and boboxx
 * \version 0.3
 * \date September 2019
 * https://github.com/rahmaevao/JbdBms/commit/831f9186d93d02b18e2886764bd58ab8fd5e7546
 *
 * Modified by @d4rken to work with JBD BMS AP21S001
 */

#include "JbdBms.h"

JbdBms::JbdBms(HardwareSerial *t_hardwareSerial) {
    m_port = t_hardwareSerial;
    m_hwserial = true;
    static_cast<HardwareSerial *>(m_port)->begin(9600);
}

JbdBms::JbdBms(SoftwareSerial *t_softwareSerial) {
    m_port = t_softwareSerial;
    m_hwserial = false;
    static_cast<SoftwareSerial *>(m_port)->begin(9600);
}

JbdBms::JbdBms(int rx, int tx) {
    SoftwareSerial mySerial(rx, tx);
    m_port = &mySerial;
    m_hwserial = false;
    static_cast<SoftwareSerial *>(m_port)->begin(9600);
}

/*!
 * \brief Reading basic data from bms
 * \retval status of reading
 */
bool JbdBms::readBmsData() {
    uint8_t responce[BMS_LEN_RESPONCE];

    sendReqBasicMessage();
    readResponce(responce);

    if (checkCheckSumRecieve(responce) == true) {
        parseReqBasicMessage(responce);
    } else {
        return false;
    }
    return true;
}
/*!
 * \brief Reading pack data from bms
 * \retval status of reading
 */
bool JbdBms::readPackData() {
    uint8_t responce[BMS_LEN_RESPONCE];

    sendCellMessage();
    readResponce(responce);

    if (checkCheckSumRecieve(responce) == true) {
        parseReqPackMessage(responce);
    } else {
        return false;
    }
    return true;
}

float JbdBms::getVoltage() { return m_voltage; }

float JbdBms::getCurrent() { return m_current; }

float JbdBms::getChargePercentage() { return m_chargePercentage; }
uint16_t JbdBms::getProtectionState() { return m_protectionState; }

uint16_t JbdBms::getCycle() { return m_cycle; }

/**
 * \Onboard Temp Probe
 */
float JbdBms::getTemp1() { return m_Temp1; }

/**
 * \External Temp Probe 1
 */
float JbdBms::getTemp2() { return m_Temp2; }

/**
 * \External Temp Probe 2
 */
float JbdBms::getTemp3() { return m_Temp3; }

packCellInfoStruct JbdBms::getPackCellInfo() { return m_packCellInfo; }

void JbdBms::sendReqBasicMessage() {
    uint8_t reqMessage[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
    m_port->write(reqMessage, 7);
}

void JbdBms::sendCellMessage() {
    uint8_t reqMessage[] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};
    m_port->write(reqMessage, 7);
}

void JbdBms::parseReqBasicMessage(uint8_t *t_message) {
    m_voltage = (float)convertTwoIntsToUint16(t_message[4], t_message[5]) / 100;
    m_current = ((float)convertTwoIntsToInt16(t_message[6], t_message[7])) * 10;
    m_chargePercentage = t_message[23];
    m_protectionState = ((float)convertTwoIntsToUint16(t_message[20], t_message[21]));
    m_cycle = ((float)convertTwoIntsToUint16(t_message[12], t_message[13]));
    m_Temp1 = (((float)convertTwoIntsToUint16(t_message[27], t_message[28])) - 2731) / 10.00f;
    m_Temp2 = (((float)convertTwoIntsToUint16(t_message[29], t_message[30])) - 2731) / 10.00f;
    m_Temp3 = (((float)convertTwoIntsToUint16(t_message[31], t_message[32])) - 2731) / 10.00f;
}

void JbdBms::parseReqPackMessage(uint8_t *t_message) {  // packCellInfoStruct * t_packCellInfo) {
    uint16_t _cellSum = 0;
    uint16_t _CellLow = 5000;  // 5v
    uint16_t _CellHigh = 0;

    m_packCellInfo.NumOfCells = t_message[3] / 2;  // Data length * 2 is number of cells !!!!!!

    byte offset = 4;
    for (byte i = 0; i < m_packCellInfo.NumOfCells; i++) {
        m_packCellInfo.CellVoltage[i] = ((uint16_t)convertTwoIntsToUint16(
            t_message[i * 2 + offset], t_message[i * 2 + 1 + offset]));  // Data length * 2 is number of cells !!!!!!

        _cellSum += m_packCellInfo.CellVoltage[i];

        if (m_packCellInfo.CellVoltage[i] > _CellHigh) {
            _CellHigh = m_packCellInfo.CellVoltage[i];
        }
        if (m_packCellInfo.CellVoltage[i] < _CellLow) {
            _CellLow = m_packCellInfo.CellVoltage[i];
        }

        m_packCellInfo.CellLow = _CellLow;
        m_packCellInfo.CellHigh = _CellHigh;
        m_packCellInfo.CellDiff = _CellHigh - _CellLow;  // Resolution 10 mV -> convert to volts
        m_packCellInfo.CellAvg = _cellSum / m_packCellInfo.NumOfCells;
    }
}

bool JbdBms::readResponce(uint8_t *t_outMessage) {
    uint8_t i = 0;
    bool findBeginByte = false;
    uint32_t statrTime = millis();
    while (i <= BMS_LEN_RESPONCE - 1) {
        if (abs((millis() - statrTime) > getMaxTimeout())) {
            return false;
        }
        if (m_port->available() > 0) {
            // if (Serial2.available() > 0) {
            uint8_t thisByte = m_port->read();
            // uint8_t thisByte = Serial2.read();
            if (thisByte == 0xDD) {
                findBeginByte = true;
            }
            if (findBeginByte) {
                t_outMessage[i] = thisByte;
                // Serial.println(String(i)+"=" + String(thisByte));
                i++;
            }
        }
    }
    return true;
}

/**
 * \brief Compute and cheking check sum in message
 * \param [in] t_message
 * \return Fact of passing the test:
 *  true - check passed;
 *  false - check not passed
 */
bool JbdBms::checkCheckSumRecieve(uint8_t *t_message) {
    uint16_t checkSumCompute;
    uint16_t checkSumAccepted;
    uint8_t lengthData;
    uint8_t startIndexCS;

    if (t_message[2] != 0)  // Status OK
        return false;

    lengthData = t_message[3];
    checkSumCompute = computeCrc16JbdChina(t_message, BMS_LEN_RESPONCE);
    startIndexCS = lengthData + 4;

    checkSumAccepted = (t_message[startIndexCS] << 8) | t_message[startIndexCS + 1];

    if (checkSumCompute != checkSumAccepted) return false;
    return true;
}

/**
 * \brief Compute check sum in message for JBD protocol semiModbus :)
 * \param[in] puchMsg Message buffer containing binary data to be used for
 *            generating the CRC.
 * \param[in] usDataLen The quantity of bytes in the message buffer.
 * \return The function returns the CRC.
 */
uint16_t JbdBms::computeCrc16JbdChina(uint8_t *puchMsg, uint8_t usDataLen) {
    uint8_t lengthData = puchMsg[3];

    uint16_t summa = 0;

    for (int i = 4; i < lengthData + 4; i++) summa = summa + puchMsg[i];

    uint16_t checkSum = (summa + lengthData - 1) ^ 0xFFFF;
    return checkSum;
}

uint32_t JbdBms::getMaxTimeout() { return 100; }

int16_t JbdBms::convertTwoIntsToInt16(int highbyte, int lowbyte) {
    int16_t value = convertTwoIntsToUint16(highbyte, lowbyte);
    if (value & 0b1000000000000000)  // Test for positive / negative with bit mask
    {
        value = ~value;
        value = value * -1;
    }
    return value;
}

/**
 * \brief Build one uint16_t out of two uint8_t
 */
uint16_t JbdBms::convertTwoIntsToUint16(int highbyte, int lowbyte)  // turns two bytes into a single long integer
{
    uint16_t a16bitvar = (highbyte);
    a16bitvar <<= 8;                    // Left shift 8 bits,
    a16bitvar = (a16bitvar | lowbyte);  // OR operation, merge the two
    return a16bitvar;
}
