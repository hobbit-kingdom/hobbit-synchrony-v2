#pragma once
//ENGLISH HEADER COMMENT
/** 
 * @file Message.h
 * @brief This file defines a messaging system with various message types for communication.
 *
 * The messaging system includes a base message class and derived classes for different message types:
 * - TextMessage: Represents a text message.
 * - EventMessage: Represents an event message, storing event data in a queue.
 * - SnapshotMessage: Represents a snapshot message, storing snapshot data in a queue.
 * - ClientListMessage: Represents a message containing a list of client IDs.
 * - ClientIDMessage: Represents a message containing a single client ID.
 *
 * The system supports serialization and deserialization of messages to and from a byte buffer.
 *
 * Preconditions:
 * - The input data for constructing messages must be valid and correctly formatted.
 * - The buffer used for serialization must be properly allocated and managed.
 *
 * Postconditions:
 * - Upon successful serialization, the buffer will contain a byte representation of the message.
 * - Upon successful deserialization, a new instance of the appropriate message type will be created.
 *
 * Dynamic Memory Usage:
 * - Dynamic memory is used in the deserialization process where new message objects are created using `new`.
 * - The user of this API is responsible for managing the lifetime of these dynamically allocated message objects.
 *
 * Threading:
 * - The provided code does not explicitly manage threads; however, it is designed to be thread-safe in terms of message handling.
 * - If used in a multi-threaded environment, care should be taken to synchronize access to shared resources (e.g., message queues).
 *
 * Note: The user should ensure that the appropriate platform-specific headers are included for network operations.
 */
//RUSSIAN HEADER COMMENT
/**
 * @file Message.h
 * @brief Этот файл определяет систему сообщений с различными типами сообщений для коммуникации.
 *
 * Система сообщений включает базовый класс сообщения и производные классы для различных типов сообщений:
 * - TextMessage: Представляет текстовое сообщение.
 * - EventMessage: Представляет сообщение события, хранящее данные события в очереди.
 * - SnapshotMessage: Представляет сообщение снимка, хранящее данные снимка в очереди.
 * - ClientListMessage: Представляет сообщение, содержащее список идентификаторов клиентов.
 * - ClientIDMessage: Представляет сообщение, содержащее один идентификатор клиента.
 *
 * Система поддерживает сериализацию и десериализацию сообщений в и из байтового буфера.
 *
 * Предусловия:
 * - Входные данные для создания сообщений должны быть действительными и правильно отформатированными.
 * - Буфер, используемый для сериализации, должен быть правильно выделен и управляем.
 *
 * Постусловия:
 * - После успешной сериализации буфер будет содержать байтовое представление сообщения.
 * - После успешной десериализации будет создан новый экземпляр соответствующего типа сообщения.
 *
 * Динамическое использование памяти:
 * - Динамическая память используется в процессе десериализации, когда создаются новые объекты сообщений с помощью `new`.
 * - Пользователь этого API несет ответственность за управление временем жизни этих динамически выделенных объектов сообщений.
 *
 * Потоки:
 * - Предоставленный код не управляет потоками явно; однако он разработан так, чтобы быть потокобезопасным в отношении обработки сообщений.
 * - Если используется в многопоточной среде, следует позаботиться о синхронизации доступа к общим ресурсам (например, к очередям сообщений).
 *
 * Примечание: Пользователь должен убедиться, что соответствующие платформенные заголовки включены для сетевых операций.
 */

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <cstring>
#include <cstdint>
#include <queue>
#include "platform-specific.h"

// Message Type Constants
const uint8_t TEXT_MESSAGE = 0;
const uint8_t EVENT_MESSAGE = 1;
const uint8_t SNAPSHOT_MESSAGE = 2;
const uint8_t CLIENT_LIST_MESSAGE = 3;
const uint8_t CLIENT_ID_MESSAGE = 4; 

// Базовый класс сообщения
// Base class for messages
class BaseMessage {
public:
    uint8_t messageType; // Тип сообщения
    uint8_t senderID;    // Идентификатор отправителя

    // Конструктор, инициализирующий тип сообщения и идентификатор отправителя
    // Constructor that initializes the message type and sender ID
    BaseMessage(uint8_t type, uint8_t sender)
        : messageType(type), senderID(sender) {}

