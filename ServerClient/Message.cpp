#include "Message.h"
#include "platform-specific.h"


// Deserialization Function
// Deserialization Function
// Функция десериализации
BaseMessage* BaseMessage::deserializeMessage(const std::vector<uint8_t>& buffer) {
    // Check if the buffer has at least 2 bytes (for message type and sender ID)
// Проверка, что буфер содержит как минимум 2 байта (для типа сообщения и идентификатора отправителя)
    if (buffer.size() < 2) return nullptr;

    uint8_t messageType = buffer[0]; // Get the message type from the buffer
    // Получить тип сообщения из буфера
    uint8_t senderID = buffer[1]; // Get the sender ID from the buffer
    // Получить идентификатор отправителя из буфера
    size_t offset = 2; // Initialize offset to 2 (after message type and sender ID)

    // Switch based on the message type
    // Переключение по типу сообщения
    switch (messageType) {
    case TEXT_MESSAGE: {
        // Check if there are enough bytes for the length of the text message
        // Проверка, что достаточно байтов для длины текстового сообщения
        if (buffer.size() < offset + 4) return nullptr;
        uint32_t length;
        memcpy(&length, &buffer[offset], 4); // Copy the length of the message
        length = ntohl(length); // Convert from network byte order to host byte order
        offset += 4; // Move the offset past the length

        // Check if there are enough bytes for the actual text
        // Проверка, что достаточно байтов для самого текста
        if (buffer.size() < offset + length) return nullptr;

        // Create a string from the text data in the buffer
        // Создать строку из текстовых данных в буфере
        std::string text(buffer.begin() + offset, buffer.begin() + offset + length);
        return new TextMessage(senderID, text); // Return a new TextMessage object
    }
    case EVENT_MESSAGE: {
        // Similar checks and operations for EVENT_MESSAGE
        // Аналогичные проверки и операции для EVENT_MESSAGE
        if (buffer.size() < offset + 4) return nullptr;
        uint32_t length;
        memcpy(&length, &buffer[offset], 4);
        length = ntohl(length);
        offset += 4;

        if (buffer.size() < offset + length) return nullptr;

        std::string data(buffer.begin() + offset, buffer.begin() + offset + length);
        return new EventMessage(senderID, data); // Return a new EventMessage object
    }
    case SNAPSHOT_MESSAGE: {
        // Similar checks and operations for SNAPSHOT_MESSAGE
        // Аналогичные проверки и операции для SNAPSHOT_MESSAGE
        if (buffer.size() < offset + 4) return nullptr;
        uint32_t length;
        memcpy(&length, &buffer[offset], 4);
        length = ntohl(length);
        offset += 4;

        if (buffer.size() < offset + length) return nullptr;

        std::string data(buffer.begin() + offset, buffer.begin() + offset + length);
        return new SnapshotMessage(senderID, data); // Return a new SnapshotMessage object
    }
    case CLIENT_LIST_MESSAGE: {
        // Similar checks and operations for CLIENT_LIST_MESSAGE
        // Аналогичные проверки и операции для CLIENT_LIST_MESSAGE
        if (buffer.size() < offset + 4) return nullptr;
        uint32_t length;
        memcpy(&length, &buffer[offset], 4);
        length = ntohl(length);
        offset += 4;

        if (buffer.size() < offset + length) return nullptr;

        // Read the client IDs into a vector
        // Считать идентификаторы клиентов в вектор
        std::vector<uint8_t> clientIDs(buffer.begin() + offset, buffer.begin() + offset + length);
        return new ClientListMessage(senderID, clientIDs); // Return a new ClientListMessage object
    }
    case CLIENT_ID_MESSAGE: {
        // Check if there is enough data for the client ID
        // Проверка, что достаточно данных для идентификатора клиента
        if (buffer.size() < offset + 1) return nullptr;
        uint8_t clientID = buffer[offset];  // Client ID is the next byte
        // Идентификатор клиента - следующий байт
        return new ClientIDMessage(senderID, clientID); // Return a new ClientIDMessage object
    }
    default:
        return nullptr; // Return nullptr for unknown message types
    }
}

