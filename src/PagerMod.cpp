#include<Arduino.h>
#include<RadioLib.h>
#include "BCH3121.hpp"
#include "networks.hpp"


int16_t PagerClient::readDataMod(String &str, size_t len, uint32_t *addr, uint32_t *func, bool *add, size_t *clen) {
    int16_t state = RADIOLIB_ERR_NONE;

    // determine the message length, based on user input or the amount of received data
    size_t length = len;
    if (length == 0) {
        // one batch can contain at most 80 message symbols
        length = available() * 80;
    }

    // build a temporary buffer
#if defined(RADIOLIB_STATIC_ONLY)
    uint8_t data[RADIOLIB_STATIC_ARRAY_SIZE + 1];
#else
    uint8_t *data = new uint8_t[length + 1];
    if (!data) {
        return (RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
    }
#endif

    // read the received data
    state = readDataM(data, &length, addr, func, add, clen);

    if (state == RADIOLIB_ERR_NONE) {
        // check tone-only tramsissions
        if (length == 0) {
            length = 6;
            strncpy((char *) data, "<tone>", length + 1);
        }

        // add null terminator
        data[length] = 0;

        // initialize Arduino String class
        str = String((char *) data);
    }

    // deallocate temporary buffer
#if !defined(RADIOLIB_STATIC_ONLY)
    delete[] data;
#endif

    return (state);
}

int16_t PagerClient::readDataM(uint8_t *data, size_t *len, uint32_t *addr, uint32_t *func, bool *add, size_t *clen) {
    // find the correct address
    bool match = false;
    uint8_t framePos = 0;
    uint8_t symbolLength = 0;
    uint16_t errors = 0;
    CBCH3121 PocsagFec;
    bool is_sync = true;

    while (!match && phyLayer->available()) {

        uint32_t cw = read();
        framePos++;

        if (!is_sync) {
            if (!PocsagFec.decode(cw, errors)) {
                Serial.println("BCH Failed twice.");
                break; // failed within two batches, drop message.
            }
        }

        if (!PocsagFec.decode(cw, errors)) {
            if (is_sync)
                is_sync = false;
            Serial.println("BCH Failed.");
            continue;
        } else {
            is_sync = true;
        }

        // check if it's the idle code word
        if (cw == RADIOLIB_PAGER_IDLE_CODE_WORD) {
            continue;
        }

        // check if it's the sync word
        if (cw == RADIOLIB_PAGER_FRAME_SYNC_CODE_WORD) {
            framePos = 0;
            continue;
        }


        // not an idle code word, check if it's an address word
        if (cw & (RADIOLIB_PAGER_MESSAGE_CODE_WORD << (RADIOLIB_PAGER_CODE_WORD_LEN - 1))) {
            // this is pretty weird, it seems to be a message code word without address
            continue;
        }

        // should be an address code word, extract the address
        uint32_t addr_found =
                ((cw & RADIOLIB_PAGER_ADDRESS_BITS_MASK) >> (RADIOLIB_PAGER_ADDRESS_POS - 3)) | (framePos / 2);
        if ((addr_found & filterMask) == (filterAddr & filterMask)) {
            // we have a match!
            match = true;
            if (addr) {
                *addr = addr_found;
                *func = (cw & RADIOLIB_PAGER_FUNCTION_BITS_MASK) >> RADIOLIB_PAGER_FUNC_BITS_POS;
            }

//            Serial.print("Function: ");
//            Serial.print((cw & RADIOLIB_PAGER_FUNCTION_BITS_MASK) >> RADIOLIB_PAGER_FUNC_BITS_POS);


            // determine the encoding from the function bits
            // No determine
            symbolLength = 4;
        }
    }

    if (!match) {
        // address not found
        return (RADIOLIB_ERR_ADDRESS_NOT_FOUND);
    }

    // we have the address, start pulling out the message
    bool complete = false;
    size_t decodedBytes = 0;
    uint32_t prevCw = 0;
    bool overflow = false;
    int8_t ovfBits = 0;
    errors = 0;
    while (!complete && phyLayer->available()) {
        uint32_t cw = read();

        if (!is_sync) {
            if (!PocsagFec.decode(cw, errors)) {
                Serial.println("BCH Failed msg twice.");
                break;
            }
        }

        if (PocsagFec.decode(cw, errors)) {
            is_sync = true;
        } else {
            if (is_sync)
                Serial.println("BCH Failed.");
            is_sync = false;
            continue;
        }

        // check if it's the idle code word
        if (cw == RADIOLIB_PAGER_IDLE_CODE_WORD) {
            complete = true;
            break;
        }

        // skip the sync words
        if (cw == RADIOLIB_PAGER_FRAME_SYNC_CODE_WORD) {
            continue;
        }

        if (!(cw & (RADIOLIB_PAGER_MESSAGE_CODE_WORD << (RADIOLIB_PAGER_CODE_WORD_LEN - 1)))) {
            uint32_t addr_found =
                    ((cw & RADIOLIB_PAGER_ADDRESS_BITS_MASK) >> (RADIOLIB_PAGER_ADDRESS_POS - 3)) | (framePos / 2);
            if (addr_found == 1234000 || addr_found == 1234002) {
                *clen = decodedBytes;
                *add = true;
                continue;
            }
        }

        // if (PocsagFec.decode(cw, errors)) {
        //   // Serial.print("\nBCH Corrected ");
        //   // Serial.print(errors);
        //   // Serial.print("Errors.\r");
        //   errors = 0;
        // } else {
        //   // Serial.print("\nBCH ERROR. ");
        //   // Serial.print(errors);
        //   // Serial.print("Errors.\r");
        //   // State_code = POCSAG_ERR_PARITY_NOTMATCH;
        //   break;
        // }

        // check overflow from previous code word
        uint8_t bitPos = RADIOLIB_PAGER_CODE_WORD_LEN - 1 - symbolLength;
        if (overflow) {
            overflow = false;

            // this is a bit convoluted - first, build masks for both previous and current code word
            uint8_t currPos = RADIOLIB_PAGER_CODE_WORD_LEN - 1 - symbolLength + ovfBits;
            uint8_t prevPos = RADIOLIB_PAGER_MESSAGE_END_POS;
            uint32_t prevMask =
                    (0x7FUL << prevPos) & ~((uint32_t) 0x7FUL << (RADIOLIB_PAGER_MESSAGE_END_POS + ovfBits));
            uint32_t currMask = (0x7FUL << currPos) & ~((uint32_t) 1 << (RADIOLIB_PAGER_CODE_WORD_LEN - 1));

            // next, get the two parts of the message symbol and stick them together
            uint8_t prevSymbol = (prevCw & prevMask) >> prevPos;
            uint8_t currSymbol = (cw & currMask) >> currPos;
            uint32_t symbol = prevSymbol << (symbolLength - ovfBits) | currSymbol;

            // finally, we can flip the bits
            symbol = Module::reflect((uint8_t) symbol, 8);
            symbol >>= (8 - symbolLength);

            // decode BCD and we're done
            if (symbolLength == 4) {
                symbol = decodeBCD(symbol);
            }
            data[decodedBytes++] = symbol;

            // adjust the bit position of the next message symbol
            bitPos += ovfBits;
            bitPos -= symbolLength;
        }

        // get the message symbols based on the encoding type
        while (bitPos >= RADIOLIB_PAGER_MESSAGE_END_POS) {
            // get the message symbol from the code word and reverse bits
            uint32_t symbol = (cw & (0x7FUL << bitPos)) >> bitPos;
            symbol = Module::reflect((uint8_t) symbol, 8);
            symbol >>= (8 - symbolLength);

            // decode BCD if needed
            if (symbolLength == 4) {
                symbol = decodeBCD(symbol);
            }
            data[decodedBytes++] = symbol;

            // now calculate if the next symbol is overflowing to the following code word
            int8_t remBits = bitPos - RADIOLIB_PAGER_MESSAGE_END_POS;
            if (remBits < symbolLength) {
                // overflow!
                prevCw = cw;
                overflow = true;
                ovfBits = remBits;
            }
            bitPos -= symbolLength;
        }

    }

    // save the number of decoded bytes
    *len = decodedBytes;
    return (RADIOLIB_ERR_NONE);
}

int16_t PagerClient::readDataMSA(struct PagerClient::pocsag_data *p, size_t len) {
    int16_t state = RADIOLIB_ERR_NONE;
    bool complete = false;
    uint8_t framePos = 0;
    uint32_t addr_next = 0;
//        bool is_empty = true;
    for (size_t i = 0; i < POCDAT_SIZE; i++) {
        // determine the message length, based on user input or the amount of received data
        size_t length = len;
        if (len == 0) {
            // one batch can contain at most 80 message symbols
            len = available() * 80;
        }

        if (complete)
            break;

        // build a temporary buffer
#if defined(RADIOLIB_STATIC_ONLY)
        uint8_t data[RADIOLIB_STATIC_ARRAY_SIZE + 1];
#else
        // auto *data = new uint8_t[len + 1];
        // if (!data) {
        //     return (RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
        // }
#endif
        uint8_t data[len + 1];

        state = readDataMA(data, &length, &p[i].addr, &p[i].func, &framePos, &addr_next, &p[i].is_empty,
                           &complete, &p[i].errs_total, &p[i].errs_uncorrected);

        if (i && state == RADIOLIB_ERR_ADDRESS_NOT_FOUND) {
//                Serial.println("ADDR NO MATCH");
//             delete[] data;
//             data = nullptr;
            state = RADIOLIB_ERR_NONE;
            break;
        }

        if (i && state == RADIOLIB_ERR_MSG_CORRUPT) {
            Serial.printf("[D] MSG%d CORRUPT.\n", i);
            // Serial.printf("[D] data[] len %d, message len %d, data addr %p\n", len + 1, length, data);
            // delete[] data; // Due to unknown reason crash often occurs here.
            // Fixed by stop using new and delete...no choice, sorry.
            // data = nullptr; // REMEMBER TO INITIALIZE POINTER AFTER DELETE!!!
            state = RADIOLIB_ERR_NONE;
            break;
        }

        if (state == RADIOLIB_ERR_NONE && !p[i].is_empty) {
            if (length == 0) {
                p[i].is_empty = true;
//                    length = 6;
//                    strncpy((char *) data, "<tone>", length + 1);
            }
            data[length] = 0;
            p[i].str = String((char *) data);
            if (!p[i].str.length())
                p[i].is_empty = true;
            p[i].len = length;
//                length = 0;

        }
#if !defined(RADIOLIB_STATIC_ONLY)
        // delete[] data;
        // data = nullptr;
#endif
        if (state != RADIOLIB_ERR_NONE)
            break;
    }
//    // build a temporary buffer
//#if defined(RADIOLIB_STATIC_ONLY)
//    uint8_t data[RADIOLIB_STATIC_ARRAY_SIZE + 1];
//#else
//    uint8_t *data = new uint8_t[length + 1];
//    if (!data) {
//        return (RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
//    }
//#endif
//
//    // read the received data
////    state = readDataMSA(data, &length, addr, func, add, clen);
//
//    if (state == RADIOLIB_ERR_NONE) {
//        // check tone-only tramsissions
//        if (length == 0) {
//            length = 6;
//            strncpy((char *) data, "<tone>", length + 1);
//        }
//
//        // add null terminator
//        data[length] = 0;
//
//        // initialize Arduino String class
////        str = String((char *) data);
//    }
//
//    // deallocate temporary buffer
//#if !defined(RADIOLIB_STATIC_ONLY)
//    delete[] data;
//#endif

    return (state);
}

int16_t PagerClient::readDataMA(uint8_t *data, size_t *len, uint32_t *addr, uint32_t *func, uint8_t *framePos,
                                uint32_t *addr_next, bool *is_empty, bool *complete, uint16_t *errs_total,
                                uint16_t *errs_uncorrected) {
    // find the correct address
    bool match = false;
//    uint8_t framePos = 0;
    uint8_t symbolLength = 0;
    uint16_t errors = 0;
    uint16_t err_prev = 0;
    CBCH3121 PocsagFec;
    bool is_sync = true;

    if (*addr_next) {
        uint32_t addr_found =
                ((*addr_next & RADIOLIB_PAGER_ADDRESS_BITS_MASK) >> (RADIOLIB_PAGER_ADDRESS_POS - 3)) | (*framePos / 2);
        if ((addr_found & filterMask) == (filterAddr & filterMask)) {
            *is_empty = false;
            match = true;
            symbolLength = 4;
            *addr = addr_found;
            *func = (*addr_next & RADIOLIB_PAGER_FUNCTION_BITS_MASK) >> RADIOLIB_PAGER_FUNC_BITS_POS;
            *addr_next = 0;
        } else {
            return (RADIOLIB_ERR_ADDRESS_NOT_FOUND);
        }
    }

    while (!match && phyLayer->available()) {

        uint32_t cw = read();
//        *framePos++;
        *framePos = *framePos + 1;


        if (!is_sync) {
            err_prev = errors;
            if (!PocsagFec.decode(cw, errors)) {
                *errs_uncorrected += errors - err_prev;
//                Serial.println("BCH Failed twice.");
                *errs_total = errors;
                return (RADIOLIB_ERR_MSG_CORRUPT); // failed within two batches, drop message.
//                break;
            }
        }

        err_prev = errors;
        if (!PocsagFec.decode(cw, errors)) {
            if (is_sync)
                is_sync = false;
//            Serial.println("BCH Failed.");
            *errs_uncorrected += errors - err_prev;
            continue;
        } else {
            is_sync = true;
        }

        // check if it's the idle code word
        if (cw == RADIOLIB_PAGER_IDLE_CODE_WORD) {
//            Serial.println("IDLE FOUND.");
            continue;
        }

        // check if it's the sync word
        if (cw == RADIOLIB_PAGER_FRAME_SYNC_CODE_WORD) {
            *framePos = 0;
//            Serial.println("SYNC FOUND.");
            continue;
        }


        // not an idle code word, check if it's an address word
        if (cw & (RADIOLIB_PAGER_MESSAGE_CODE_WORD << (RADIOLIB_PAGER_CODE_WORD_LEN - 1))) {
            // this is pretty weird, it seems to be a message code word without address
            continue;
        }

        // should be an address code word, extract the address
        uint32_t addr_found =
                ((cw & RADIOLIB_PAGER_ADDRESS_BITS_MASK) >> (RADIOLIB_PAGER_ADDRESS_POS - 3)) | (*framePos / 2);
        if ((addr_found & filterMask) == (filterAddr & filterMask)) {
            // we have a match!
            *is_empty = false;
            match = true;
//            Serial.printf("GOT ADDR %d \n",addr_found);
//            Serial.printf("RAW CW %X \n",cw);
            if (addr) {
                *addr = addr_found;
                *func = (cw & RADIOLIB_PAGER_FUNCTION_BITS_MASK) >> RADIOLIB_PAGER_FUNC_BITS_POS;
            }

//            Serial.print("Function: ");
//            Serial.print((cw & RADIOLIB_PAGER_FUNCTION_BITS_MASK) >> RADIOLIB_PAGER_FUNC_BITS_POS);


            // determine the encoding from the function bits
            // No determine
            symbolLength = 4;
        }
    }

    if (!match) {
        // address not found
        return (RADIOLIB_ERR_ADDRESS_NOT_FOUND);
    }

    // we have the address, start pulling out the message
//    bool complete = false;
    size_t decodedBytes = 0;
    uint32_t prevCw = 0;
    bool overflow = false;
    int8_t ovfBits = 0;
    errors = 0;
    while (!*complete && phyLayer->available()) {
//        *framePos++;
        *framePos = *framePos + 1;
        uint32_t cw = read();

        if (!is_sync) {
            err_prev = errors;
            if (!PocsagFec.decode(cw, errors)) {
//                Serial.printf("BCH Failed twice. ERR %d \n",errors);
                *errs_uncorrected += errors - err_prev;
                *errs_total = errors;
                return (RADIOLIB_ERR_MSG_CORRUPT);
            } else {
//                Serial.printf("SYNC RECOVERED, ERRORS %d \n",errors);
            }
        }

        err_prev = errors;
        if (PocsagFec.decode(cw, errors)) {
            is_sync = true;
//            Serial.printf("SYNC BCH CHECK PERFORMED, ERRORS %d \n", errors);
        } else {
            if (is_sync)
                is_sync = false;
//            Serial.printf("BCH Failed. ERR %d \n",errors);
            for (size_t i = 0; i < 5; i++) {
                data[decodedBytes++] = 'X';
            }
            *errs_uncorrected += errors - err_prev;
            continue;
        }

        // check if it's the idle code word
        if (cw == RADIOLIB_PAGER_IDLE_CODE_WORD) {
//            Serial.println("IDLE FOUND.");
            *complete = true;
            break;
        }

        // skip the sync words
        if (cw == RADIOLIB_PAGER_FRAME_SYNC_CODE_WORD) {
//            Serial.println("SYNC FOUND.");
            continue;
        }

        if (!(cw & (RADIOLIB_PAGER_MESSAGE_CODE_WORD << (RADIOLIB_PAGER_CODE_WORD_LEN - 1)))) {
            *addr_next = cw; // returned codeword actually for function determination.
            break;
        }

        // if (PocsagFec.decode(cw, errors)) {
        //   // Serial.print("\nBCH Corrected ");
        //   // Serial.print(errors);
        //   // Serial.print("Errors.\r");
        //   errors = 0;
        // } else {
        //   // Serial.print("\nBCH ERROR. ");
        //   // Serial.print(errors);
        //   // Serial.print("Errors.\r");
        //   // State_code = POCSAG_ERR_PARITY_NOTMATCH;
        //   break;
        // }

        // check overflow from previous code word
        uint8_t bitPos = RADIOLIB_PAGER_CODE_WORD_LEN - 1 - symbolLength;
//        Serial.println("GOT DATA.");
//        Serial.printf("RAW CW %X \n",cw);
        if (overflow) {
            overflow = false;

            // this is a bit convoluted - first, build masks for both previous and current code word
            uint8_t currPos = RADIOLIB_PAGER_CODE_WORD_LEN - 1 - symbolLength + ovfBits;
            uint8_t prevPos = RADIOLIB_PAGER_MESSAGE_END_POS;
            uint32_t prevMask =
                    (0x7FUL << prevPos) & ~((uint32_t) 0x7FUL << (RADIOLIB_PAGER_MESSAGE_END_POS + ovfBits));
            uint32_t currMask = (0x7FUL << currPos) & ~((uint32_t) 1 << (RADIOLIB_PAGER_CODE_WORD_LEN - 1));

            // next, get the two parts of the message symbol and stick them together
            uint8_t prevSymbol = (prevCw & prevMask) >> prevPos;
            uint8_t currSymbol = (cw & currMask) >> currPos;
            uint32_t symbol = prevSymbol << (symbolLength - ovfBits) | currSymbol;

            // finally, we can flip the bits
            symbol = Module::reflect((uint8_t) symbol, 8);
            symbol >>= (8 - symbolLength);

            // decode BCD and we're done
            if (symbolLength == 4) {
                symbol = decodeBCD(symbol);
            }
//            Serial.printf("DE LEN %d \n",decodedBytes);
            data[decodedBytes++] = symbol;

            // adjust the bit position of the next message symbol
            bitPos += ovfBits;
            bitPos -= symbolLength;
        }

        // get the message symbols based on the encoding type
        while (bitPos >= RADIOLIB_PAGER_MESSAGE_END_POS) {
            // get the message symbol from the code word and reverse bits
            uint32_t symbol = (cw & (0x7FUL << bitPos)) >> bitPos;
            symbol = Module::reflect((uint8_t) symbol, 8);
            symbol >>= (8 - symbolLength);

            // decode BCD if needed
            if (symbolLength == 4) {
                symbol = decodeBCD(symbol);
            }
//            Serial.printf("DE LEN %d \n",decodedBytes);
            data[decodedBytes++] = symbol;

            // now calculate if the next symbol is overflowing to the following code word
            int8_t remBits = bitPos - RADIOLIB_PAGER_MESSAGE_END_POS;
            if (remBits < symbolLength) {
                // overflow!
                prevCw = cw;
                overflow = true;
                ovfBits = remBits;
            }
            bitPos -= symbolLength;
        }

    }

    // save the number of decoded bytes
    *len = decodedBytes;
    *errs_total = errors;
    return (RADIOLIB_ERR_NONE);
}


int16_t PagerClient::readDataMS(struct PagerClient::pocsag_data *p, size_t len) {
    int16_t state = RADIOLIB_ERR_NONE;

    // determine the message length, based on user input or the amount of received data
    size_t length = len;
    if (length == 0) {
        // one batch can contain at most 80 message symbols
        length = available() * 80;
    }

    // build a temporary buffer
#if defined(RADIOLIB_STATIC_ONLY)
    uint8_t data[RADIOLIB_STATIC_ARRAY_SIZE + 1];
#else
//    uint8_t *data = new uint8_t[length + 1];
    for (size_t i = 0; i < POCDAT_SIZE; i++) {
        p[i].data = new uint8_t[length + 1];
        if (!&p[i].data) {
            return (RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
        }
    }

#endif

    // read the received data

    state = readDataS(p);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[ESP32] Free MEM " + String(esp_get_free_heap_size() / 1024) + " KBytes");
        // check tone-only tramsissions
        for (size_t i = 0; i < POCDAT_SIZE; i++) {
            if (p[i].is_empty) {
                delete[] p[i].data;
                continue;
            }
            length = p[i].len;
            if (length == 0) {
                length = 6;
                strncpy((char *) p[i].data, "<tone>", length + 1);
            }

            // add null terminator
            p[i].data[length] = 0;

            // initialize Arduino String class
            p[i].str = String((char *) p[i].data);


            // deallocate temporary buffer
#if !defined(RADIOLIB_STATIC_ONLY)
            delete[] p[i].data;
#endif
        }
    }
    return (state);
}

int16_t PagerClient::readDataS(struct PagerClient::pocsag_data *p) {
    // find the correct address
    bool match = false;
    uint8_t framePos = 0;
    uint8_t symbolLength = 0;
    uint16_t errors = 0;
    CBCH3121 PocsagFec;
    bool is_sync = true;
    uint16_t cnt = 0;

    bool complete = false;
    size_t decodedBytes = 0;
    uint32_t prevCw = 0;
    bool overflow = false;
    int8_t ovfBits = 0;

    while (!complete && phyLayer->available()) {

        uint32_t cw = read();
        framePos++;

        if (cnt >= POCDAT_SIZE - 1) {
            Serial.println("EXCEED STRUCT LIMIT.");
            return (RADIOLIB_ERR_INVALID_PAYLOAD);
        }


        if (!is_sync) {
            if (!PocsagFec.decode(cw, errors)) {
                Serial.printf("BCH Failed twice. ERR %d \n", errors);
                errors = 0;
                p[cnt].len = decodedBytes;
                Serial.printf("data struct %d, len %d \n", cnt, p[cnt].len);
                if (!match) {
                    if (!cnt)
                        return (RADIOLIB_ERR_ADDRESS_NOT_FOUND);
                    Serial.println("ADDR NO MATCH IN SYNC.");
                    return (RADIOLIB_ERR_NONE);
                }
                break; // failed within two batches, drop message.
            } else {
                Serial.printf("SYNC RECOVERED, ERRORS %d \n", errors);
                errors = 0;
            }
        }

        if (!PocsagFec.decode(cw, errors)) {
            if (is_sync)
                is_sync = false;
            Serial.printf("BCH Failed. ERR %d \n", errors);
            errors = 0;
            continue;
        } else {
            Serial.printf("SYNC BCH CHECK PERFORMED, ERRORS %d \n", errors);
            errors = 0;
            is_sync = true;
        }

        // check if it's the idle code word
        if (cw == RADIOLIB_PAGER_IDLE_CODE_WORD) {
            complete = true;
            // save the number of decoded bytes
            Serial.println("IDLE FIND.");
            p[cnt].len = decodedBytes;
            Serial.printf("data struct %d, len %d \n", cnt, p[cnt].len);
            break;
        }

        // skip the sync words
        if (cw == RADIOLIB_PAGER_FRAME_SYNC_CODE_WORD) {
            Serial.println("SYNC FIND");
            continue;
        }


        // not an idle code word, check if it's an address word
        if (cw & (RADIOLIB_PAGER_MESSAGE_CODE_WORD << (RADIOLIB_PAGER_CODE_WORD_LEN - 1))) {
            Serial.println("GOT DATA.");
            Serial.printf("RAW CW %X \n", cw);
            // check overflow from previous code word
            uint8_t bitPos = RADIOLIB_PAGER_CODE_WORD_LEN - 1 - symbolLength;
            if (overflow) {
                overflow = false;
                Serial.println("OVF WARNING");

                // this is a bit convoluted - first, build masks for both previous and current code word
                uint8_t currPos = RADIOLIB_PAGER_CODE_WORD_LEN - 1 - symbolLength + ovfBits;
                uint8_t prevPos = RADIOLIB_PAGER_MESSAGE_END_POS;
                uint32_t prevMask =
                        (0x7FUL << prevPos) & ~((uint32_t) 0x7FUL << (RADIOLIB_PAGER_MESSAGE_END_POS + ovfBits));
                uint32_t currMask = (0x7FUL << currPos) & ~((uint32_t) 1 << (RADIOLIB_PAGER_CODE_WORD_LEN - 1));

                // next, get the two parts of the message symbol and stick them together
                uint8_t prevSymbol = (prevCw & prevMask) >> prevPos;
                uint8_t currSymbol = (cw & currMask) >> currPos;
                uint32_t symbol = prevSymbol << (symbolLength - ovfBits) | currSymbol;

                // finally, we can flip the bits
                symbol = Module::reflect((uint8_t) symbol, 8);
                symbol >>= (8 - symbolLength);

                // decode BCD and we're done
                if (symbolLength == 4) {
                    symbol = decodeBCD(symbol);
                }
                p[cnt].data[decodedBytes++] = symbol;

                // adjust the bit position of the next message symbol
                bitPos += ovfBits;
                bitPos -= symbolLength;
            }

            // get the message symbols based on the encoding type
            while (bitPos >= RADIOLIB_PAGER_MESSAGE_END_POS) {
                // get the message symbol from the code word and reverse bits
                uint32_t symbol = (cw & (0x7FUL << bitPos)) >> bitPos;
                symbol = Module::reflect((uint8_t) symbol, 8);
                symbol >>= (8 - symbolLength);

                // decode BCD if needed
                if (symbolLength == 4) {
                    symbol = decodeBCD(symbol);
                }

                p[cnt].data[decodedBytes++] = symbol;


                // now calculate if the next symbol is overflowing to the following code word
                int8_t remBits = bitPos - RADIOLIB_PAGER_MESSAGE_END_POS;
                if (remBits < symbolLength) {
                    // overflow!
                    prevCw = cw;
                    overflow = true;
                    ovfBits = remBits;
                }
                bitPos -= symbolLength;
            }
        } else {
            // should be an address code word, extract the address
            if (!p[cnt].is_empty) {
                match = false;
                p[cnt].len = decodedBytes;
                Serial.printf("data struct %d, len %d \n", cnt, p[cnt].len);
                errors = 0;
                decodedBytes = 0;
                prevCw = 0;
                overflow = false;
                ovfBits = 0;
                cnt++;
            }

            uint32_t addr_found =
                    ((cw & RADIOLIB_PAGER_ADDRESS_BITS_MASK) >> (RADIOLIB_PAGER_ADDRESS_POS - 3)) | (framePos / 2);
            if ((addr_found & filterMask) == (filterAddr & filterMask)) {
                // we have a match!
                match = true;
                p[cnt].is_empty = false;
                if (!p[cnt].addr) {
                    Serial.printf("GOT ADDR %d \n", addr_found);
                    Serial.printf("RAW CW %X \n", cw);
                    p[cnt].addr = addr_found;
                    p[cnt].func = (cw & RADIOLIB_PAGER_FUNCTION_BITS_MASK) >> RADIOLIB_PAGER_FUNC_BITS_POS;
                }
                // addr_found = 0;

//            Serial.print("Function: ");
//            Serial.print((cw & RADIOLIB_PAGER_FUNCTION_BITS_MASK) >> RADIOLIB_PAGER_FUNC_BITS_POS);


                // determine the encoding from the function bits
                // No determine
                symbolLength = 4;
            }
        }

        if (!match) {
            if (!cnt)
                return (RADIOLIB_ERR_ADDRESS_NOT_FOUND);
            Serial.println("ADDR NO MATCH.");
            return (RADIOLIB_ERR_NONE);
        }


//    // we have the address, start pulling out the message
//        if (!is_sync){
//            if(!PocsagFec.decode(cw,errors)){
//                Serial.println("BCH Failed msg twice.");
//                break;
//            }
//        }
//
//        if (PocsagFec.decode(cw, errors)) {
//            is_sync = true;
//        } else {
//            if (is_sync)
//                Serial.println("BCH Failed.");
//            is_sync = false;
//            continue;
//        }



        // if (PocsagFec.decode(cw, errors)) {
        //   // Serial.print("\nBCH Corrected ");
        //   // Serial.print(errors);
        //   // Serial.print("Errors.\r");
        //   errors = 0;
        // } else {
        //   // Serial.print("\nBCH ERROR. ");
        //   // Serial.print(errors);
        //   // Serial.print("Errors.\r");
        //   // State_code = POCSAG_ERR_PARITY_NOTMATCH;
        //   break;
        // }



    }

    return (RADIOLIB_ERR_NONE);
}


//int16_t do_one_bit(){
//
//}