    // Статическая функция для сериализации сообщения в буфер
    // Static function to serialize a message into a buffer
    static void serializeMessage(BaseMessage* msg, std::vector<uint8_t>& buffer);

    // Статическая функция для десериализации сообщения из буфера
    // Static function to deserialize a message from a buffer
    static BaseMessage* deserializeMessage(const std::vector<uint8_t>& buffer);

    // Виртуальный деструктор
    // Virtual destructor
    virtual ~BaseMessage() {}
};

// Производный класс текстового сообщения
// Derived class for text messages
class TextMessage : public BaseMessage {
public:
    std::vector<uint8_t> text; // Текст сообщения в виде вектора байтов

    // Конструктор, принимающий идентификатор отправителя и текст сообщения
    // Constructor that takes sender ID and message text
    TextMessage(uint8_t sender, const std::string& msg)
        : BaseMessage(TEXT_MESSAGE, sender), text(msg.begin(), msg.end()) {}
};

// Производный класс сообщения события
// Derived class for event messages
class EventMessage : public BaseMessage {
public:
    std::queue<uint8_t> eventData; // Данные события в виде очереди

    // Конструктор, принимающий строку и преобразующий ее в очередь
    // Constructor that takes a string and converts it to a queue
    EventMessage(uint8_t sender, const std::string& data)
        : BaseMessage(EVENT_MESSAGE, sender) {
        for (char c : data) {
            eventData.push(static_cast<uint8_t>(c)); // Добавить каждый символ как uint8_t
            // Push each character as uint8_t
        }
    }

    // Конструктор, принимающий вектор и преобразующий его в очередь
    // Constructor that takes a vector and converts it to a queue
    EventMessage(uint8_t sender, const std::vector<uint8_t>& data)
        : BaseMessage(EVENT_MESSAGE, sender) {
        for (uint8_t byte : data) {
            eventData.push(byte); // Добавить каждый байт в очередь
            // Push each byte into the queue
        }
    }
};

// Производный класс сообщения снимка
// Derived class for snapshot messages
class SnapshotMessage : public BaseMessage {
public:
    std::queue<uint8_t> snapshotData; // Данные снимка в виде очереди

    // Конструктор по умолчанию
    // Default constructor
    SnapshotMessage() : BaseMessage(SNAPSHOT_MESSAGE, 0) {}

    // Параметризованный конструктор, принимающий строку
    // Parameterized constructor that takes a string
    SnapshotMessage(uint8_t sender, const std::string& data)
        : BaseMessage(SNAPSHOT_MESSAGE, sender) {
        for (char c : data) {
            snapshotData.push(static_cast<uint8_t>(c)); // Добавить каждый символ как uint8_t
            // Push each character as uint8_t
        }
    }

    // Параметризованный конструктор, принимающий вектор
    // Parameterized constructor that takes a vector
    SnapshotMessage(uint8_t sender, const std::vector<uint8_t>& data)
        : BaseMessage(SNAPSHOT_MESSAGE, sender) {
        for (uint8_t byte : data) {
            snapshotData.push(byte); // Добавить каждый байт в очередь
            // Push each byte into the queue
        }
    }
};

// Производный класс сообщения списка клиентов
// Derived class for client list messages
class ClientListMessage : public BaseMessage {
public:
    std::vector<uint8_t> clientIDs; // Идентификаторы клиентов в виде вектора

    // Конструктор, который инициализирует clientIDs напрямую
    // Constructor that initializes clientIDs directly
    ClientListMessage(uint8_t sender, std::vector<uint8_t> data)
        : BaseMessage(CLIENT_LIST_MESSAGE, sender), clientIDs(std::move(data)) {
        // Нет необходимости копировать элементы; данные перемещаются в clientIDs
        // No need to copy elements; data is moved into clientIDs
    }
};
// Производный класс сообщения идентификатора клиента
// Derived class for client ID messages
class ClientIDMessage : public BaseMessage {
public:
    uint8_t clientID; // Идентификатор клиента

    // Конструктор, принимающий идентификатор отправителя и идентификатор клиента
    // Constructor that takes sender ID and client ID
    ClientIDMessage(uint8_t sender, uint8_t clientID)
        : BaseMessage(CLIENT_ID_MESSAGE, sender), clientID(clientID) {}
};