// Serialization Function
// Функция сериализации
void BaseMessage::serializeMessage(BaseMessage* msg, std::vector<uint8_t>& buffer) {
    // Add the message type to the buffer
    // Добавляем тип сообщения в буфер
    buffer.push_back(msg->messageType);

    // Add the sender ID to the buffer
    // Добавляем ID отправителя в буфер
    buffer.push_back(msg->senderID);

    // Switch based on the message type to handle serialization accordingly
    // Переключаемся в зависимости от типа сообщения для соответствующей сериализации
    switch (msg->messageType) {
    case TEXT_MESSAGE: {
        // Handle serialization for text messages
        // Обрабатываем сериализацию для текстовых сообщений
        TextMessage* tm = static_cast<TextMessage*>(msg);
        uint32_t length = htonl(tm->text.size()); // Get the length of the text
        // Добавляем длину текста в буфер
        buffer.insert(buffer.end(), (uint8_t*)&length, (uint8_t*)&length + sizeof(length));
        // Add the text content to the buffer
        // Добавляем содержимое текста в буфер
        buffer.insert(buffer.end(), tm->text.begin(), tm->text.end());
        break;
    }
    case EVENT_MESSAGE: {
        // Handle serialization for event messages
        // Обрабатываем сериализацию для событийных сообщений
        EventMessage* em = static_cast<EventMessage*>(msg);
        uint32_t length = htonl(em->eventData.size()); // Get the length of the event data
        // Добавляем длину данных события в буфер
        buffer.insert(buffer.end(), (uint8_t*)&length, (uint8_t*)&length + sizeof(length));

        // Extract elements from the queue and insert them into the buffer
        // Извлекаем элементы из очереди и добавляем их в буфер
        std::queue<uint8_t> tempQueue = em->eventData; // Create a copy of the queue
        while (!tempQueue.empty()) {
            uint8_t byte = tempQueue.front(); // Get the front element
            // Insert it into the buffer
            // Вставляем его в буфер
            buffer.push_back(byte);
            tempQueue.pop(); // Remove the front element from the queue
        }
        break;
    }
    case SNAPSHOT_MESSAGE: {
        // Handle serialization for snapshot messages
        // Обрабатываем сериализацию для сообщений снимков
        SnapshotMessage* sm = static_cast<SnapshotMessage*>(msg);
        uint32_t length = htonl(sm->snapshotData.size()); // Get the length of the snapshot data
        // Добавляем длину данных снимка в буфер
        buffer.insert(buffer.end(), (uint8_t*)&length, (uint8_t*)&length + sizeof(length));

        // Extract elements from the queue and insert them into the buffer
        // Извлекаем элементы из очереди и добавляем их в буфер
        std::queue<uint8_t> tempQueue = sm->snapshotData; // Create a copy of the queue
        while (!tempQueue.empty()) {
            uint8_t byte = tempQueue.front(); // Get the front element
            // Insert it into the buffer
            // Вставляем его в буфер
            buffer.push_back(byte);
            tempQueue.pop(); // Remove the front element from the queue
        }
        break;
    }
    case CLIENT_LIST_MESSAGE: {
        // Handle serialization for client list messages
        // Обрабатываем сериализацию для сообщений списка клиентов
        ClientListMessage* clm = static_cast<ClientListMessage*>(msg);
        uint32_t length = htonl(clm->clientIDs.size()); // Get the length of the client IDs
        // Добавляем длину ID клиентов в буфер
        buffer.insert(buffer.end(), (uint8_t*)&length, (uint8_t*)&length + sizeof(length));
        // Add the client IDs to the buffer
        // Добавляем ID клиентов в буфер
        buffer.insert(buffer.end(), clm->clientIDs.begin(), clm->clientIDs.end());
        break;
    }
    case CLIENT_ID_MESSAGE: {
        // Handle serialization for client ID messages
        // Обрабатываем сериализацию для сообщений ID клиента
        ClientIDMessage* idMsg = static_cast<ClientIDMessage*>(msg);
        // Send client ID as a single byte
        // Отправляем ID клиента как один байт
        buffer.push_back(idMsg->clientID);
        break;
    }
    }
}
